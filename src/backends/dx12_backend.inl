// DX12-only resource, command-recording, feature lifetime and evaluation backend.
// Included once inside the addon namespace after shared settings/logging helpers.
// No DLL loads or GPU allocations until a verified DX12 callback/evaluation arrives.
namespace dx12
{
struct SourceBindings
{
    reshade::api::device *device = nullptr;
    reshade::api::resource source_color = {};
    reshade::api::resource source_output = {};
    reshade::api::resource source_motion = {};
    reshade::api::resource source_depth = {};
    reshade::api::resource source_ui = {};
    reshade::api::resource source_ui_alpha = {};
    reshade::api::resource_view source_color_srv = {}, source_output_uav = {};
    reshade::api::resource_view source_motion_srv = {}, source_depth_srv = {};
    reshade::api::resource_view source_ui_srv = {}, source_ui_alpha_srv = {};

    bool matches(reshade::api::resource color, reshade::api::resource output,
        reshade::api::resource motion, reshade::api::resource depth,
        reshade::api::resource ui, reshade::api::resource ui_alpha) const
    {
        return source_color == color && source_output == output && source_motion == motion &&
            source_depth == depth && source_ui == ui && source_ui_alpha == ui_alpha;
    }
};

struct ResourceSet : SourceBindings
{
    bool native_resolve_only = false;
    bool active = false;
    bool capture = false;
    bool pooled = false;
    ID3D12Fence *capture_fence = nullptr;
    std::uint64_t capture_fence_value = 0, capture_queue = 0;
    bool valid = false;
    bool inputs_are_shader_resources = false;
    // Immutable views for sequential passes; all retire with this working set.
    std::array<SourceBindings, 9> additional_sources = {};
    std::uint32_t display_width = 0;
    std::uint32_t display_height = 0;
    std::uint32_t work_width = 0;
    std::uint32_t work_height = 0;
    reshade::api::format color_format = reshade::api::format::unknown;
    reshade::api::format output_format = reshade::api::format::unknown;
    reshade::api::format motion_format = reshade::api::format::unknown;
    reshade::api::format depth_format = reshade::api::format::unknown;
    reshade::api::format ui_format = reshade::api::format::unknown;
    reshade::api::format ui_alpha_format = reshade::api::format::unknown;
    unsigned reset_generation = 0;
    unsigned allocation_generation = 0;
    std::uint64_t allocated_bytes = 0;
    ULONGLONG last_use = 0;
    bool retiring = false;
    bool unsafe_tracking = false;
    std::uint32_t queue_mask = 0;
    std::array<std::uint64_t, kMaximumQueues> retire_fences = {};
    bool native_feature = false;
    void *native_handle = nullptr;
    void *native_parameters = nullptr;

    reshade::api::resource work_color = {};
    reshade::api::resource native_color = {};
    reshade::api::resource_view native_color_srv = {}, native_color_uav = {};
    reshade::api::resource work_motion = {};
    reshade::api::resource work_depth = {};
    reshade::api::resource work_output = {};
    reshade::api::resource work_ui = {};
    reshade::api::resource work_ui_alpha = {};

