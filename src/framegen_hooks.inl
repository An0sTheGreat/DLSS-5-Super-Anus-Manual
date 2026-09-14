// Shared pinned-hook setup and the validated native-SR return guard.
namespace boundary_probe {
std::atomic_uint hook_mask = 0;
#ifdef NR_NESTED_SOURCE_GUARD
DWORD evaluation_tls = TLS_OUT_OF_INDEXES;
std::atomic_bool evaluation_tracking = true;
struct EvaluationScope {
    EvaluationScope *previous;
    void *command;
    std::uint64_t feature;
    void *parameters;
    bool completed = false;
};
using VendorEvaluate = unsigned (*)(void *, std::uint64_t, void *, void *);
using EvaluateDispatch = unsigned (*)(VendorEvaluate, void *, std::uint64_t, void *, void *);
EvaluateDispatch original_evaluate = nullptr;
std::atomic_ullong duplicates_suppressed = 0;
EvaluationScope *evaluation_scope() {
    return !evaluation_tracking.load(std::memory_order_relaxed) || evaluation_tls == TLS_OUT_OF_INDEXES ? nullptr :
        static_cast<EvaluationScope *>(TlsGetValue(evaluation_tls));
}
bool reject_duplicate(unsigned source, bool retry) {
    auto *scope = evaluation_scope();
    if (source != 1 || retry || !scope || !scope->completed) return false;
    duplicates_suppressed.fetch_add(1,std::memory_order_relaxed);
    return true;
}
void complete_source(unsigned source, std::uint64_t result) {
    if (source == 1 && (result & 255) == 1)
        if (auto *scope = evaluation_scope()) scope->completed = true;
}
unsigned evaluate_dispatch(VendorEvaluate vendor, void *command, std::uint64_t feature, void *parameters, void *progress) {
    EvaluationScope scope{evaluation_scope(),command,feature,parameters};
    const bool bound = evaluation_tls != TLS_OUT_OF_INDEXES && TlsSetValue(evaluation_tls,&scope);
    if (!bound) evaluation_tracking.store(false,std::memory_order_relaxed); // Fail open, including children of an active scope.
    // Each child starts fresh: two real evaluations inside one wrapper are
    // allowed. Only their ancestor's redundant return callback is suppressed.
    const auto result = original_evaluate(vendor,command,feature,parameters,progress);
    if (bound) {
        TlsSetValue(evaluation_tls,scope.previous);
        if (scope.completed && command && feature && parameters)
            for (auto *ancestor = scope.previous; ancestor; ancestor = ancestor->previous)
                if (ancestor->command == command && ancestor->feature == feature && ancestor->parameters == parameters)
                    ancestor->completed = true;
    }
    return result;
}
#endif
bool pin_address(void *address) {
    HMODULE retained = nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(address),&retained) != FALSE;
}
bool install_hook(void *target, void *callback, void **original, const char *name, unsigned bit) {
    if (!target || !pin_address(target) || !pin_address(callback)) {
        log_message(reshade::log::level::warning,"NR boundary hook %s disabled: missing target/module ownership.",name); return false;
    }
    const auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        log_message(reshade::log::level::warning,"NR boundary hook %s disabled: MinHook init=%d.",name,static_cast<int>(init)); return false;
    }
    const auto created = MH_CreateHook(target,callback,original);
    if (created != MH_OK) {
        log_message(reshade::log::level::warning,"NR boundary hook %s disabled: creation=%d; existing hooks preserved.",name,static_cast<int>(created)); return false;
    }
    // Preserve callable trampolines even if activation fails. Never remove or
    // enable someone else's hooks, and never hold our locks across a vendor call.
    const auto enabled = MH_EnableHook(target);
    if (enabled == MH_OK) hook_mask.fetch_or(bit);
    log_message(reshade::log::level::info,"NR boundary hook %s: %s target=%p status=%d.",name,enabled == MH_OK ? "enabled" : "disabled",target,static_cast<int>(enabled));
    return enabled == MH_OK;
}
#ifdef NR_NESTED_SOURCE_GUARD
void install_evaluation_hook() {
    static std::atomic_flag attempted = ATOMIC_FLAG_INIT;
    if (g_target_module && !attempted.test_and_set()) {
        auto *target = reinterpret_cast<unsigned char *>(g_target_module)+0x4B140;
        constexpr unsigned char prefix[] = {0x55,0x56,0x48,0x81,0xec,0xa8,0,0,0};
        if (!std::memcmp(target,prefix,sizeof(prefix))) {
            evaluation_tls = TlsAlloc();
            if (evaluation_tls != TLS_OUT_OF_INDEXES)
                install_hook(target,reinterpret_cast<void *>(&evaluate_dispatch),
                    reinterpret_cast<void **>(&original_evaluate),"nested-source",64);
            else log_text(reshade::log::level::warning,"NR nested-source guard disabled: TLS unavailable; original admission preserved.");
        } else log_text(reshade::log::level::warning,"NR nested-source guard disabled: dispatcher profile mismatch; original admission preserved.");
    }
}
#endif
}
