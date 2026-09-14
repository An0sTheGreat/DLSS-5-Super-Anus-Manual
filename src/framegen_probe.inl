// FrameGen is observed only. Running NR inside the DLSSG callback can make
// Streamline stop submitting it after sustained multipass work. The native SR
// descriptor path below produces the same pre-FrameGen input without extending
// or mutating the vendor callback.
nr::AutoSourcePolicy g_auto_source;

#ifdef NR_FRAMEGEN_INPUT_TRACE
DWORD g_source_trace_tls = TLS_OUT_OF_INDEXES;
struct SourceTraceContext
{
    std::uint64_t feature, parameters;
    bool source_observed = false;
};

// The verified callback list is AFTER the vendor call. Preserve all arguments
// and its original handler; TLS correlates only nested native-SR descriptors.
extern "C" __declspec(dllexport) void observed_native_return(
    void *command, std::uint64_t feature, void *parameters, unsigned result)
{
    const auto original = reinterpret_cast<void (*)(void *, std::uint64_t, void *, unsigned)>(
        reinterpret_cast<std::uintptr_t>(g_target_module) + 0x9BD10);
    if (!g_frame_trace.recording(GetTickCount64()) || g_source_trace_tls == TLS_OUT_OF_INDEXES)
    {
        original(command, feature, parameters, result);
        return;
    }
    SourceTraceContext context{feature, reinterpret_cast<std::uint64_t>(parameters)};
    auto *previous = TlsGetValue(g_source_trace_tls);
    const bool bound = TlsSetValue(g_source_trace_tls, &context) != FALSE;
    original(command, feature, parameters, result);
    if (bound) TlsSetValue(g_source_trace_tls, previous);
    nr::FrameTraceEvent event;
    event.kind = nr::TraceKind::native_return;
    event.tick = GetTickCount64();
    event.thread = GetCurrentThreadId();
    event.command = reinterpret_cast<std::uint64_t>(command);
    event.feature = feature;
    event.parameters = reinterpret_cast<std::uint64_t>(parameters);
    event.result = result;
    event.source = context.source_observed ? 1u : 0u;
    g_frame_trace.push(event);
}
#endif

bool auto_source_enabled()
{
    return g_target_module && (field<unsigned char>(g_target_module,0x27100C)&1) != 0
        && field<unsigned char>(g_target_module,0x27100F) != 0;
}

std::uint64_t native_application_frame()
{
    return reinterpret_cast<std::uint64_t (*)()>(
        reinterpret_cast<std::uintptr_t>(g_target_module)+0xEC910)();
}

// Observe the validated 573B0 descriptor while its caller owns it. Record
// identities only: no COM retention, GPU work, or inference of GPU completion.
void trace_source_submission(void *descriptor, nr::TraceKind kind, std::uint64_t result = 0)
{
    const auto now = GetTickCount64();
    if (!descriptor || !g_frame_trace.recording(now)) return;
    nr::FrameTraceEvent event;
    event.kind = kind;
    event.tick = now;
    event.thread = GetCurrentThreadId();
    auto *command = field<reshade::api::command_list *>(descriptor, 0);
    event.command = command ? command->get_native() : 0;
#ifdef NR_FRAMEGEN_BOUNDARY_TRACE
    boundary_probe::watch_command(event.command);
#endif
    if (kind == nr::TraceKind::source_begin && command && TryEnterCriticalSection(&dx12::g_render_mutex))
    {
        if (auto *record = dx12::find_command_list_locked(command))
            record->trace_submission = g_frame_trace.capture_id();
        LeaveCriticalSection(&dx12::g_render_mutex);
    }
#ifdef NR_FRAMEGEN_INPUT_TRACE
    if (g_source_trace_tls != TLS_OUT_OF_INDEXES)
        if (auto *context = static_cast<SourceTraceContext *>(TlsGetValue(g_source_trace_tls)))
        {
            event.feature = context->feature;
            event.parameters = context->parameters;
            context->source_observed = true;
        }
#endif
    event.color = field<std::uint64_t>(descriptor, 0x10);
    event.source = field<unsigned char>(descriptor, 0x40);
    event.width = field<unsigned>(descriptor, 0x24);
    event.height = field<unsigned>(descriptor, 0x28);
    if (auto *inputs = field<void *>(descriptor, 0x30))
    {
        event.frame = field<std::uint64_t>(inputs, 0x68);
        event.motion = field<std::uint64_t>(inputs, 0x10);
        event.depth = field<std::uint64_t>(inputs, 0x18);
    }
    event.hook = static_cast<unsigned>(field<float>(g_target_module, 0x270FB0));
    event.pass = g_observed_pass_count.load();
    event.result = result;
    g_frame_trace.push(event);
}