    reshade::api::resource_view work_color_srv = {};
    reshade::api::resource_view work_motion_srv = {};
    reshade::api::resource_view work_depth_srv = {};
    reshade::api::resource_view work_color_uav = {};
    reshade::api::resource_view work_motion_uav = {};
    reshade::api::resource_view work_depth_uav = {};
    reshade::api::resource_view work_output_srv = {};
    reshade::api::resource_view work_output_uav = {};
    reshade::api::resource_view work_ui_srv = {};
    reshade::api::resource_view work_ui_uav = {};
    reshade::api::resource_view work_ui_alpha_srv = {};
    reshade::api::resource_view work_ui_alpha_uav = {};
};

struct TrackedCommandList
{
    bool active = false;
    std::uint64_t trace_submission = 0; // Capture ID only; never a lifetime reference.
    reshade::api::command_list *command_list = nullptr;
    reshade::api::device *device = nullptr;
    std::uint64_t native_command_list = 0;
    std::uint64_t native_device = 0;
    RecordingReferences references;
    unsigned group_set = UINT_MAX, next_pass = 0;
    std::uint64_t group_frame = 0;
    reshade::api::resource group_output = {};
};

struct TrackedQueue
{
    reshade::api::device *device = nullptr;
    ID3D12CommandQueue *native = nullptr;
    ID3D12Fence *fence = nullptr;
    std::uint64_t serial = 0;
};

CRITICAL_SECTION g_render_mutex = {};
bool g_render_mutex_initialized = false;
reshade::api::device *g_pipeline_device = nullptr;
reshade::api::pipeline_layout g_pipeline_layout = {};
reshade::api::pipeline g_pipeline = {};
reshade::api::pipeline_layout g_edge_pipeline_layout = {};
reshade::api::pipeline g_edge_pipeline = {};
constexpr std::uint32_t kEdgeResampleConstantCount = 24;
std::array<ResourceSet, kMaximumResourceSets> g_resource_sets = {};
std::array<TrackedCommandList, kMaximumTrackedCommandLists> g_tracked_command_lists = {};
std::array<TrackedQueue, kMaximumQueues> g_tracked_queues = {};
std::size_t g_command_slot_cursor = 0;
std::atomic_uint g_command_slots_recycled = 0, g_command_slots_exhausted = 0;
#ifdef NR_DAWNWALKER_NO_COPYBACK_TEST
std::atomic_uint g_no_copyback_log_generation = 0;
#endif
nr::ScaleHistory g_scale_history;
nr::MultipassGroupPolicy g_multipass_groups;
struct LocalMemoryAdapterCache
{
    reshade::api::device *device = nullptr;
    IDXGIAdapter3 *adapter = nullptr;
};
LocalMemoryAdapterCache g_local_memory_adapter;


void collect_resources_locked(ULONGLONG now, unsigned destruction_budget = UINT_MAX);
std::uint64_t invoke_native_evaluation(void *input, bool managed = false);

void log_fallback_once(unsigned generation, const char *reason)
{
    unsigned expected = g_logged_failure_generation.load(std::memory_order_relaxed);
    while (expected != generation)
    {
        if (g_logged_failure_generation.compare_exchange_weak(
                expected, generation, std::memory_order_relaxed))
        {
            log_message(reshade::log::level::warning,
                "RenoDX Neural Resolution: safe 100%% fallback: %s", reason);
            return;
        }
    }
}

TrackedCommandList *find_command_list_locked(reshade::api::command_list *intercepted)
{
    const std::uint64_t raw = reinterpret_cast<std::uint64_t>(intercepted);
    for (auto &tracked : g_tracked_command_lists)
        if (tracked.active &&
            (tracked.command_list == intercepted || tracked.native_command_list == raw))
            return &tracked;
    return nullptr;
}

void clear_device_tracking_locked(reshade::api::device *device)
{
    for (auto &tracked : g_tracked_command_lists)
        if (tracked.active && tracked.device == device)
            tracked = {};
}

reshade::api::format shader_view_format(reshade::api::format fmt)
{
    switch (fmt)
    {
    case reshade::api::format::d16_unorm:
    case reshade::api::format::r16_typeless:
        return reshade::api::format::r16_unorm;
    case reshade::api::format::d32_float:
    case reshade::api::format::r32_typeless:
        return reshade::api::format::r32_float;
    case reshade::api::format::d24_unorm_s8_uint:
    case reshade::api::format::r24_g8_typeless:
        return reshade::api::format::r24_unorm_x8_uint;
    case reshade::api::format::d32_float_s8_uint:
    case reshade::api::format::r32_g8_typeless:
        return reshade::api::format::r32_float_x8_uint;
    default:
        return reshade::api::format_to_default_typed(fmt, 0);
    }
}

reshade::api::format writable_format(reshade::api::format fmt)
{
    switch (shader_view_format(fmt))
    {
    case reshade::api::format::r24_unorm_x8_uint:
    case reshade::api::format::r32_float_x8_uint:
        return reshade::api::format::r32_float;
    default:
        return shader_view_format(fmt);
    }
}

void on_init_command_list(reshade::api::command_list *command_list);
// The verified 5D880 NGX wrapper receives a native ID3D12GraphicsCommandList,
// not an arbitrary ReShade object. The upstream init/destroy callbacks set and
// clear this private-data mapping. Recover it only while that command is live.
TrackedCommandList *resolve_evaluation_command_locked(void *native)
{
    auto *record = find_command_list_locked(static_cast<reshade::api::command_list *>(native));
    if (record || !native || !g_target_module) return record;
    reshade::api::command_list *api = nullptr;
    UINT bytes = sizeof(api);
    const auto *guid = reinterpret_cast<const GUID *>(reinterpret_cast<std::uintptr_t>(g_target_module)+0x22AA30);
    if (FAILED(static_cast<ID3D12GraphicsCommandList *>(native)->GetPrivateData(*guid, &bytes, &api)) ||
        bytes != sizeof(api) || !api || api->get_native() != reinterpret_cast<std::uint64_t>(native)) return nullptr;
    on_init_command_list(api);
    return find_command_list_locked(api);
}
#include "screenshot_capture.inl"

bool create_pipeline(reshade::api::device *device)
{
    if (g_pipeline.handle != 0 && g_edge_pipeline.handle != 0 && g_pipeline_device == device)
        return true;
    if (g_pipeline.handle != 0 || g_pipeline_layout.handle != 0 ||
        g_edge_pipeline.handle != 0 || g_edge_pipeline_layout.handle != 0)
        return false;

    const reshade::api::descriptor_range srv_range = {
        0, 0, 0, 1, reshade::api::shader_stage::all_compute, 1,
        reshade::api::descriptor_type::texture_shader_resource_view
    };
    const reshade::api::descriptor_range uav_range = {
        0, 0, 0, 1, reshade::api::shader_stage::all_compute, 1,
        reshade::api::descriptor_type::texture_unordered_access_view
    };
    const reshade::api::constant_range base_constants = {
        0, 0, 0, 15, reshade::api::shader_stage::all_compute
    };
    const reshade::api::constant_range edge_constants = {
        0, 0, 0, kEdgeResampleConstantCount, reshade::api::shader_stage::all_compute
    };
    auto input_range = srv_range; input_range.dx_register_index = 1;
    auto native_range = srv_range; native_range.dx_register_index = 2;
    auto depth_range = srv_range; depth_range.dx_register_index = 3;
    const reshade::api::pipeline_layout_param base_params[] = {
        reshade::api::pipeline_layout_param(1, &srv_range),
        reshade::api::pipeline_layout_param(1, &uav_range),
        reshade::api::pipeline_layout_param(base_constants),
        reshade::api::pipeline_layout_param(1, &input_range),
        reshade::api::pipeline_layout_param(1, &native_range),
    };
    const reshade::api::pipeline_layout_param edge_params[] = {
        reshade::api::pipeline_layout_param(1, &srv_range),
        reshade::api::pipeline_layout_param(1, &uav_range),
        reshade::api::pipeline_layout_param(edge_constants),
        reshade::api::pipeline_layout_param(1, &input_range),
        reshade::api::pipeline_layout_param(1, &native_range),
        reshade::api::pipeline_layout_param(1, &depth_range),
    };
    if (!device->create_pipeline_layout(static_cast<std::uint32_t>(std::size(base_params)),
            base_params, &g_pipeline_layout))
        return false;
    if (!device->create_pipeline_layout(static_cast<std::uint32_t>(std::size(edge_params)),
            edge_params, &g_edge_pipeline_layout))
    {
        device->destroy_pipeline_layout(g_pipeline_layout); g_pipeline_layout = {};
        return false;
    }

    reshade::api::shader_desc shader = {
        g_neural_resample_shader, g_neural_resample_shader_size, "Resample"
    };
    const reshade::api::pipeline_subobject subobject = {
        reshade::api::pipeline_subobject_type::compute_shader, 1, &shader
    };
    if (!device->create_pipeline(g_pipeline_layout, 1, &subobject, &g_pipeline))
    {
        device->destroy_pipeline_layout(g_pipeline_layout);
        device->destroy_pipeline_layout(g_edge_pipeline_layout);
        g_pipeline_layout = {};
        g_edge_pipeline_layout = {};
        return false;
    }
    reshade::api::shader_desc edge_shader = {
        g_neural_resample_edge_shader, g_neural_resample_edge_shader_size, "Resample"
    };
    const reshade::api::pipeline_subobject edge_subobject = {
        reshade::api::pipeline_subobject_type::compute_shader, 1, &edge_shader
    };
    if (!device->create_pipeline(g_edge_pipeline_layout, 1, &edge_subobject, &g_edge_pipeline))
    {
        device->destroy_pipeline(g_pipeline);
        device->destroy_pipeline_layout(g_pipeline_layout);
        device->destroy_pipeline_layout(g_edge_pipeline_layout);
        g_pipeline = {}; g_pipeline_layout = {}; g_edge_pipeline_layout = {};
        return false;
    }
    g_pipeline_device = device;
    return true;
}

bool create_texture_and_views(
    reshade::api::device *device,
    std::uint32_t width,
    std::uint32_t height,
    reshade::api::format format,
    reshade::api::resource &resource,
    reshade::api::resource_view &srv,
    reshade::api::resource_view &uav)
{
    if (format == reshade::api::format::unknown ||
        !device->check_format_support(format, reshade::api::resource_usage::shader_resource) ||
        !device->check_format_support(format, reshade::api::resource_usage::unordered_access))
        return false;
    const reshade::api::resource_desc desc(
        width, height, 1, 1, format, 1, reshade::api::memory_heap::gpu_only,
        reshade::api::resource_usage::shader_resource | reshade::api::resource_usage::unordered_access);
    if (!device->create_resource(desc, nullptr, reshade::api::resource_usage::unordered_access, &resource))
        return false;
    if (!device->create_resource_view(resource, reshade::api::resource_usage::shader_resource,
            reshade::api::resource_view_desc(format), &srv) ||
        !device->create_resource_view(resource, reshade::api::resource_usage::unordered_access,
            reshade::api::resource_view_desc(format), &uav))
        return false;
    return true;
}

void release_source_bindings(SourceBindings &set);

void destroy_resource_set(ResourceSet &set)
{
    if (!set.active || set.device == nullptr)
        return;
    if (set.capture) capture::discard(set);
    ResourceSet owned = set;
    set = {};
    auto *device = owned.device;
    if (owned.capture_fence) owned.capture_fence->Release();
    release_source_bindings(owned);
    for (auto &sources : owned.additional_sources) release_source_bindings(sources);
    for (const auto view : {
            owned.work_color_srv, owned.work_motion_srv, owned.work_depth_srv,
            owned.work_color_uav, owned.work_motion_uav, owned.work_depth_uav,
            owned.work_output_srv, owned.work_output_uav,
            owned.work_ui_srv, owned.work_ui_uav,
            owned.work_ui_alpha_srv, owned.work_ui_alpha_uav })
    {
        if (view.handle != 0)
            device->destroy_resource_view(view);
    }
    if (owned.native_color_srv.handle) device->destroy_resource_view(owned.native_color_srv);
    if (owned.native_color_uav.handle) device->destroy_resource_view(owned.native_color_uav);
    if (owned.native_color.handle) device->destroy_resource(owned.native_color);
    for (const auto resource : {
            owned.work_color, owned.work_motion, owned.work_depth, owned.work_output,
            owned.work_ui, owned.work_ui_alpha })
    {
        if (resource.handle != 0)
            device->destroy_resource(resource);
    }
}

void release_source_bindings(SourceBindings &set)
{
    if (set.device != nullptr)
        for (const auto view : {
                set.source_color_srv, set.source_output_uav,
                set.source_motion_srv, set.source_depth_srv,
                set.source_ui_srv, set.source_ui_alpha_srv })
            if (view.handle != 0)
                set.device->destroy_resource_view(view);
    set.source_color_srv = {};
    set.source_output_uav = {};
    set.source_motion_srv = {};
    set.source_depth_srv = {};
    set.source_ui_srv = {};
    set.source_ui_alpha_srv = {};
    set.source_color = {};
    set.source_output = {};
    set.source_motion = {};
    set.source_depth = {};
    set.source_ui = {};
    set.source_ui_alpha = {};
}

void pool_resource_set(ResourceSet &set, ULONGLONG now)
{
    release_source_bindings(set);
    for (auto &sources : set.additional_sources) release_source_bindings(sources);
    set.pooled = true;
    set.valid = true;
    set.retiring = false;
    set.unsafe_tracking = false;
    set.queue_mask = 0;
    set.retire_fences = {};
    set.last_use = now;
}

// Preserve this boundary for the test host's process-local DXGI pressure stub.
__declspec(noinline) bool query_local_memory(reshade::api::device *device, std::uint64_t &usage, std::uint64_t &budget)
{
    usage = budget = 0;
    if (device == nullptr) return false;
    if (g_local_memory_adapter.device != device)
    {
        if (g_local_memory_adapter.adapter != nullptr)
            g_local_memory_adapter.adapter->Release();
        g_local_memory_adapter = {};
        using CreateFactory = HRESULT (WINAPI *)(UINT, REFIID, void **);
        const auto create = reinterpret_cast<CreateFactory>(GetProcAddress(
            GetModuleHandleW(L"dxgi.dll"), "CreateDXGIFactory2"));
        if (create == nullptr) return false;
        IDXGIFactory4 *factory = nullptr;
        if (FAILED(create(0, __uuidof(IDXGIFactory4), reinterpret_cast<void **>(&factory))))
            return false;
        IDXGIAdapter3 *adapter = nullptr;
        const LUID luid = reinterpret_cast<ID3D12Device *>(device->get_native())->GetAdapterLuid();
        const HRESULT found = factory->EnumAdapterByLuid(
            luid, __uuidof(IDXGIAdapter3), reinterpret_cast<void **>(&adapter));
        factory->Release();
        if (FAILED(found)) return false;
        g_local_memory_adapter = {device, adapter};
    }
    DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
    if (FAILED(g_local_memory_adapter.adapter->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
        return false;
    usage = info.CurrentUsage;
    budget = info.Budget;
    g_local_memory_usage.store(usage, std::memory_order_relaxed);
    g_local_memory_budget.store(budget, std::memory_order_relaxed);
    return true;
}

std::uint64_t texture_allocation_size(reshade::api::device *device, std::uint32_t width,
                                     std::uint32_t height, reshade::api::format format)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = static_cast<DXGI_FORMAT>(format);
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const auto info = reinterpret_cast<ID3D12Device *>(device->get_native())->
        GetResourceAllocationInfo(0, 1, &desc);
    return info.SizeInBytes;
}

struct NativeSlotVector
{
    NativeFeatureSlot *begin;
    NativeFeatureSlot *end;
    NativeFeatureSlot *capacity;
    std::size_t count() const
    {
        return feature_slot_count(reinterpret_cast<std::uintptr_t>(begin),
            reinterpret_cast<std::uintptr_t>(end), reinterpret_cast<std::uintptr_t>(capacity));
    }
};

NativeSlotVector &native_slots(std::uintptr_t rva)
{
    return *reinterpret_cast<NativeSlotVector *>(reinterpret_cast<std::uintptr_t>(g_target_module) + rva);
}

NativeFeatureSlot native_primary_slot()
{
    NativeFeatureSlot slot;
    slot.parameters = field<void *>(g_target_module, 0x26D820);
    slot.handle = field<void *>(g_target_module, 0x26D828);
    slot.width = field<unsigned>(g_target_module, 0x26D878);
    slot.height = field<unsigned>(g_target_module, 0x26D87C);
    return slot;
}

bool host_owns_native_feature(const ResourceSet &set)
{
    const auto primary = native_primary_slot();
    if (primary.handle == set.native_handle && primary.parameters == set.native_parameters)
        return true;
    for (const auto rva : {0x26D8D0u, 0x26D8E8u})
    {
        const auto &slots = native_slots(rva);
        const auto count = slots.count();
        // Invalid bounds are already an unsafe hold in release_native_feature.
        if (count == SIZE_MAX) return true;
        for (std::size_t i = 0; i < count; ++i)
            if (slots.begin[i].handle == set.native_handle &&
                slots.begin[i].parameters == set.native_parameters)
                return true;
    }
    return false;
}

// All accesses to native slots occur under the host's NR mutex. Evaluation
// call sites already own it; maintenance acquires it BEFORE g_render_mutex.
bool release_native_feature(ResourceSet &set)
{
    NativeFeatureSlot *owner = nullptr;
    NativeSlotVector *retained_owner = nullptr;
    const auto primary = native_primary_slot();
    const bool primary_owner = primary.handle == set.native_handle &&
        primary.parameters == set.native_parameters;
    for (const auto rva : {0x26D8D0u, 0x26D8E8u})
    {
        auto &slots = native_slots(rva);
        const auto count = slots.count();
        if (count == SIZE_MAX) { set.unsafe_tracking = true; return false; }
        for (std::size_t i = 0; i < count; ++i)
            if (slots.begin[i].handle == set.native_handle &&
                slots.begin[i].parameters == set.native_parameters)
            {
                if (owner != nullptr || primary_owner) { set.unsafe_tracking = true; return false; }
                owner = slots.begin + i;
                if (rva == 0x26D8E8u) retained_owner = &slots;
            }
    }
    if (!primary_owner && owner == nullptr)
    {
        // The upstream owner already removed it (for example during teardown).
        // Do not call ReleaseFeature on a stale handle.
        return true;
    }
    using Release = unsigned (__cdecl *)(void *);
    const auto release = field<Release>(g_target_module, 0x26D710);
    const auto destroy_parameters = field<Release>(g_target_module, 0x26D778);
    if (release == nullptr || destroy_parameters == nullptr || release(set.native_handle) != 1)
    {
        set.unsafe_tracking = true;
        return false;
    }
    // Remove the now-invalid handle before it can be selected again. Neither
    // ReleaseFeature nor DestroyParameters is applied to game SR/RR/FG handles.
    if (primary_owner)
    {
        field<void *>(g_target_module, 0x26D820) = nullptr;
        field<void *>(g_target_module, 0x26D828) = nullptr;
        field<std::uint64_t>(g_target_module, 0x26D878) = 0;
        field<std::uint64_t>(g_target_module, 0x26D898) = 0;
        field<std::uint8_t>(g_target_module, 0x26D900) = 1;
        field<std::uint8_t>(g_target_module, 0x26D903) = 0;
    }
    else if (retained_owner != nullptr)
    {
        const auto remaining = static_cast<std::size_t>(retained_owner->end - owner - 1);
        std::memmove(owner, owner + 1, remaining * sizeof(NativeFeatureSlot));
        --retained_owner->end;
        *retained_owner->end = {};
    }
    else *owner = {};
    g_native_features_retired.fetch_add(1, std::memory_order_relaxed);
    if (destroy_parameters(set.native_parameters) != 1)
        log_text(reshade::log::level::warning,
            "NR V6.6: feature released but NGX parameter destruction failed; no retry on an ambiguous pointer.");
    return true;
}

std::uint64_t invoke_native_evaluation(void *input, bool managed)
{
    ScopedLock lock(g_render_mutex);
    const auto original = reinterpret_cast<EvaluateFunction>(
        reinterpret_cast<std::uintptr_t>(g_target_module) + kEvaluateWrapperRva);
    auto *record = resolve_evaluation_command_locked(field<void *>(input, 0));
    if (record != nullptr) record->references.recording();
#ifdef NR_PASS_INPUT_TRACE
    // Snapshot caller-owned scalars before the vendor wrapper. No resource
    // dereference/readback or parameter mutation; capture is manually bounded.
    nr::FrameTraceEvent temporal_trace;
    const bool temporal_recording = g_frame_trace.recording(GetTickCount64());
    if (temporal_recording)
    {
        std::memcpy(temporal_trace.temporal.data(), static_cast<const std::uint8_t *>(input) + 0x4C, 16);
        std::memcpy(temporal_trace.rects.data(), static_cast<const std::uint8_t *>(input) + 0x60, 64);
        temporal_trace.motion = field<std::uint64_t>(input, 0x20);
        temporal_trace.depth = field<std::uint64_t>(input, 0x28);
        temporal_trace.reset = field<std::uint8_t>(input, 0x5D);
        temporal_trace.hdr = field<std::uint8_t>(input, 0x5C);
        temporal_trace.host_reset = field<std::uint8_t>(g_target_module, 0x26D900);
        temporal_trace.managed = managed;
    }
#endif
    const auto result = original(input);
    if (record != nullptr)
    {
        const auto identity = reinterpret_cast<std::uintptr_t>(record->device);
        const auto pass = field<unsigned>(input, 8);
        auto *history = g_scale_history.find(identity, pass);
        // A continuously neutral/native first pass is not a new discontinuity
        // every frame. Only leaving managed output (or failure) breaks history.
        if ((result & 255) != 1 || (!managed && history && history->generation != 0))
            g_scale_history.invalidate_from(identity, pass);
    }
    if ((result & 255) == 1) g_successful_evaluations.fetch_add(1, std::memory_order_relaxed);
    if (record != nullptr)
        g_evaluation_device.observe(reinterpret_cast<std::uintptr_t>(record->device), result);
    const auto now = GetTickCount64();
    if (g_frame_trace.recording(now)
#ifdef NR_PASS_INPUT_TRACE
        && temporal_recording
#endif
        )
    {
        nr::FrameTraceEvent event
#ifdef NR_PASS_INPUT_TRACE
            = temporal_trace
#endif
            ;
        event.kind = nr::TraceKind::evaluation;
        event.tick = now;
        event.thread = GetCurrentThreadId();
        event.command = reinterpret_cast<std::uint64_t>(field<void *>(input, 0));
        event.color = reinterpret_cast<std::uint64_t>(field<void *>(input, 0x10));
        event.output = reinterpret_cast<std::uint64_t>(field<void *>(input, 0x18));
        event.width = field<unsigned>(input, 0x44);
        event.height = field<unsigned>(input, 0x48);
        event.pass = field<unsigned>(input, 8);
        event.frame = framegen_transition_frame();
        event.mfg_index = framegen_transition_index();
        event.result = result;
        g_frame_trace.push(event);
    }
    const unsigned pass = field<unsigned>(input, 8);
    NativeFeatureSlot slot;
    if (pass == 0) slot = native_primary_slot();
    else
    {
        const auto &slots = native_slots(0x26D8D0);
        const auto count = slots.count();
        if (count == SIZE_MAX || pass > count) return result;
        slot = slots.begin[pass - 1];
    }
    if (slot.handle == nullptr || slot.parameters == nullptr) return result;
    ResourceSet *feature = nullptr;
    for (auto &set : g_resource_sets)
        if (set.active && set.native_feature && set.native_handle == slot.handle &&
            set.native_parameters == slot.parameters) { feature = &set; break; }
    if (feature == nullptr)
        for (auto &set : g_resource_sets)
            if (!set.active)
            {
                feature = &set;
                set.active = true;
                set.valid = true;
                set.native_feature = true;
                set.native_handle = slot.handle;
                set.native_parameters = slot.parameters;
                break;
            }
    if (feature == nullptr) return result; // Host retains ownership if registry is full.
    feature->retiring = false; // A still-live cached handle may be reused while fences drain.
    feature->retire_fences = {};
    feature->last_use = GetTickCount64();
    feature->allocation_generation = g_scale_generation.load();
    if (record == nullptr || !g_lifetime_events_registered) feature->unsafe_tracking = true;
    else
    {
        feature->device = record->device;
        record->references.sets |= 1ull << (feature - g_resource_sets.data());
    }
    return result;
}

void on_reset_recording(reshade::api::command_list *cmd)
{
    if (cmd == nullptr || !nr::backends::handles(cmd->get_device())) return;
    ScopedLock lock(g_render_mutex);
    capture::reset_command(cmd);
    if (auto *record = find_command_list_locked(cmd))
    {
        record->trace_submission = 0;
        record->references.resetting();
        cmd->set_private_data(kPendingRecordingGuid, 1);
    }
}

void on_recording_pipeline(reshade::api::command_list *cmd, reshade::api::pipeline_stage,
                           reshade::api::pipeline)
{
    if (cmd == nullptr || !nr::backends::handles(cmd->get_device())) return;
    // Pipeline binds are frequent. Only the first bind following Reset takes
    // our render lock; normal binds do not contend with NR recording threads.
    std::uint64_t pending = 0;
    cmd->get_private_data(kPendingRecordingGuid, &pending);
    if (pending == 0) return;
    ScopedLock lock(g_render_mutex);
    // A valid pipeline bind after Reset belongs to the new recording. Merely
    // receiving the PRE-Reset notification is deliberately insufficient.
    if (auto *record = find_command_list_locked(cmd)) record->references.recording();
    cmd->set_private_data(kPendingRecordingGuid, 0);
}

void on_secondary_recording(reshade::api::command_list *primary, reshade::api::command_list *secondary)
{
    if (primary == nullptr || secondary == nullptr ||
        !nr::backends::handles(primary->get_device()) ||
        primary->get_device() != secondary->get_device()) return;
    ScopedLock lock(g_render_mutex);
    auto *child = find_command_list_locked(secondary);
    if (child == nullptr) return;
    if (auto *parent = find_command_list_locked(primary))
    {
        parent->references.recording();
        parent->references.sets |= child->references.sets;
        if (child->trace_submission == g_frame_trace.capture_id())
            parent->trace_submission = child->trace_submission;
    }
    else
        for (std::size_t i = 0; i < g_resource_sets.size(); ++i)
            if ((child->references.sets & (1ull << i)) != 0)
                g_resource_sets[i].unsafe_tracking = true;
}

void on_execute_recording(reshade::api::command_queue *queue, reshade::api::command_list *cmd)
{
    if (queue == nullptr || cmd == nullptr || !nr::backends::handles(queue->get_device()) ||
        queue->get_device() != cmd->get_device()) return;
    ScopedLock lock(g_render_mutex);
    auto *record = find_command_list_locked(cmd);
    if (record == nullptr) return;
    const auto now = GetTickCount64();
    if (g_frame_trace.take_submission(&record->trace_submission, now))
    {
        nr::FrameTraceEvent event;
        event.kind = nr::TraceKind::queue_submit;
        event.tick = now;
        event.thread = GetCurrentThreadId();
        event.command = cmd->get_native();
        event.queue = queue->get_native();
        g_frame_trace.push(event);
    }
    if (record->references.sets == 0) return;
    auto *native = reinterpret_cast<ID3D12CommandQueue *>(queue->get_native());
    std::size_t index = g_tracked_queues.size();
    for (std::size_t i = 0; i < g_tracked_queues.size(); ++i)
        if (g_tracked_queues[i].native == native) { index = i; break; }
    if (index == g_tracked_queues.size() && native != nullptr)
        for (std::size_t i = 0; i < g_tracked_queues.size(); ++i)
        {
            auto &tracked = g_tracked_queues[i];
            if (tracked.native != nullptr) continue;
            ID3D12Fence *fence = nullptr;
            auto *device = reinterpret_cast<ID3D12Device *>(record->device->get_native());
            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                    __uuidof(ID3D12Fence), reinterpret_cast<void **>(&fence)))) break;
            native->AddRef();
            tracked = {record->device, native, fence, 0};
            index = i;
            break;
        }
    for (std::size_t i = 0; i < g_resource_sets.size(); ++i)
    {
        if ((record->references.sets & (1ull << i)) == 0) continue;
        auto &set = g_resource_sets[i];
        if (index == g_tracked_queues.size()) set.unsafe_tracking = true;
        else set.queue_mask |= 1u << index;
    }
    // This event precedes ExecuteCommandLists. Signaling HERE would not prove
    // that this submission completed. Signals are deferred until all recordings
    // that could submit these resources have been replaced or destroyed.
}

