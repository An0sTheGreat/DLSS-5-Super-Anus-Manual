// FrameGen is observed only. Running NR inside the DLSSG callback can make
// Streamline stop submitting it after sustained multipass work. The native SR
// descriptor path below produces the same pre-FrameGen input without extending
// or mutating the vendor callback.
nr::AutoSourcePolicy g_auto_source;

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

    // Returning success skips only RenoDX's registered NR handler. The game's
    // vendor DLSSG evaluation and all of its arguments remain untouched.
    g_framegen_transparent_bypass.fetch_add(1,std::memory_order_relaxed);
    return 1;
}