extern "C" __declspec(dllexport) bool auto_native_source(
    unsigned original_method, std::uint64_t)
{
    if (!use_native_sr_source(original_method,nr_enabled())) return false;
    if (original_method == 3)
    {
        if (auto_source_enabled() && !g_auto_source.force_native()) return false;
        g_framegen_scaled_calls.fetch_add(1,std::memory_order_relaxed);
    }
    return true;
}

extern "C" __declspec(dllexport) std::uint64_t observed_framegen_callback(
    void *command, std::uint64_t feature, void *parameters)
{
    using Original = std::uint64_t (*)(void *,std::uint64_t,void *);
    const auto original = reinterpret_cast<Original>(
        reinterpret_cast<std::uintptr_t>(g_target_module)+0x9A4B0);
    if (!parameters || !command || !feature) return original(command,feature,parameters);

    const auto *vtable = *reinterpret_cast<std::uintptr_t **>(parameters);
    using GetResource = unsigned (*)(void *,const char *,ID3D12Resource **);
    const auto get_resource = reinterpret_cast<GetResource>(vtable[0x48/8]);
    ID3D12Resource *backbuffer = nullptr, *hudless = nullptr;
    get_resource(parameters,"DLSSG.Backbuffer",&backbuffer);
    get_resource(parameters,"DLSSG.HUDLess",&hudless);
    if (!backbuffer && !hudless) return original(command,feature,parameters);

#if defined(NR_FRAMEGEN_INPUT_TRACE) && !defined(NR_FRAMEGEN_BOUNDARY_TRACE)
    // One automatic capture, only once both FG and successful NR are observed.
    // Additional captures use the existing overlay button.
    static std::atomic_flag requested = ATOMIC_FLAG_INIT;
    if (g_successful_evaluations.load() != 0 && !requested.test_and_set())
        g_trace_requested.store(true);
#endif
    const auto now = GetTickCount64();
    if (g_frame_trace.recording(now))
    {
        nr::FrameTraceEvent event;
        event.kind = nr::TraceKind::framegen_entry;
        event.tick = now;
        event.thread = GetCurrentThreadId();
        // Application counter is context only, NOT a proven FG source frame.
        event.frame = native_application_frame();
        event.command = reinterpret_cast<std::uint64_t>(command);
        event.parameters = reinterpret_cast<std::uint64_t>(parameters);
        event.caller = reinterpret_cast<std::uint64_t>(_ReturnAddress());
        const auto base = reinterpret_cast<std::uint64_t>(g_target_module);
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(g_target_module);
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
        event.caller = event.caller >= base && event.caller < base + nt->OptionalHeader.SizeOfImage
            ? event.caller - base : 0; // Zero means outside this image, e.g. the fixture.
        // A vendor callback may receive ReShade's COM wrapper while the source
        // descriptor exposes the native list. Keep both identities to avoid
        // mistaking two wrappers of one command list for different queues.
        reshade::api::command_list *api = nullptr;
        UINT bytes = sizeof(api);
        const auto *guid = reinterpret_cast<const GUID *>(
            reinterpret_cast<std::uintptr_t>(g_target_module) + 0x22AA30);
        if (SUCCEEDED(static_cast<ID3D12GraphicsCommandList *>(command)->GetPrivateData(*guid, &bytes, &api)) &&
            bytes == sizeof(api) && api)
        {
            event.native_command = api->get_native();
            if (TryEnterCriticalSection(&dx12::g_render_mutex))
            {
                if (auto *record = dx12::find_command_list_locked(api))
                    record->trace_submission = g_frame_trace.capture_id();
                LeaveCriticalSection(&dx12::g_render_mutex);
            }
        }
#ifdef NR_FRAMEGEN_BOUNDARY_TRACE
        boundary_probe::watch_command(event.command);
        boundary_probe::watch_command(event.native_command);
#endif
        event.feature = feature;
        event.color = reinterpret_cast<std::uint64_t>(backbuffer);
        event.output = reinterpret_cast<std::uint64_t>(hudless);
        ID3D12Resource *motion = nullptr, *depth = nullptr;
        get_resource(parameters, "DLSSG.MVecs", &motion);
        get_resource(parameters, "DLSSG.Depth", &depth);
        event.motion = reinterpret_cast<std::uint64_t>(motion);
        event.depth = reinterpret_cast<std::uint64_t>(depth);
        using GetUnsigned = unsigned (*)(void *, const char *, unsigned *);
        unsigned index = UINT_MAX;
        if (reinterpret_cast<GetUnsigned>(vtable[0x60/8])(
                parameters, "DLSSG.MultiFrameIndex", &index) == 1) event.mfg_index = index;
        event.hook = static_cast<unsigned>(field<float>(g_target_module, 0x270FB0));
        event.pass = g_observed_pass_count.load();
        g_frame_trace.push(event);
    }

    // Returning success skips only RenoDX's registered NR handler. The game's
    // vendor DLSSG evaluation and all of its arguments remain untouched.
    g_framegen_transparent_bypass.fetch_add(1,std::memory_order_relaxed);
    return 1;
}