void collect_resources_locked(ULONGLONG now, unsigned destruction_budget)
{
    unsigned destructive_actions = 0;
    std::uint64_t recording_mask = 0;
    for (const auto &record : g_tracked_command_lists)
        if (record.active) recording_mask |= record.references.sets;
    unsigned active = 0, pinned = 0, retiring = 0, unsafe = 0, features = 0, pooled = 0;
    std::uint64_t bytes = 0;
    const bool bridge_backed_dx11 =
        static_cast<reshade::api::device_api>(g_runtime_api.load()) == reshade::api::device_api::d3d11 &&
        g_evaluation_device.observed();
    const bool active_nr = nr_enabled();
    const bool keep_working_sets = active_nr && (bridge_backed_dx11 ||
        nr::uses_scaled_path(g_scale_percent.load()) ||
        nr::uses_base_resolve(g_transfer_percent.load(), g_color_percent.load(), g_sharpness_percent.load()) ||
        g_observed_pass_count.load(std::memory_order_relaxed) > 1);
    for (std::size_t i = 0; i < g_resource_sets.size(); ++i)
    {
        auto &set = g_resource_sets[i];
        if (!set.active) continue;
        // Pooled sets own only private working textures. Their previous source
        // descriptors have already drained behind real queue fences and can be
        // rebound without reallocating the large textures.
        if (set.pooled)
        {
            if (pooled_retirement_candidate(keep_working_sets,
                    set.allocation_generation == g_scale_generation.load(),
                    bridge_backed_dx11, set.last_use, now))
            {
                if (destructive_actions < destruction_budget)
                {
                    ++destructive_actions;
                    destroy_resource_set(set);
                    g_retired_sets.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
            }
            ++active;
            ++pooled;
            bytes += set.allocated_bytes;
            continue;
        }
        // The second final-screen image has not yet been recorded. Keep the
        // one bounded pair alive until completion or its 500 ms deadline.
        if (set.capture && capture::hold_final_pair(now)) {
            ++active; bytes += set.allocated_bytes; continue;
        }
        // Final-screen copies use ReShade's immediate command list. These do
        // not produce application recording/reset events. The dedicated fence
        // is signaled by queue->signal AFTER it submits that immediate list.
        if (set.capture_fence) {
            if (!set.unsafe_tracking && set.capture_fence_value &&
                fence_completed(set.capture_fence->GetCompletedValue(),set.capture_fence_value) &&
                destructive_actions < destruction_budget) {
                ++destructive_actions;
                capture::finish(set);
                destroy_resource_set(set);
                g_retired_sets.fetch_add(1);
                continue;
            }
            ++active; ++pinned; bytes += set.allocated_bytes;
            if (set.unsafe_tracking) ++unsafe;
            continue;
        }
        const bool referenced = (recording_mask & (1ull << i)) != 0;
        // No observed queue means completion cannot be established. This also
        // deliberately holds discarded-only recordings rather than assuming an
        // unobserved native submission never happened.
        if (!referenced && set.queue_mask == 0) set.unsafe_tracking = true;
        // The host keeps reduced pass slots available for later reuse. Releasing
        // one during a live DX11 bridge session can deadlock inside NVIDIA NGX.
        const bool keep_native_feature = set.native_feature && set.valid && active_nr &&
            bridge_backed_dx11 && host_owns_native_feature(set);
        if (keep_native_feature)
        {
            set.retiring = false;
            set.retire_fences = {};
        }
        // A replaced working set can begin real-fence retirement immediately.
        // Recording references still protect PRE-Reset and unsubmitted work;
        // live bridge-owned native features are held by the guard above.
        const bool recyclable_working_set = !set.native_feature && set.queue_mask != 0;
        if (!keep_native_feature && !set.retiring && !referenced && !set.unsafe_tracking &&
            (recyclable_working_set ||
                retirement_candidate(set.valid, active_nr, set.native_feature ? 99 : g_scale_percent.load(),
                    set.allocation_generation, g_scale_generation.load(), set.last_use, now)))
        {
            set.retiring = true;
            for (std::size_t q = 0; q < g_tracked_queues.size(); ++q)
            {
                if ((set.queue_mask & (1u << q)) == 0) continue;
                auto &queue = g_tracked_queues[q];
                if (queue.native == nullptr || queue.fence == nullptr ||
                    FAILED(queue.native->Signal(queue.fence, ++queue.serial)))
                {
                    set.unsafe_tracking = true;
                    break;
                }
                set.retire_fences[q] = queue.serial;
            }
        }
        if (set.retiring && !set.unsafe_tracking && !referenced)
        {
            bool complete = true;
            for (std::size_t q = 0; q < g_tracked_queues.size(); ++q)
                if (set.retire_fences[q] != 0 &&
                    !fence_completed(g_tracked_queues[q].fence->GetCompletedValue(), set.retire_fences[q]))
                    complete = false;
            if (complete)
            {
                if (set.native_feature)
                {
                    if (destructive_actions < destruction_budget && release_native_feature(set))
                    {
                        ++destructive_actions;
                        set = {};
                        g_retired_sets.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                }
                else
                {
                    if (set.capture && destructive_actions < destruction_budget)
                    {
                        ++destructive_actions;
                        capture::finish(set);
                        destroy_resource_set(set);
                        g_retired_sets.fetch_add(1, std::memory_order_relaxed);
                    }
                    else if (keep_working_sets &&
                        set.allocation_generation == g_scale_generation.load())
                    {
                        pool_resource_set(set, now);
                        ++active;
                        ++pooled;
                        bytes += set.allocated_bytes;
                    }
                    else if (!set.capture && destructive_actions < destruction_budget)
                    {
                        ++destructive_actions;
                        destroy_resource_set(set);
                        g_retired_sets.fetch_add(1, std::memory_order_relaxed);
                    }
                    if (!set.active || set.pooled) continue;
                }
            }
        }
        ++active;
        bytes += set.allocated_bytes;
        if (referenced) ++pinned;
        if (set.retiring) ++retiring;
        if (set.unsafe_tracking) ++unsafe;
        if (set.native_feature) ++features;
    }
    g_cached_sets.store(active);
    g_cached_mib.store(static_cast<unsigned>((bytes + (1u << 20) - 1) >> 20));
    g_pinned_sets.store(pinned);
    g_retiring_sets.store(retiring);
    g_unsafe_sets.store(unsafe);
    g_native_features.store(features);
    g_pooled_sets.store(pooled);
}

bool configuration_epoch_ready_locked(unsigned generation, ULONGLONG now)
{
    if (g_quiesce_generation.load(std::memory_order_acquire) != generation) return true;
    collect_resources_locked(now);
    for (const auto &set : g_resource_sets)
        if (set.active && !set.capture && set.allocation_generation != generation)
            return false;
    unsigned expected = generation;
    if (g_quiesce_generation.compare_exchange_strong(expected, 0, std::memory_order_acq_rel))
        g_multipass_groups.reset();
    return true;
}

bool try_lock_native_nr()
{
    // Verified MSVC mutex layout in this exact host image: SRW at +0x10,
    // owner thread at +0x48, count at +0x4c. Try directly rather than calling
    // _Mtx_lock from a Present/overlay callback that may own a queue lock.
    auto *mutex = reinterpret_cast<std::uint8_t *>(g_target_module) + 0x26D650;
    auto *srw = reinterpret_cast<PSRWLOCK>(mutex + 0x10);
    if (!TryAcquireSRWLockExclusive(srw)) return false;
    if (field<unsigned>(mutex, 0x4C) != 0)
    {
        ReleaseSRWLockExclusive(srw);
        return false;
    }
    field<DWORD>(mutex, 0x48) = GetCurrentThreadId();
    field<unsigned>(mutex, 0x4C) = 1;
    return true;
}

void unlock_native_nr()
{
    auto *mutex = reinterpret_cast<std::uint8_t *>(g_target_module) + 0x26D650;
    field<unsigned>(mutex, 0x4C) = 0;
    field<DWORD>(mutex, 0x48) = MAXDWORD;
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(mutex + 0x10));
}

#if defined(NR_EXPERIMENTAL_DX11) || defined(NR_LIFETIME_TEST)
// Only a quiesced backend may request this. It does not manufacture completion,
// erase recording pins, touch untracked host output caches, or destroy a device.
// Returning complete means OUR tracked sets drained, not that NGX can shut down.
bool retire_device_resources(reshade::api::device *device, unsigned &remaining)
{
    remaining = UINT_MAX;
    if (!device || !try_lock_native_nr()) return false;
    struct UnlockNative { ~UnlockNative() { unlock_native_nr(); } } native_lock;
    if (!TryEnterCriticalSection(&g_render_mutex)) return false;
    struct UnlockRender { ~UnlockRender() { LeaveCriticalSection(&g_render_mutex); } } render_lock;
    for (auto &set : g_resource_sets)
        if (set.active && set.device == device) set.valid = false;
    // Keep the production fence/recording/error policy. Other devices receive
    // ordinary maintenance only; their settings and validity are not changed.
    collect_resources_locked(GetTickCount64());
    remaining = 0;
    for (const auto &set : g_resource_sets)
        if (set.active && set.device == device) ++remaining;
    return remaining == 0;
}
#include "consumer_session.inl"

// Private backend only: caller has stopped submissions, destroyed transport
// recordings, detached NR and shut down its core. Never call this for a game
// device. Validate everything before detaching; release outside our lock so
// ReShade destruction callbacks cannot observe stale ownership or deadlock.
bool close_quiescent_private_caches(reshade::api::device *device)
{
    if (!device || !TryEnterCriticalSection(&g_render_mutex)) return false;
    for (const auto &set : g_resource_sets)
        if (set.active && set.device == device)
        { LeaveCriticalSection(&g_render_mutex); return false; }
    for (const auto &cmd : g_tracked_command_lists)
        if (cmd.active && cmd.device == device)
        { LeaveCriticalSection(&g_render_mutex); return false; }
    for (const auto &tracked : g_tracked_queues)
        if (tracked.device == device)
        {
            const auto completed = tracked.fence ? tracked.fence->GetCompletedValue() : UINT64_MAX;
            if (!tracked.native || completed == UINT64_MAX || completed < tracked.serial)
            { LeaveCriticalSection(&g_render_mutex); return false; }
        }
    std::array<TrackedQueue, kMaximumQueues> owned = {};
    for (unsigned i = 0; i < kMaximumQueues; ++i)
        if (g_tracked_queues[i].device == device)
        { owned[i] = g_tracked_queues[i]; g_tracked_queues[i] = {}; }
    reshade::api::pipeline pipeline = {};
    reshade::api::pipeline_layout layout = {};
    reshade::api::pipeline edge_pipeline = {};
    reshade::api::pipeline_layout edge_layout = {};
    reshade::api::pipeline capture_pipeline = {};
    reshade::api::pipeline_layout capture_layout = {};
    if (capture::pipeline_device == device)
    {
        capture_pipeline = capture::copy_pipeline; capture_layout = capture::copy_layout;
        capture::copy_pipeline = {}; capture::copy_layout = {}; capture::pipeline_device = nullptr;
    }
    if (g_pipeline_device == device)
    {
        pipeline = g_pipeline; layout = g_pipeline_layout;
        edge_pipeline = g_edge_pipeline; edge_layout = g_edge_pipeline_layout;
        g_pipeline = {}; g_pipeline_layout = {};
        g_edge_pipeline = {}; g_edge_pipeline_layout = {}; g_pipeline_device = nullptr;
    }
#ifdef NR_EXPERIMENTAL_DX11
    g_scale_history.forget(reinterpret_cast<std::uintptr_t>(device));
#endif
    g_evaluation_device.forget(reinterpret_cast<std::uintptr_t>(device));
    LeaveCriticalSection(&g_render_mutex);
    if (pipeline.handle) device->destroy_pipeline(pipeline);
    if (layout.handle) device->destroy_pipeline_layout(layout);
    if (edge_pipeline.handle) device->destroy_pipeline(edge_pipeline);
    if (edge_layout.handle) device->destroy_pipeline_layout(edge_layout);
    if (capture_pipeline.handle) device->destroy_pipeline(capture_pipeline);
    if (capture_layout.handle) device->destroy_pipeline_layout(capture_layout);
    for (const auto &tracked : owned)
    {
        if (tracked.fence) tracked.fence->Release();
        if (tracked.native) tracked.native->Release();
    }
    return true;
}
#endif

void maintain_resources()
{
    const ULONGLONG now = GetTickCount64();
    static ULONGLONG next_collect = 0;
    if (now < next_collect) return;
    next_collect = now + 250;
    // The verified host call sites lock this mutex before entering our evaluate
    // wrappers. Match their lock order; never take it while holding our mutex.
    if (!try_lock_native_nr()) return;
    struct UnlockNative
    {
        ~UnlockNative() { unlock_native_nr(); }
    } native_lock;
    if (!TryEnterCriticalSection(&g_render_mutex)) return;
    struct UnlockRender
    {
        ~UnlockRender() { LeaveCriticalSection(&g_render_mutex); }
    } render_lock;
    collect_resources_locked(now, 1);
}

bool create_output_view(
    reshade::api::device *device,
    reshade::api::resource resource,
    reshade::api::format format,
    reshade::api::resource_view &view)
{
    const auto view_format = writable_format(format);
    return view_format != reshade::api::format::unknown &&
        device->check_format_support(view_format, reshade::api::resource_usage::unordered_access) &&
        device->create_resource_view(resource, reshade::api::resource_usage::unordered_access,
            reshade::api::resource_view_desc(view_format), &view);
}

bool create_source_view(
    reshade::api::device *device,
    reshade::api::resource resource,
    reshade::api::format format,
    reshade::api::resource_view &view)
{
    const auto view_format = shader_view_format(format);
    if (view_format == reshade::api::format::unknown ||
        !device->check_format_support(view_format, reshade::api::resource_usage::shader_resource))
        return false;
    return device->create_resource_view(
        resource, reshade::api::resource_usage::shader_resource,
        reshade::api::resource_view_desc(view_format), &view);
}

bool bind_source_resources(
    SourceBindings &set,
    reshade::api::resource color,
    reshade::api::resource output,
    reshade::api::resource motion,
    reshade::api::resource depth,
    reshade::api::resource ui,
    reshade::api::resource ui_alpha,
    const reshade::api::resource_desc &color_desc,
    const reshade::api::resource_desc &output_desc,
    const reshade::api::resource_desc &motion_desc,
    const reshade::api::resource_desc &depth_desc,
    const reshade::api::resource_desc &ui_desc,
    const reshade::api::resource_desc &ui_alpha_desc)
{
    reshade::api::resource_view color_srv = {}, output_uav = {};
    reshade::api::resource_view motion_srv = {}, depth_srv = {};
    reshade::api::resource_view ui_srv = {}, ui_alpha_srv = {};
    const bool ok =
        create_source_view(set.device, color, color_desc.texture.format, color_srv) &&
        create_output_view(set.device, output, output_desc.texture.format, output_uav) &&
        create_source_view(set.device, motion, motion_desc.texture.format, motion_srv) &&
        create_source_view(set.device, depth, depth_desc.texture.format, depth_srv) &&
        (!ui.handle || create_source_view(set.device, ui, ui_desc.texture.format, ui_srv)) &&
        (!ui_alpha.handle || create_source_view(set.device, ui_alpha,
            ui_alpha_desc.texture.format, ui_alpha_srv));
    if (!ok)
    {
        for (const auto view : {color_srv, output_uav, motion_srv, depth_srv, ui_srv, ui_alpha_srv})
            if (view.handle)
                set.device->destroy_resource_view(view);
        return false;
    }
    set.source_color = color;
    set.source_output = output;
    set.source_motion = motion;
    set.source_depth = depth;
    set.source_ui = ui;
    set.source_ui_alpha = ui_alpha;
    set.source_color_srv = color_srv;
    set.source_output_uav = output_uav;
    set.source_motion_srv = motion_srv;
    set.source_depth_srv = depth_srv;
    set.source_ui_srv = ui_srv;
    set.source_ui_alpha_srv = ui_alpha_srv;
    return true;
}

bool valid_source_texture(
    const reshade::api::resource_desc &desc,
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t width,
    std::uint32_t height)
{
    return desc.type == reshade::api::resource_type::texture_2d &&
        desc.texture.samples == 1 && width != 0 && height != 0 &&
        x <= desc.texture.width && y <= desc.texture.height &&
        width <= desc.texture.width - x && height <= desc.texture.height - y;
}

ResourceSet *find_or_create_resource_set(
    reshade::api::device *device,
    reshade::api::resource color,
    reshade::api::resource output,
    reshade::api::resource motion,
    reshade::api::resource depth,
    reshade::api::resource ui,
    reshade::api::resource ui_alpha,
    const reshade::api::resource_desc &color_desc,
    const reshade::api::resource_desc &output_desc,
    const reshade::api::resource_desc &motion_desc,
    const reshade::api::resource_desc &depth_desc,
    const reshade::api::resource_desc &ui_desc,
    const reshade::api::resource_desc &ui_alpha_desc,
    std::uint32_t display_width,
    std::uint32_t display_height,
    std::uint32_t work_width,
    std::uint32_t work_height,
    bool force_new = false, bool native_resolve_only = false, bool allow_new = true)
{
    if (color_desc.type != reshade::api::resource_type::texture_2d ||
        output_desc.type != reshade::api::resource_type::texture_2d ||
        motion_desc.type != reshade::api::resource_type::texture_2d ||
        depth_desc.type != reshade::api::resource_type::texture_2d ||
        color_desc.texture.samples != 1 || output_desc.texture.samples != 1 ||
        motion_desc.texture.samples != 1 || depth_desc.texture.samples != 1 ||
        (ui.handle && (ui_desc.type != reshade::api::resource_type::texture_2d || ui_desc.texture.samples != 1)) ||
        (ui_alpha.handle && (ui_alpha_desc.type != reshade::api::resource_type::texture_2d || ui_alpha_desc.texture.samples != 1)))
        return nullptr;

    const auto color_format = writable_format(color_desc.texture.format);
    const auto output_format = writable_format(output_desc.texture.format);
    const auto motion_format = writable_format(motion_desc.texture.format);
    const auto depth_format = writable_format(depth_desc.texture.format);
    const auto ui_format = ui.handle ? writable_format(ui_desc.texture.format) : reshade::api::format::unknown;
    const auto ui_alpha_format = ui_alpha.handle ? writable_format(ui_alpha_desc.texture.format) : reshade::api::format::unknown;
    if (color_format == reshade::api::format::unknown ||
        output_format == reshade::api::format::unknown ||
        motion_format == reshade::api::format::unknown ||
        depth_format == reshade::api::format::unknown ||
        (ui.handle && ui_format == reshade::api::format::unknown) ||
        (ui_alpha.handle && ui_alpha_format == reshade::api::format::unknown))
        return nullptr;

    const unsigned generation = g_scale_generation.load();
    const auto compatible_working_set = [&](const ResourceSet &set)
    {
        return set.active && !set.capture && !set.native_feature && set.device == device &&
            set.native_resolve_only == native_resolve_only &&
            set.allocation_generation == generation &&
            set.display_width == display_width && set.display_height == display_height &&
            set.work_width == work_width && set.work_height == work_height &&
            set.color_format == color_format && set.output_format == output_format &&
            set.motion_format == motion_format && (native_resolve_only ||
            (set.depth_format == depth_format && set.ui_format == ui_format && set.ui_alpha_format == ui_alpha_format));
    };

    if (!force_new)
    for (auto &set : g_resource_sets)
    {
        if (compatible_working_set(set) && set.valid && !set.pooled && !set.retiring &&
            set.source_color == color && set.source_output == output &&
            set.source_motion == motion && set.source_depth == depth &&
            set.source_ui == ui && set.source_ui_alpha == ui_alpha)
        {
            set.last_use = GetTickCount64();
            return &set;
        }
    }

    const auto now = GetTickCount64();
    collect_resources_locked(now);
    // Rebind only a set whose previous command recordings and real queue fences
    // have fully drained. The private working textures and their state survive;
    // only the lightweight descriptors for the new game resources are replaced.
    if (!force_new)
    for (auto &set : g_resource_sets)
        if (set.pooled && compatible_working_set(set) &&
            bind_source_resources(set, color, output, motion, depth, ui, ui_alpha,
                color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc))
        {
            set.pooled = false;
            set.valid = true;
            set.last_use = now;
            g_rebound_sets.fetch_add(1, std::memory_order_relaxed);
            return &set;
        }

    // Backoff forbids fresh allocation, never fence-safe reuse of our cache.
    if (!allow_new) return nullptr;

    // Allow up to three complete in-flight multipass groups while the real
    // queue fences drain. Memory admission remains the harder upper bound.
    const unsigned maximum_sets = nr::maximum_working_sets(
        field<unsigned>(g_target_module,0x266FA4));
    unsigned stream_sets = 0;
    for (const auto &set : g_resource_sets)
        if (compatible_working_set(set)) ++stream_sets;
    if (stream_sets >= maximum_sets)
    {
        g_budget_fallbacks.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    std::uint64_t requested_bytes = 0, used_bytes = 0;
    for (const auto &set : g_resource_sets)
        if (set.active) used_bytes += set.allocated_bytes;
    for (const auto format : {
            native_resolve_only ? reshade::api::format::unknown : color_format,
            output_format, motion_format,
            native_resolve_only ? reshade::api::format::unknown : depth_format,
            native_resolve_only ? reshade::api::format::unknown : ui_format,
            native_resolve_only ? reshade::api::format::unknown : ui_alpha_format})
    {
        if (format == reshade::api::format::unknown) continue;
        const auto size = texture_allocation_size(device, work_width, work_height, format);
        if (size == UINT64_MAX || size > kWorkingTextureBudget) return nullptr;
        requested_bytes += size;
    }
    const auto native_bytes = texture_allocation_size(device, display_width, display_height,
        writable_format(color_desc.texture.format));
    if (native_bytes == UINT64_MAX || native_bytes > kWorkingTextureBudget) return nullptr;
    requested_bytes += native_bytes;
    // Incompatible pooled shapes are already fence-safe. Evict them before
    // rejecting a current frame for budget pressure.
    std::uint64_t local_usage = 0, local_budget = 0;
    auto admission = nr::MemoryAdmission{};
    if (query_local_memory(device, local_usage, local_budget))
        admission = nr::adaptive_memory_admission(used_bytes, local_usage, local_budget,
            field<unsigned>(g_target_module,0x266FA4));
    const std::uint64_t working_budget = admission.queried ? admission.cache_limit : kWorkingTextureBudget;
    g_adaptive_cache_limit.store(working_budget, std::memory_order_relaxed);
    if (!allocation_fits(used_bytes, requested_bytes, working_budget))
        for (auto &candidate : g_resource_sets)
        {
            if (!candidate.pooled || compatible_working_set(candidate)) continue;
            const auto released = candidate.allocated_bytes;
            destroy_resource_set(candidate);
            g_retired_sets.fetch_add(1, std::memory_order_relaxed);
            used_bytes = released <= used_bytes ? used_bytes - released : 0;
            if (allocation_fits(used_bytes, requested_bytes, working_budget)) break;
        }
    if (!allocation_fits(used_bytes, requested_bytes, working_budget))
    {
        g_budget_fallbacks.fetch_add(1, std::memory_order_relaxed);
        if (field<unsigned>(g_target_module,0x266FA4) > 1)
            g_multipass_vram_failure_generation.store(
                g_stream_generation.load(std::memory_order_relaxed), std::memory_order_release);
        log_message(reshade::log::level::warning,
            "NR admission rejected: layout=%s requested=%llu KiB cached=%llu KiB limit=%llu KiB usage=%llu MiB budget=%llu MiB reserve=%llu MiB.",
            native_resolve_only ? "native-controls" : "scaled",
            requested_bytes >> 10, used_bytes >> 10, working_budget >> 10,
            local_usage >> 20, local_budget >> 20, admission.reserved_headroom >> 20);
        return nullptr;
    }

    auto iterator = std::find_if(g_resource_sets.begin(), g_resource_sets.end(),
        [](const ResourceSet &set) { return !set.active; });
    if (iterator == g_resource_sets.end())
        return nullptr;

    ResourceSet &set = *iterator;
    set.native_resolve_only = native_resolve_only;
    set.active = true;
    set.valid = true;
    set.device = device;
    set.display_width = display_width;
    set.display_height = display_height;
    set.work_width = work_width;
    set.work_height = work_height;
    set.color_format = color_format;
    set.output_format = output_format;
    set.motion_format = motion_format;
    set.depth_format = depth_format;
    set.ui_format = ui_format;
    set.ui_alpha_format = ui_alpha_format;
    set.allocated_bytes = requested_bytes;
    set.allocation_generation = generation;
    set.last_use = now;

    if (!create_texture_and_views(device, display_width, display_height, color_format,
            set.native_color, set.native_color_srv, set.native_color_uav) ||
        (!native_resolve_only && !create_texture_and_views(device, work_width, work_height, color_format,
            set.work_color, set.work_color_srv, set.work_color_uav)) ||
        !create_texture_and_views(device, work_width, work_height, motion_format,
            set.work_motion, set.work_motion_srv, set.work_motion_uav) ||
        (!native_resolve_only && !create_texture_and_views(device, work_width, work_height, depth_format,
            set.work_depth, set.work_depth_srv, set.work_depth_uav)) ||
        !create_texture_and_views(device, work_width, work_height, output_format,
            set.work_output, set.work_output_srv, set.work_output_uav))
        goto fail;

    if (!native_resolve_only && ui.handle != 0)
    {
        if (!create_texture_and_views(device, work_width, work_height, ui_format,
                set.work_ui, set.work_ui_srv, set.work_ui_uav))
            goto fail;
    }
    if (!native_resolve_only && ui_alpha.handle != 0)
    {
        if (!create_texture_and_views(device, work_width, work_height, ui_alpha_format,
                set.work_ui_alpha, set.work_ui_alpha_srv, set.work_ui_alpha_uav))
            goto fail;
    }
    if (!bind_source_resources(set, color, output, motion, depth, ui, ui_alpha,
            color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc))
        goto fail;

    return &set;

fail:
    destroy_resource_set(set);
    return nullptr;
}

void dispatch_resample(
    reshade::api::command_list *cmd_list,
    reshade::api::resource_view source,
    reshade::api::resource_view destination,
    std::uint32_t source_x,
    std::uint32_t source_y,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t destination_width,
    std::uint32_t destination_height,
    std::uint32_t filter_mode = 0,
    float sharpness = 0.0f,
    reshade::api::resource_view small_input = {},
    reshade::api::resource_view native_color = {},
    std::uint32_t destination_x = 0, std::uint32_t destination_y = 0,
    float transfer = 1.0f, float color_strength = 1.0f,
    float detail = 1.0f, float coupling = 0.0f, float edge_protection = 0.0f,
    reshade::api::resource_view depth_input = {},
    std::uint32_t depth_x = 0, std::uint32_t depth_y = 0,
    std::uint32_t depth_width = 0, std::uint32_t depth_height = 0,
    float edge_thickness = 1.0f, float edge_shift = 0.0f, float edge_softness = 0.0f,
    std::uint32_t depth_inverted = 0)
{
    const reshade::api::descriptor_table_update source_update = {
        {}, 0, 0, 1, reshade::api::descriptor_type::texture_shader_resource_view, &source
    };
    const reshade::api::descriptor_table_update destination_update = {
        {}, 0, 0, 1, reshade::api::descriptor_type::texture_unordered_access_view, &destination
    };
    const bool depth_available = depth_input.handle && depth_width && depth_height;
    const std::uint32_t constants[] = {
        source_x, source_y, source_width, source_height,
        destination_width, destination_height,
        filter_mode, std::bit_cast<std::uint32_t>(sharpness), destination_x, destination_y,
        std::bit_cast<std::uint32_t>(transfer), std::bit_cast<std::uint32_t>(color_strength),
        std::bit_cast<std::uint32_t>(detail), std::bit_cast<std::uint32_t>(coupling),
        std::bit_cast<std::uint32_t>(edge_protection),
        depth_available ? 1u : 0u, depth_x, depth_y, depth_width, depth_height,
        std::bit_cast<std::uint32_t>(edge_thickness), std::bit_cast<std::uint32_t>(edge_shift),
        std::bit_cast<std::uint32_t>(edge_softness),
        depth_inverted
    };
    static_assert(std::size(constants) == kEdgeResampleConstantCount);

    const auto pipeline = depth_available ? g_edge_pipeline : g_pipeline;
    const auto layout = depth_available ? g_edge_pipeline_layout : g_pipeline_layout;
    cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_compute, pipeline);
    cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, layout, 0, source_update);
    cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, layout, 1, destination_update);
    cmd_list->push_constants(reshade::api::shader_stage::all_compute, layout, 2, 0,
        depth_available ? kEdgeResampleConstantCount : 15, constants);
    // Bind valid descriptors even in branches which do not read these textures.
    if (!small_input.handle) small_input = source;
    if (!native_color.handle) native_color = source;
    auto extra_update = source_update;
    extra_update.descriptors = &small_input;
    cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, layout, 3, extra_update);
    extra_update.descriptors = &native_color;
    cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, layout, 4, extra_update);
    if (depth_available)
    {
        extra_update.descriptors = &depth_input;
        cmd_list->push_descriptors(reshade::api::shader_stage::all_compute, layout, 5, extra_update);
    }
    cmd_list->dispatch((destination_width + 7) / 8, (destination_height + 7) / 8, 1);
}

std::uint64_t __fastcall scaled_evaluate_body(void *input, unsigned call_site)
{
    auto original = [](void *value) { return invoke_native_evaluation(value); };
    if (input == nullptr) return 0;
    ScopedLock lock(g_render_mutex);
    const auto capture_deadline = g_capture_off_until.load();
    if (capture_deadline && GetTickCount64() < capture_deadline) {
        g_capture_skipped.fetch_add(1);
        return 0; // Existing caller failure path leaves the original image intact.
    }
    if (!nr_enabled())
    {
        g_off_evaluation_calls.fetch_add(1, std::memory_order_relaxed);
        return 0; // The caller's existing failure path preserves the game image.
    }
    g_evaluation_calls.fetch_add(1, std::memory_order_relaxed);
    const int scale_percent = g_scale_percent.load(std::memory_order_relaxed);
    const auto multipass_motion_mode = g_multipass_motion_mode.load(std::memory_order_relaxed);
    const unsigned evaluation_pass = field<unsigned>(input, 8);
    const bool detail_working_path = nr::uses_base_resolve(
        g_transfer_percent.load(), g_color_percent.load(), g_sharpness_percent.load());
    auto controls = nr::resolve_pass(g_pass_controls, evaluation_pass,
        {false, g_resolve_mode.load(), g_transfer_percent.load(), g_color_percent.load(), g_sharpness_percent.load()});
    // Native resolution starts from the direct NR result, but still applies
    // the independent transfer, colour and sharpness controls when requested.
    if (scale_percent == 100 && evaluation_pass == 0) {
        controls.mode = 0;
    }
    if (!g_lifetime_events_registered)
    {
        g_effective_scale.store(100, std::memory_order_relaxed);
        return original(input);
    }

    const unsigned allocation_generation = g_scale_generation.load(std::memory_order_relaxed);
    const unsigned generation = g_stream_generation.load(std::memory_order_relaxed);
    // Reuse is always allowed once real fences/recordings retire. New compact
    // allocations back off for 250 ms after failure, without blocking reuse.
    static unsigned controls_retry_generation = 0;
    static ULONGLONG controls_retry_after = 0;
    static bool controls_recovery_pending = false;
    static unsigned native_controls_generation = 0, controls_drain_generation = 0;
    const bool allow_control_allocation = controls_retry_generation != generation ||
        GetTickCount64() >= controls_retry_after;
    const auto framegen_frame = framegen_transition_frame();
    const bool framegen_route = framegen_frame != 0;
    const unsigned hook_method = static_cast<unsigned>(field<float>(g_target_module, 0x270FB0));
    const bool route_transition = framegen_route ?
        transition_uses_native(generation, framegen_frame) :
        (hook_method >= 3 ? transition_uses_native_pass(generation, evaluation_pass,
            field<unsigned>(g_target_module, 0x266FA4)) :
            transition_uses_native(generation, g_native_last_frame.load(std::memory_order_relaxed)));
    if (route_transition)
    {
        g_transition_native_calls.fetch_add(1, std::memory_order_relaxed);
        return original(input);
    }
    // Even a neutral first pass must advance the complete-group transition.
    // Skipping it leaves later passes permanently waiting for its mask bit.
    if (!nr::uses_evaluation_working_path(scale_percent, evaluation_pass) && !detail_working_path)
    {
        g_effective_scale.store(100, std::memory_order_relaxed);
        return original(input);
    }
    auto &site_generation = call_site == 1 ?
        g_logged_create_site_generation : g_logged_existing_site_generation;
    const bool trace_this_call =
        site_generation.exchange(generation, std::memory_order_relaxed) != generation;
    if (trace_this_call)
        log_message(reshade::log::level::info,
            "RenoDX Neural Resolution: [1/9 hook] entered scaled hook site=%u (%s); input=%p.",
            call_site, call_site == 1 ? "create/evaluate" : "existing-handle evaluate", input);
    auto native_fallback = [&](const char *reason) {
        g_scale_fallback_calls.fetch_add(1, std::memory_order_relaxed);
        log_fallback_once(generation, reason);
        g_effective_scale.store(100, std::memory_order_relaxed);
        return original(input);
    };

    auto *cmd_list = reinterpret_cast<reshade::api::command_list *>(field<void *>(input, 0x00));
    const auto color = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x10)) };
    const auto output = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x18)) };
    const auto motion = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x20)) };
    const auto depth = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x28)) };
    const auto ui = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x30)) };
    const auto ui_alpha = reshade::api::resource {
        reinterpret_cast<std::uint64_t>(field<void *>(input, 0x38)) };
    const std::uint32_t display_width = field<std::uint32_t>(input, 0x44);
    const std::uint32_t display_height = field<std::uint32_t>(input, 0x48);
    if (trace_this_call)
        log_message(reshade::log::level::info,
            "RenoDX Neural Resolution: [2/9 raw] command_list=%p color=0x%llx output=0x%llx motion=0x%llx depth=0x%llx extent=%ux%u.",
            static_cast<void *>(cmd_list),
            static_cast<unsigned long long>(color.handle),
            static_cast<unsigned long long>(output.handle),
            static_cast<unsigned long long>(motion.handle),
            static_cast<unsigned long long>(depth.handle),
            display_width, display_height);
    if (color.handle == 0 || output.handle == 0 || motion.handle == 0 || depth.handle == 0 ||
        display_width == 0 || display_height == 0 ||
        display_width > 16384 || display_height > 16384)
        return native_fallback("the intercepted resources or display extent are invalid");

    const std::uint32_t color_x = field<std::uint32_t>(input, 0x60);
    const std::uint32_t color_y = field<std::uint32_t>(input, 0x64);
    const std::uint32_t color_width_value = field<std::uint32_t>(input, 0x68);
    const std::uint32_t color_height_value = field<std::uint32_t>(input, 0x6C);
    const std::uint32_t color_width = color_width_value != 0 ? color_width_value : display_width;
    const std::uint32_t color_height = color_height_value != 0 ? color_height_value : display_height;
    const auto output_x = field<std::uint32_t>(input, 0x70);
    const auto output_y = field<std::uint32_t>(input, 0x74);
    // A 1:1 native anchor cannot be promised for mismatched input/output extents.
    // Preserve the original NR route rather than silently resizing the base image.
    if (color_width != display_width || color_height != display_height ||
        field<unsigned>(input, 0x40) != static_cast<unsigned>(reshade::api::resource_usage::unordered_access))
        return native_fallback("native anchor extent or output state is unsupported");
    const std::uint32_t motion_x = field<std::uint32_t>(input, 0x80);
    const std::uint32_t motion_y = field<std::uint32_t>(input, 0x84);
    const std::uint32_t motion_width_value = field<std::uint32_t>(input, 0x88);
    const std::uint32_t motion_height_value = field<std::uint32_t>(input, 0x8C);
    const std::uint32_t motion_width = motion_width_value != 0 ? motion_width_value : display_width;
    const std::uint32_t motion_height = motion_height_value != 0 ? motion_height_value : display_height;
    const std::uint32_t depth_x = field<std::uint32_t>(input, 0x90);
    const std::uint32_t depth_y = field<std::uint32_t>(input, 0x94);
    const std::uint32_t depth_width_value = field<std::uint32_t>(input, 0x98);
    const std::uint32_t depth_height_value = field<std::uint32_t>(input, 0x9C);
    const std::uint32_t depth_width = depth_width_value != 0 ? depth_width_value : display_width;
    const std::uint32_t depth_height = depth_height_value != 0 ? depth_height_value : display_height;

    TrackedCommandList *command_list_record = resolve_evaluation_command_locked(cmd_list);
    if (command_list_record == nullptr)
        return native_fallback("the intercepted command list did not match a ReShade wrapper or native D3D12 handle");
    const bool matched_native_command_list = cmd_list != command_list_record->command_list;
    reshade::api::device *device = command_list_record->device;
    cmd_list = command_list_record->command_list;
    command_list_record->references.recording();
    if (!configuration_epoch_ready_locked(allocation_generation, GetTickCount64()))
    {
        g_quiesce_native_calls.fetch_add(1, std::memory_order_relaxed);
        g_effective_scale.store(100, std::memory_order_relaxed);
        return original(input);
    }
    if (trace_this_call)
        log_message(reshade::log::level::info,
            "RenoDX Neural Resolution: [3/9 command] matched %s command list; querying input descriptions through its verified ReShade device.",
            matched_native_command_list ? "native" : "ReShade wrapper");
    const auto color_desc = device->get_resource_desc(color);
    const auto output_desc = device->get_resource_desc(output);
    const auto motion_desc = device->get_resource_desc(motion);
    const auto depth_desc = device->get_resource_desc(depth);
    const auto ui_desc = ui.handle != 0 ? device->get_resource_desc(ui) : reshade::api::resource_desc{};
    const auto ui_alpha_desc = ui_alpha.handle != 0 ? device->get_resource_desc(ui_alpha) : reshade::api::resource_desc{};
    if (!valid_source_texture(color_desc, color_x, color_y, color_width, color_height) ||
        !valid_source_texture(output_desc, output_x, output_y, display_width, display_height) ||
        !valid_source_texture(motion_desc, motion_x, motion_y, motion_width, motion_height) ||
        !valid_source_texture(depth_desc, depth_x, depth_y, depth_width, depth_height) ||
        (ui.handle != 0 && !valid_source_texture(ui_desc, 0, 0, display_width, display_height)) ||
        (ui_alpha.handle != 0 && !valid_source_texture(ui_alpha_desc, 0, 0, display_width, display_height)))
        return native_fallback("an input texture or subrect is unsupported");

    // Keep dimensions even: feature 18 and FP16 paths are most reliable on 2-pixel alignment.
    bool native_resolve_only = scale_percent == 100 || g_multipass_groups.blocked(generation) ||
        native_controls_generation == generation;
    std::uint32_t work_width = native_resolve_only ? display_width : nr::scaled_extent(display_width, scale_percent);
    std::uint32_t work_height = native_resolve_only ? display_height : nr::scaled_extent(display_height, scale_percent);
    if (work_width > nr::maximum_texture_extent || work_height > nr::maximum_texture_extent)
        return native_fallback("the requested internal Neural Rendering extent exceeds the D3D12 texture limit");
    if (work_width == display_width && work_height == display_height &&
        nr::uses_scaled_path(scale_percent) && !native_resolve_only)
        return original(input);

    const unsigned pass_count = std::clamp(field<unsigned>(g_target_module,0x266FA4),1u,10u);
    const unsigned native_passes = nr::uses_scaled_path(scale_percent) || detail_working_path ? 0u : 1u;
    const unsigned working_pass = evaluation_pass - native_passes;
    const unsigned working_pass_count = std::max(1u, pass_count - native_passes);
    // Identical colour/output formats allow the existing snapshot/resample/
    // resolve sequence to reuse one scratch set across ordered passes. Each
    // recording still pins the entire set and its immutable source views.
    const bool share_working_set = writable_format(color_desc.texture.format) ==
        writable_format(output_desc.texture.format);
    const auto previous_group_set = command_list_record->group_set;
    const bool continuing_group = working_pass != 0 &&
        command_list_record->next_pass == evaluation_pass &&
        command_list_record->group_frame == (framegen_route ? framegen_frame : g_native_last_frame.load()) &&
        command_list_record->group_output == color;
    command_list_record->group_set = UINT_MAX; // Every failure/native return ends the lease.
    const std::uint64_t group_token = framegen_route ? framegen_frame :
        g_native_last_frame.load(std::memory_order_relaxed);
    bool group_pressure = false;
    if (working_pass == 0 && working_pass_count > 1 && !share_working_set && !native_resolve_only)
    {
        // Complete pending fence retirement BEFORE counting reusable capacity.
        // Otherwise a full cache can reject the group without ever reaching
        // the allocator's collector; only the 250 ms maintenance tick recovers it.
        collect_resources_locked(GetTickCount64());
        const auto color_format = writable_format(color_desc.texture.format);
        const auto output_format = writable_format(output_desc.texture.format);
        const auto motion_format = writable_format(motion_desc.texture.format);
        const auto depth_format = writable_format(depth_desc.texture.format);
        const auto ui_format = ui.handle ? writable_format(ui_desc.texture.format) : reshade::api::format::unknown;
        const auto ui_alpha_format = ui_alpha.handle ? writable_format(ui_alpha_desc.texture.format) : reshade::api::format::unknown;
        unsigned compatible = 0;
        for (const auto &candidate : g_resource_sets)
        {
            if (!candidate.active) continue;
            if (!candidate.capture && !candidate.native_feature && candidate.valid &&
                candidate.device == device && candidate.allocation_generation == allocation_generation &&
                candidate.display_width == display_width && candidate.display_height == display_height &&
                candidate.work_width == work_width && candidate.work_height == work_height &&
                candidate.color_format == color_format && candidate.output_format == output_format &&
                candidate.motion_format == motion_format && candidate.depth_format == depth_format &&
                candidate.ui_format == ui_format && candidate.ui_alpha_format == ui_alpha_format &&
                nr::prewarm_slot_available(candidate.pooled, candidate.retiring,
                    candidate.source_color == color && candidate.source_output == output &&
                    candidate.source_motion == motion && candidate.source_depth == depth &&
                    candidate.source_ui == ui && candidate.source_ui_alpha == ui_alpha))
                ++compatible;
        }
        const unsigned missing = working_pass_count > compatible ? working_pass_count - compatible : 0;
        // The allocator owns admission, including eviction of incompatible
        // fence-safe pooled shapes. No scaled GPU work starts until all pass
        // slots exist; a partial prewarm remains safely pooled on failure.
        for (unsigned i = 0; i < missing; ++i)
        {
            auto *reserved = find_or_create_resource_set(
                device, color, output, motion, depth, ui, ui_alpha,
                color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc,
                display_width, display_height, work_width, work_height, true);
            if (reserved == nullptr) { group_pressure = true; break; }
            pool_resource_set(*reserved, GetTickCount64());
            g_prewarmed_sets.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const bool native_group = g_multipass_groups.use_native(
        generation, group_token, working_pass, working_pass_count, group_pressure);
    if (native_group)
    {
        native_controls_generation = generation;
        if (group_pressure)
        {
            g_budget_fallbacks.fetch_add(1, std::memory_order_relaxed);
            log_fallback_once(generation,
                "the complete multipass group could not be reserved after fence collection; holding 100% until resolution, pass count, preset or hook changes");
        }
        g_memory_native_groups.fetch_add(evaluation_pass == 0 ? 1u : 0u, std::memory_order_relaxed);
        native_resolve_only = true;
        work_width = display_width;
        work_height = display_height;
    }

    if (trace_this_call)
    {
        log_message(reshade::log::level::info,
            "RenoDX Neural Resolution: [4/9 input] generation=%u, scale=%d%%, native=%ux%u, internal=%ux%u, formats=%u/%u/%u.",
            generation, scale_percent, display_width, display_height, work_width, work_height,
            static_cast<unsigned>(color_desc.texture.format),
            static_cast<unsigned>(motion_desc.texture.format),
            static_cast<unsigned>(depth_desc.texture.format));
    }

    if (!create_pipeline(device))
        return native_fallback("compute pipeline creation failed");
    auto *history = g_scale_history.find(reinterpret_cast<std::uintptr_t>(device), evaluation_pass);
    if (!history) return native_fallback("scale-history registry is full");
    if (trace_this_call)
        log_text(reshade::log::level::info,
            "RenoDX Neural Resolution: [5/9 pipeline] compute pipeline is ready.");
    ResourceSet *set = nullptr;
    SourceBindings *sources = nullptr;
    if (share_working_set && continuing_group && previous_group_set < g_resource_sets.size())
    {
        auto &candidate = g_resource_sets[previous_group_set];
        if (candidate.active && candidate.valid && !candidate.pooled && !candidate.retiring &&
            !candidate.capture && !candidate.native_feature && candidate.device == device &&
            candidate.native_resolve_only == native_resolve_only &&
            (command_list_record->references.sets & (1ull << previous_group_set)) != 0 &&
            candidate.allocation_generation == allocation_generation &&
            candidate.display_width == display_width && candidate.display_height == display_height &&
            candidate.work_width == work_width && candidate.work_height == work_height &&
            candidate.color_format == writable_format(color_desc.texture.format) &&
            candidate.output_format == writable_format(output_desc.texture.format) &&
            candidate.motion_format == writable_format(motion_desc.texture.format) &&
            (native_resolve_only || (candidate.depth_format == writable_format(depth_desc.texture.format) &&
            candidate.ui_format == (ui.handle ? writable_format(ui_desc.texture.format) : reshade::api::format::unknown) &&
            candidate.ui_alpha_format == (ui_alpha.handle ? writable_format(ui_alpha_desc.texture.format) : reshade::api::format::unknown))))
        {
            for (auto &binding : candidate.additional_sources)
                if (binding.matches(color, output, motion, depth, ui, ui_alpha)) { sources = &binding; break; }
            if (!sources) for (auto &binding : candidate.additional_sources)
                if (!binding.source_color.handle)
                {
                    binding.device = device;
                    if (bind_source_resources(binding, color, output, motion, depth, ui, ui_alpha,
                        color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc)) sources = &binding;
                    break;
                }
            if (sources) set = &candidate;
        }
    }
    if (!set)
    {
        set = find_or_create_resource_set(device, color, output, motion, depth, ui, ui_alpha,
            color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc,
            display_width, display_height, work_width, work_height, false, native_resolve_only,
            !native_resolve_only || allow_control_allocation);
        sources = set;
    }
    if (!set && !native_resolve_only && evaluation_pass == 0)
    {
        // Scaling pressure must not disable the independent image controls.
        // Keep native NR for this generation, with a smaller resolve allocation.
        g_multipass_groups.allocation_failed(generation, group_token);
        native_controls_generation = generation;
        native_resolve_only = true;
        work_width = display_width;
        work_height = display_height;
        set = find_or_create_resource_set(device, color, output, motion, depth, ui, ui_alpha,
            color_desc, output_desc, motion_desc, depth_desc, ui_desc, ui_alpha_desc,
            display_width, display_height, work_width, work_height, false, true, allow_control_allocation);
        sources = set;
        if (set) log_message(reshade::log::level::warning,
            "NR scale admission: requested=%d%% effective=100%%; independent per-pass controls retained. Scaling stays native until settings change.", scale_percent);
    }
    if (set == nullptr)
    {
        g_scale_history.invalidate_from(reinterpret_cast<std::uintptr_t>(device), evaluation_pass);
        if (native_resolve_only)
        {
            bool old_scaled_sets = false;
            for (const auto &candidate : g_resource_sets)
                old_scaled_sets |= candidate.active && !candidate.capture && !candidate.native_feature &&
                    !candidate.native_resolve_only && candidate.device == device;
            if (old_scaled_sets && controls_drain_generation != generation)
            {
                // A compact fallback must not compete with the scaled cache
                // it replaces. Retire through the existing nonblocking fence
                // barrier once; retain native-controls routing after the drain.
                controls_drain_generation = native_controls_generation = generation;
                g_quiesce_generation.store(g_scale_generation.fetch_add(1) + 1, std::memory_order_release);
                log_message(reshade::log::level::warning,
                    "NR native controls: retiring scaled cache through real fences before compact admission (generation=%u).", generation);
            }
            else if (allow_control_allocation)
            {
                controls_retry_generation = generation;
                controls_retry_after = GetTickCount64() + 250;
                controls_recovery_pending = true;
            }
        }
        g_multipass_groups.allocation_failed(generation, group_token);
        if (evaluation_pass == 0)
        {
            g_memory_native_groups.fetch_add(1, std::memory_order_relaxed);
            return native_fallback("native-control resources unavailable; waiting for fence-safe reuse or memory recovery");
        }
        g_partial_group_suppressed.fetch_add(1, std::memory_order_relaxed);
        g_effective_scale.store(100, std::memory_order_relaxed);
        log_fallback_once(generation,
            "a later multipass allocation failed; the previous pass was preserved and future groups stay native");
        return 0;
    }
    command_list_record->references.sets |= 1ull << (set - g_resource_sets.data());
    g_scaled_calls.fetch_add(1, std::memory_order_relaxed);
    if (framegen_route)
        g_framegen_scaled_calls.fetch_add(1, std::memory_order_relaxed);
    if (trace_this_call)
        log_text(reshade::log::level::info,
            "RenoDX Neural Resolution: [6/9 resources] working textures and persistent views are ready.");

    if (native_resolve_only && set->inputs_are_shader_resources)
        cmd_list->barrier(set->work_motion, reshade::api::resource_usage::shader_resource_non_pixel,
            reshade::api::resource_usage::unordered_access);
    if (!native_resolve_only && set->inputs_are_shader_resources)
    {
        const reshade::api::resource resources[] = {
            set->work_color, set->work_motion, set->work_depth };
        constexpr reshade::api::resource_usage old_states[] = {
            reshade::api::resource_usage::shader_resource_non_pixel,
            reshade::api::resource_usage::shader_resource_non_pixel,
            reshade::api::resource_usage::shader_resource_non_pixel };
        constexpr reshade::api::resource_usage new_states[] = {
            reshade::api::resource_usage::unordered_access,
            reshade::api::resource_usage::unordered_access,
            reshade::api::resource_usage::unordered_access };
        cmd_list->barrier(3, resources, old_states, new_states);
        if (ui.handle != 0)
            cmd_list->barrier(set->work_ui,
                reshade::api::resource_usage::shader_resource_non_pixel,
                reshade::api::resource_usage::unordered_access);
        if (ui_alpha.handle != 0)
            cmd_list->barrier(set->work_ui_alpha,
                reshade::api::resource_usage::shader_resource_non_pixel,
                reshade::api::resource_usage::unordered_access);
    }
    // Snapshot each pass's input before NR or resolve can modify it. This also
    // makes in-place input/output safe: the final UAV write never reads itself.
    if (color == output) cmd_list->barrier(color,
        reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::shader_resource_non_pixel);
    dispatch_resample(cmd_list, sources->source_color_srv, set->native_color_uav,
        color_x, color_y, display_width, display_height, display_width, display_height, 2);
    if (color == output) cmd_list->barrier(color,
        reshade::api::resource_usage::shader_resource_non_pixel, reshade::api::resource_usage::unordered_access);
    cmd_list->barrier(set->native_color, reshade::api::resource_usage::unordered_access,
        reshade::api::resource_usage::shader_resource_non_pixel);
    if (native_resolve_only)
    {
        // The snapshot is already the native-sized NR input. Depth, UI and
        // first-pass motion remain the caller's original read-only resources.
        dispatch_resample(cmd_list, sources->source_motion_srv, set->work_motion_uav,
            motion_x, motion_y, motion_width, motion_height, display_width, display_height,
            nr::motion_resample_filter(multipass_motion_mode, evaluation_pass));
        cmd_list->barrier(set->work_motion, reshade::api::resource_usage::unordered_access,
            reshade::api::resource_usage::shader_resource_non_pixel);
    }
    else
    {
        dispatch_resample(cmd_list, set->native_color_srv, set->work_color_uav,
            0, 0, display_width, display_height, work_width, work_height,
            nr::input_resample_filter(scale_percent));
        dispatch_resample(cmd_list, sources->source_motion_srv, set->work_motion_uav,
            motion_x, motion_y, motion_width, motion_height, work_width, work_height,
            nr::motion_resample_filter(multipass_motion_mode, evaluation_pass));
        dispatch_resample(cmd_list, sources->source_depth_srv, set->work_depth_uav,
            depth_x, depth_y, depth_width, depth_height, work_width, work_height, 3);
        if (ui.handle != 0)
            dispatch_resample(cmd_list, sources->source_ui_srv, set->work_ui_uav,
                0, 0, display_width, display_height, work_width, work_height);
        if (ui_alpha.handle != 0)
            dispatch_resample(cmd_list, sources->source_ui_alpha_srv, set->work_ui_alpha_uav,
                0, 0, display_width, display_height, work_width, work_height);
        if (trace_this_call)
            log_text(reshade::log::level::info,
                "RenoDX Neural Resolution: [7/9 resample] input resampling commands recorded.");

        {
            const reshade::api::resource resources[] = {
                set->work_color, set->work_motion, set->work_depth };
            constexpr reshade::api::resource_usage old_states[] = {
                reshade::api::resource_usage::unordered_access,
                reshade::api::resource_usage::unordered_access,
                reshade::api::resource_usage::unordered_access };
            constexpr reshade::api::resource_usage new_states[] = {
                reshade::api::resource_usage::shader_resource_non_pixel,
                reshade::api::resource_usage::shader_resource_non_pixel,
                reshade::api::resource_usage::shader_resource_non_pixel };
            cmd_list->barrier(3, resources, old_states, new_states);
            if (ui.handle != 0)
                cmd_list->barrier(set->work_ui,
                    reshade::api::resource_usage::unordered_access,
                    reshade::api::resource_usage::shader_resource_non_pixel);
            if (ui_alpha.handle != 0)
                cmd_list->barrier(set->work_ui_alpha,
                    reshade::api::resource_usage::unordered_access,
                    reshade::api::resource_usage::shader_resource_non_pixel);
        }
    }
    set->inputs_are_shader_resources = true;

    alignas(16) std::array<std::uint8_t, 0xB0> scaled_input = {};
    std::memcpy(scaled_input.data(), input, kInputSize);
    field<void *>(scaled_input.data(), 0x10) = reinterpret_cast<void *>(set->work_color.handle);
    field<void *>(scaled_input.data(), 0x18) = reinterpret_cast<void *>(set->work_output.handle);
    field<void *>(scaled_input.data(), 0x20) = reinterpret_cast<void *>(set->work_motion.handle);
    field<void *>(scaled_input.data(), 0x28) = reinterpret_cast<void *>(set->work_depth.handle);
    if (ui.handle != 0)
        field<void *>(scaled_input.data(), 0x30) = reinterpret_cast<void *>(set->work_ui.handle);
    if (ui_alpha.handle != 0)
        field<void *>(scaled_input.data(), 0x38) = reinterpret_cast<void *>(set->work_ui_alpha.handle);
    field<std::uint32_t>(scaled_input.data(), 0x44) = work_width;
    field<std::uint32_t>(scaled_input.data(), 0x48) = work_height;

    const float ratio_x = static_cast<float>(work_width) / static_cast<float>(display_width);
    const float ratio_y = static_cast<float>(work_height) / static_cast<float>(display_height);
    field<float>(scaled_input.data(), 0x4C) *= ratio_x;
    field<float>(scaled_input.data(), 0x50) *= ratio_y;
    field<float>(scaled_input.data(), 0x54) *= ratio_x;
    field<float>(scaled_input.data(), 0x58) *= ratio_y;

    // Replace all explicit color/motion/depth subrects with the full scaled extent.
    for (const std::size_t rect : { 0x60u, 0x70u, 0x80u, 0x90u })
    {
        field<std::uint64_t>(scaled_input.data(), rect) = 0;
        field<std::uint32_t>(scaled_input.data(), rect + 8) = work_width;
        field<std::uint32_t>(scaled_input.data(), rect + 12) = work_height;
    }
    if (native_resolve_only)
    {
        std::memcpy(scaled_input.data(), input, kInputSize);
        field<void *>(scaled_input.data(), 0x10) = reinterpret_cast<void *>(set->native_color.handle);
        field<void *>(scaled_input.data(), 0x18) = reinterpret_cast<void *>(set->work_output.handle);
        for (const std::size_t rect : {0x60u, 0x70u})
        {
            field<std::uint64_t>(scaled_input.data(), rect) = 0;
            field<std::uint32_t>(scaled_input.data(), rect + 8) = display_width;
            field<std::uint32_t>(scaled_input.data(), rect + 12) = display_height;
        }
        if (evaluation_pass != 0)
        {
            field<void *>(scaled_input.data(), 0x20) = reinterpret_cast<void *>(set->work_motion.handle);
            field<std::uint64_t>(scaled_input.data(), 0x80) = 0;
            field<std::uint32_t>(scaled_input.data(), 0x88) = display_width;
            field<std::uint32_t>(scaled_input.data(), 0x8C) = display_height;
        }
        if (evaluation_pass == 0) controls.mode = 0;
    }

    // Experimental backend: rotating working textures share one NR history per
    // device/pass. Creating another texture slot must not reset that history.
    if (history->generation != generation ||
        nr::reset_later_pass_history(multipass_motion_mode, evaluation_pass))
    {
        field<std::uint8_t>(scaled_input.data(), 0x5D) = 1;
        set->reset_generation = generation;
    }

    const std::uint64_t result = invoke_native_evaluation(scaled_input.data(), true);
    if ((result & 0xFFu) == 0)
    {
        cmd_list->barrier(set->native_color, reshade::api::resource_usage::shader_resource_non_pixel,
            reshade::api::resource_usage::unordered_access);
        g_multipass_groups.allocation_failed(generation, group_token);
        if (evaluation_pass != 0)
        {
            g_partial_group_suppressed.fetch_add(1, std::memory_order_relaxed);
            g_effective_scale.store(100, std::memory_order_relaxed);
            log_fallback_once(generation,
                "a later scaled pass failed; the previous pass was preserved and future groups stay native");
            return 0;
        }
        return native_fallback("the scaled Neural Rendering evaluation failed");
    }
    history->generation = generation; // Failed records never consume the reset.
    g_effective_scale.store(native_resolve_only ? 100 : scale_percent, std::memory_order_relaxed);
    unsigned failed_generation = g_multipass_vram_failure_generation.load(std::memory_order_acquire);
    if (nr::multipass_vram_failure_recovered(
            failed_generation, generation, pass_count, g_multipass_groups.complete()))
        g_multipass_vram_failure_generation.compare_exchange_strong(
            failed_generation, 0, std::memory_order_release, std::memory_order_relaxed);
    if (trace_this_call)
        log_text(reshade::log::level::info,
            "RenoDX Neural Resolution: [8/9 evaluation] scaled Neural Rendering evaluation returned successfully.");

#ifdef NR_DAWNWALKER_NO_COPYBACK_TEST
    const bool suppress_copyback = framegen_route && evaluation_pass != 0;
#else
    constexpr bool suppress_copyback = false;
#endif
    if (!suppress_copyback)
    {
        const bool visualize_edge_mask = g_visualize_edge_mask.load(std::memory_order_relaxed);
        const int edge_protection_percent = g_edge_protection_percent.load(std::memory_order_relaxed);
        const bool use_edge_depth = nr::uses_multipass_edge_depth(
            g_multipass_edge_protection_enabled.load(std::memory_order_relaxed),
            evaluation_pass, edge_protection_percent, visualize_edge_mask);
        const float edge_protection = use_edge_depth ?
            nr::multipass_edge_mode(edge_protection_percent, visualize_edge_mask) : 0.0f;
        const float edge_thickness = use_edge_depth ?
            nr::multipass_edge_thickness(g_edge_thickness_percent.load(std::memory_order_relaxed)) : 0.0f;
        const float edge_shift = use_edge_depth ? static_cast<float>(nr::clamp_multipass_edge_shift(
            g_edge_shift_pixels.load(std::memory_order_relaxed))) : 0.0f;
        const float edge_softness = use_edge_depth ? nr::multipass_edge_softness(
            g_edge_softness_percent.load(std::memory_order_relaxed)) : 0.0f;
        const auto edge_depth = use_edge_depth ?
            (native_resolve_only ? sources->source_depth_srv : set->work_depth_srv) :
            reshade::api::resource_view{};
        cmd_list->barrier(set->work_output,
            reshade::api::resource_usage::unordered_access,
            reshade::api::resource_usage::shader_resource_non_pixel);
        dispatch_resample(cmd_list, set->work_output_srv, sources->source_output_uav,
            0, 0, work_width, work_height, display_width, display_height,
            controls.mode == 1 ? 1u : 4u,
            static_cast<float>(controls.sharpness) / 100.0f,
            native_resolve_only ? set->native_color_srv : set->work_color_srv, set->native_color_srv, output_x, output_y,
            static_cast<float>(controls.transfer) / 100.0f,
            static_cast<float>(controls.color) / 100.0f,
            static_cast<float>(controls.detail) / 100.0f,
            static_cast<float>(controls.coupling) / 100.0f,
            edge_protection, edge_depth,
            use_edge_depth && native_resolve_only ? depth_x : 0u,
            use_edge_depth && native_resolve_only ? depth_y : 0u,
            use_edge_depth ? (native_resolve_only ? depth_width : work_width) : 0u,
            use_edge_depth ? (native_resolve_only ? depth_height : work_height) : 0u,
            edge_thickness, edge_shift, edge_softness,
            use_edge_depth && field<std::uint8_t>(input, 0x5C) != 0 ? 1u : 0u);
        cmd_list->barrier(output, reshade::api::resource_usage::unordered_access,
            reshade::api::resource_usage::unordered_access);
    }
    cmd_list->barrier(set->native_color, reshade::api::resource_usage::shader_resource_non_pixel,
        reshade::api::resource_usage::unordered_access);
    if (!suppress_copyback)
        cmd_list->barrier(set->work_output,
            reshade::api::resource_usage::shader_resource_non_pixel,
            reshade::api::resource_usage::unordered_access);
#ifdef NR_DAWNWALKER_NO_COPYBACK_TEST
    if (suppress_copyback &&
        g_no_copyback_log_generation.exchange(generation,std::memory_order_relaxed) != generation)
        log_message(reshade::log::level::info,
            "NR DAWNWALKER A/B 1: pass=%u evaluated successfully; FrameGen caller-output copyback suppressed.",
            evaluation_pass);
#endif
    static std::array<unsigned, 10> logged_resolve_generations = {};
    if (evaluation_pass < logged_resolve_generations.size() && logged_resolve_generations[evaluation_pass] != generation)
    {
        logged_resolve_generations[evaluation_pass] = generation;
        log_message(reshade::log::level::info,
            "NR pass resolve recorded: pass=%u resolve=%s sharpness=%d%% transfer=%d%% color=%d%% scratch=%u shared=%u; output origin=%u/%u.",
            evaluation_pass + 1, controls.mode == 1 ? "matched-residual" : "direct", controls.sharpness,
            controls.transfer, controls.color, static_cast<unsigned>(set - g_resource_sets.data()), sources != set ? 1u : 0u,
            output_x, output_y);
    }

    if (g_logged_success_generation.exchange(generation, std::memory_order_relaxed) != generation)
    {
        log_message(reshade::log::level::info,
            "RenoDX Neural Resolution: generation %u completed its first scaled frame successfully.",
            generation);
    }
    if (share_working_set && evaluation_pass + 1 < pass_count)
    {
        command_list_record->group_set = static_cast<unsigned>(set - g_resource_sets.data());
        command_list_record->next_pass = evaluation_pass + 1;
        command_list_record->group_frame = group_token;
        command_list_record->group_output = output;
    }
    if (controls_retry_generation == generation && controls_recovery_pending && evaluation_pass + 1 == pass_count)
    {
        controls_recovery_pending = false;
        log_message(reshade::log::level::info,
            "NR controls recovered: generation=%u passes=%u effective=%d%%; existing memory limits and fences retained.",
            generation, pass_count, native_resolve_only ? 100 : scale_percent);
    }
    return result;
}

std::uint64_t __fastcall scaled_evaluate_impl(void *input, unsigned call_site)
{
    const auto ticket = capture::before_bypass(input);
    const auto result = scaled_evaluate_body(input,call_site);
    if (ticket) capture::after(input,result,ticket);
    return result;
}

// Caller holds g_render_mutex. Never evict a recording that still references
// our resources, including PRE-Reset recordings that may still execute.
TrackedCommandList *claim_command_list_locked(reshade::api::command_list *command_list,
    reshade::api::device *device, std::uint64_t native_command_list, std::uint64_t native_device)
{
    for (auto &tracked : g_tracked_command_lists)
        if (tracked.active && tracked.command_list == command_list)
            return &tracked;
    for (unsigned sweep = 0; sweep < 2; ++sweep)
        for (std::size_t n = 0; n < g_tracked_command_lists.size(); ++n)
        {
            const auto index = (g_command_slot_cursor + n) % g_tracked_command_lists.size();
            auto &tracked = g_tracked_command_lists[index];
            if (tracked.active && (sweep == 0 || tracked.references.sets != 0)) continue;
            if (tracked.active) g_command_slots_recycled.fetch_add(1, std::memory_order_relaxed);
            tracked = {};
            tracked.active = true;
            tracked.command_list = command_list;
            tracked.device = device;
            tracked.native_command_list = native_command_list;
            tracked.native_device = native_device;
            g_command_slot_cursor = (index + 1) % g_tracked_command_lists.size();
            return &tracked;
        }
    g_command_slots_exhausted.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
}

void on_init_command_list(reshade::api::command_list *command_list)
{
    if (command_list == nullptr)
        return;
    reshade::api::device *device = command_list->get_device();
    if (device == nullptr || device->get_api() != reshade::api::device_api::d3d12)
        return;
    const std::uint64_t native_command_list = command_list->get_native();
    const std::uint64_t native_device = device->get_native();
    if (native_command_list == 0 || native_device == 0)
        return;

    ScopedLock lock(g_render_mutex);
    if (claim_command_list_locked(command_list, device, native_command_list, native_device)) return;
    if (!g_command_registry_warning_logged.exchange(true, std::memory_order_relaxed))
        log_text(reshade::log::level::warning,
            "RenoDX Neural Resolution: command-list registry is fully pinned; preserving every outstanding reference; untracked lists use the original path.");
}

void on_destroy_command_list(reshade::api::command_list *command_list)
{
    ScopedLock lock(g_render_mutex);
    capture::reset_command(command_list);
    for (auto &tracked : g_tracked_command_lists)
    {
        if (tracked.active && tracked.command_list == command_list)
        {
            tracked = {};
            return;
        }
    }
}

void on_destroy_device(reshade::api::device *device)
{
    ScopedLock lock(g_render_mutex);
    if (g_local_memory_adapter.device == device)
    {
        if (g_local_memory_adapter.adapter != nullptr)
            g_local_memory_adapter.adapter->Release();
        g_local_memory_adapter = {};
    }
    g_scale_history.forget(reinterpret_cast<std::uintptr_t>(device));
    g_evaluation_device.forget(reinterpret_cast<std::uintptr_t>(device));
    for (auto &set : g_resource_sets)
        if (set.active && set.device == device)
            destroy_resource_set(set);
    capture::destroy_pipeline(device);
    if (g_pipeline_device == device)
    {
        if (g_pipeline.handle != 0)
            device->destroy_pipeline(g_pipeline);
        if (g_pipeline_layout.handle != 0)
            device->destroy_pipeline_layout(g_pipeline_layout);
        if (g_edge_pipeline.handle != 0)
            device->destroy_pipeline(g_edge_pipeline);
        if (g_edge_pipeline_layout.handle != 0)
            device->destroy_pipeline_layout(g_edge_pipeline_layout);
        g_pipeline = {};
        g_pipeline_layout = {};
        g_edge_pipeline = {};
        g_edge_pipeline_layout = {};
        g_pipeline_device = nullptr;
    }
    clear_device_tracking_locked(device);
    for (auto &queue : g_tracked_queues)
        if (queue.device == device)
        {
            if (queue.fence) queue.fence->Release();
            if (queue.native) queue.native->Release();
            queue = {};
        }
}

void on_destroy_resource(reshade::api::device *device, reshade::api::resource resource)
{
    ScopedLock lock(g_render_mutex);
    capture::forget_resource(device, resource);
    for (auto &set : g_resource_sets)
    {
        if (!set.active || set.device != device)
            continue;

        // Invalidate here; the collector handles recording references and GPU
        // fences. Never destroy owned objects inside a source destruction event.
        if (set.source_color == resource || set.source_output == resource ||
            set.source_motion == resource || set.source_depth == resource ||
            set.source_ui == resource || set.source_ui_alpha == resource)
            set.valid = false;
        for (const auto &sources : set.additional_sources)
            if (sources.source_color == resource || sources.source_output == resource ||
                sources.source_motion == resource || sources.source_depth == resource ||
                sources.source_ui == resource || sources.source_ui_alpha == resource)
                set.valid = false;
    }
}

} // namespace dx12
