// Diagnostic-only, CPU observations. No copies, barriers, queue signals/waits,
// vendor parameter writes or NR evaluations are added by this observer.
namespace boundary_probe {
SRWLOCK mutex = SRWLOCK_INIT;
nr::BoundaryIdentities identities;
std::atomic_uint missed = 0;
std::atomic_ullong tag_serial = 0;

void report() {
    log_message(reshade::log::level::info,"NR boundary hooks: active-mask=0x%x (tag=1 tag-frame=2 token=4 execute=8 signal=16 wait=32); missing hooks mean incomplete coverage even when missed=0. Tags are BEFORE RenoDX resource/command rewriting.",hook_mask.load());
    log_message(reshade::log::level::info,"NR boundary coverage: missed=%u (cumulative invalid/unsupported/contended/full observations); fence values are instantaneous samples, UINT64_MAX is unavailable/device-removed, never completion proof.",missed.load());
#ifdef NR_NESTED_SOURCE_GUARD
    log_message(reshade::log::level::info,"NR nested-source guard: active=%u duplicate-returns-suppressed=%llu; only completed native SR within the same nested evaluation, not separate calls or configured passes.", (hook_mask.load() & 64) && evaluation_tracking.load() ? 1u : 0u, duplicates_suppressed.load());
#endif
}
using Tag = unsigned (*)(const void *, const void *, unsigned, void *);
using TagFrame = unsigned (*)(const void *, const void *, const void *, unsigned, void *);
using Token = unsigned (*)(void **, const unsigned *);
using Execute = void (STDMETHODCALLTYPE *)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);
using FenceOp = HRESULT (STDMETHODCALLTYPE *)(ID3D12CommandQueue *, ID3D12Fence *, UINT64);
Tag original_tag = nullptr;
TagFrame original_tag_frame = nullptr;
Token original_token = nullptr;
Execute original_execute = nullptr;
FenceOp original_signal = nullptr, original_wait = nullptr;

// Read only public v1 prefixes. Reject unknown GUIDs/versions before reading
// members; never walk extension chains or call an unknown FrameToken vtable.
struct Base { void *next; GUID type; std::uint64_t version; };
struct TagData { Base base; void *resource; unsigned type, lifecycle, left, top, width, height; };
struct ResourceData { Base base; unsigned type, padding; void *native, *memory, *view; unsigned state; };
struct ViewportData { Base base; unsigned value; };
static_assert(sizeof(Base) == 32 && sizeof(TagData) == 64);
static_assert(offsetof(ResourceData, native) == 40 && offsetof(ResourceData, state) == 64);
constexpr GUID tag_guid = {0x4c6a5aad,0xb445,0x496c,{0x87,0xff,0x1a,0xf3,0x84,0x5b,0xe6,0x53}};
constexpr GUID resource_guid = {0x3a9d70cf,0x2418,0x4b72,{0x83,0x91,0x13,0xf8,0x72,0x1c,0x72,0x61}};
constexpr GUID viewport_guid = {0x171b6435,0x9b3c,0x4fc8,{0x99,0x94,0xfb,0xe5,0x25,0x69,0xaa,0xa4}};
template<class T> bool read(const void *pointer, T &value) {
    SIZE_T copied = 0;
    return pointer && ReadProcessMemory(GetCurrentProcess(),pointer,&value,sizeof(value),&copied) && copied == sizeof(value);
}
template<class T> bool typed(const void *pointer, const GUID &guid, T &value) {
    Base base{};
    return read(pointer,base) && base.version == 1 && !std::memcmp(&base.type,&guid,sizeof(guid)) && read(pointer,value);
}
void watch_command(std::uint64_t command) {
    const auto now = GetTickCount64();
    if (!command || !g_frame_trace.recording(now)) return;
    if (!TryAcquireSRWLockExclusive(&mutex)) { ++missed; return; }
    identities.begin(g_frame_trace.capture_id());
    if (!nr::BoundaryIdentities::add(identities.commands,command)) ++missed;
    ReleaseSRWLockExclusive(&mutex);
}
std::uint64_t observe_tags(const void *frame, const void *viewport, const void *tags, unsigned count, void *command) {
    const auto now = GetTickCount64();
    if (!g_frame_trace.recording(now)) return 0;
    const auto serial = ++tag_serial;
    ViewportData vp{};
    if (!typed(viewport,viewport_guid,vp) || count > 32 || (count && !tags)) { ++missed; return serial; }
    watch_command(reinterpret_cast<std::uint64_t>(command));
    for (unsigned i = 0; i < count; ++i) {
        TagData tag{}; ResourceData resource{};
        if (!typed(static_cast<const unsigned char *>(tags)+i*sizeof(TagData),tag_guid,tag) ||
            tag.lifecycle > 2 || (tag.resource && !typed(tag.resource,resource_guid,resource))) { ++missed; continue; }
        nr::FrameTraceEvent e;
        e.kind = nr::TraceKind::resource_tag; e.tick = now; e.thread = GetCurrentThreadId();
        e.callback = serial;
        e.command = reinterpret_cast<std::uint64_t>(command);
        e.feature = reinterpret_cast<std::uint64_t>(frame); // Opaque token, NOT frame index.
        e.source = vp.value; e.pass = tag.type; e.hook = tag.lifecycle;
        e.color = reinterpret_cast<std::uint64_t>(resource.native);
        e.result = tag.resource ? resource.state : UINT_MAX;
        e.width = tag.width; e.height = tag.height; e.motion = tag.left; e.depth = tag.top;
        e.parameters = reinterpret_cast<std::uint64_t>(tag.base.next); // Extensions present, not decoded.
        g_frame_trace.push(e);
    }
    return serial;
}
void tag_return(std::uint64_t serial, unsigned result) {
    if (!serial) return;
    nr::FrameTraceEvent e; e.kind = nr::TraceKind::tag_return;
    e.tick = GetTickCount64(); e.thread = GetCurrentThreadId(); e.callback = serial; e.result = result;
    g_frame_trace.push(e);
}
unsigned tag_dispatch(const void *vp, const void *tags, unsigned count, void *cmd) {
    const auto serial = observe_tags(nullptr,vp,tags,count,cmd);
    const auto result = original_tag(vp,tags,count,cmd);
    tag_return(serial,result); return result;
}
unsigned tag_frame_dispatch(const void *frame, const void *vp, const void *tags, unsigned count, void *cmd) {
    const auto serial = observe_tags(frame,vp,tags,count,cmd);
    const auto result = original_tag_frame(frame,vp,tags,count,cmd);
    tag_return(serial,result); return result;
}
unsigned token_dispatch(void **token, const unsigned *index) {
    unsigned supplied = UINT_MAX;
    const bool known = g_frame_trace.recording(GetTickCount64()) && read(index,supplied);
    const auto result = original_token(token,index);
    const auto now = GetTickCount64();
    if (g_frame_trace.recording(now)) {
        void *value = nullptr;
        nr::FrameTraceEvent e; e.kind = nr::TraceKind::frame_token; e.tick = now;
        e.thread = GetCurrentThreadId(); e.result = result; e.frame = known ? supplied : UINT_MAX;
        if (result == 0 && read(token,value)) e.feature = reinterpret_cast<std::uint64_t>(value);
        g_frame_trace.push(e);
    }
    return result;
}
void STDMETHODCALLTYPE execute_dispatch(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *lists) {
    // All inspection occurs BEFORE forwarding; returned means submitted, not completed.
    const auto now = GetTickCount64();
    if (g_frame_trace.recording(now)) {
        if (count > 256 || (count && !lists) || !TryAcquireSRWLockExclusive(&mutex)) ++missed;
        else {
            identities.begin(g_frame_trace.capture_id());
            for (UINT i = 0; i < count; ++i) {
                const auto cmd = reinterpret_cast<std::uint64_t>(lists[i]);
                if (!nr::BoundaryIdentities::contains(identities.commands,cmd)) continue;
                const auto q = reinterpret_cast<std::uint64_t>(queue);
                if (!nr::BoundaryIdentities::add(identities.queues,q)) ++missed;
                nr::FrameTraceEvent e; e.kind = nr::TraceKind::native_submit; e.tick = now;
                e.thread = GetCurrentThreadId(); e.command = cmd; e.queue = q;
                g_frame_trace.push(e);
            }
            ReleaseSRWLockExclusive(&mutex);
        }
    }
    original_execute(queue,count,lists);
}
void observe_fence(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value, HRESULT result, bool wait) {
    const auto now = GetTickCount64();
    if (!g_frame_trace.recording(now)) return;
    if (!TryAcquireSRWLockExclusive(&mutex)) { ++missed; return; }
    const bool known = identities.capture == g_frame_trace.capture_id() &&
        nr::BoundaryIdentities::contains(identities.queues,reinterpret_cast<std::uint64_t>(queue));
    ReleaseSRWLockExclusive(&mutex);
    if (!known) return;
    nr::FrameTraceEvent e; e.kind = nr::TraceKind::native_fence; e.tick = now;
    e.thread = GetCurrentThreadId(); e.queue = reinterpret_cast<std::uint64_t>(queue);
    e.color = reinterpret_cast<std::uint64_t>(fence); e.frame = value; e.result = static_cast<unsigned>(result);
    e.source = wait ? 1u : 0u;
    // Sample only while caller owns the fence. No retention, waits or new signals.
    e.output = fence && SUCCEEDED(result) ? fence->GetCompletedValue() : UINT64_MAX;
    g_frame_trace.push(e);
}
HRESULT STDMETHODCALLTYPE signal_dispatch(ID3D12CommandQueue *q, ID3D12Fence *f, UINT64 v) {
    const auto result = original_signal(q,f,v); observe_fence(q,f,v,result,false); return result;
}
HRESULT STDMETHODCALLTYPE wait_dispatch(ID3D12CommandQueue *q, ID3D12Fence *f, UINT64 v) {
    const auto result = original_wait(q,f,v); observe_fence(q,f,v,result,true); return result;
}
void install_queue_hooks(ID3D12CommandQueue *queue) {
    auto **vt = *reinterpret_cast<void ***>(queue);
    void *targets[] = {vt[10],vt[14],vt[15]};
    void *callbacks[] = {reinterpret_cast<void *>(&execute_dispatch),reinterpret_cast<void *>(&signal_dispatch),reinterpret_cast<void *>(&wait_dispatch)};
    void **originals[] = {reinterpret_cast<void **>(&original_execute),reinterpret_cast<void **>(&original_signal),reinterpret_cast<void **>(&original_wait)};
    const char *names[] = {"execute","signal","wait"};
    for (unsigned i=0; i<3; ++i) {
        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(targets[i]),&owner) ||
            (owner != GetModuleHandleW(L"D3D12Core.dll") && owner != GetModuleHandleW(L"d3d12.dll"))) {
            log_message(reshade::log::level::warning,"NR boundary hook %s disabled: native entry is not owned by D3D12 runtime.",names[i]); continue;
        }
        install_hook(targets[i],callbacks[i],originals[i],names[i],8u<<i);
    }
}
void install_streamline_hooks(HMODULE sl) {
    if (!sl || !pin_address(sl)) return;
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(reinterpret_cast<unsigned char *>(sl)+
        reinterpret_cast<IMAGE_DOS_HEADER *>(sl)->e_lfanew);
    // Local 2.13.0 image profile; never assume the latest public headers certify
    // an arbitrary installed build. Prefix structures are independently checked.
    if (nt->FileHeader.TimeDateStamp != 0x6a7c8ed5 || nt->OptionalHeader.SizeOfImage != 0xa2000) {
        log_text(reshade::log::level::warning,"NR boundary tags/token disabled: unverified Streamline image profile; queue observation is independent."); return;
    }
    // These are RenoDX's existing Hooked_slSetTag/Hooked_slSetTagForFrame
    // entries in the hash-verified base addon, NOT already-detoured SL exports.
    // Their Real_* slots stay untouched, preserving Detours attach/detach state.
    constexpr unsigned entries[] = {0xA240,0xB920}, slots[] = {0x26DE40,0x26DE48};
    constexpr unsigned char prefix[] = {0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41};
    void *callbacks[] = {reinterpret_cast<void *>(&tag_dispatch),reinterpret_cast<void *>(&tag_frame_dispatch)};
    void **originals[] = {reinterpret_cast<void **>(&original_tag),reinterpret_cast<void **>(&original_tag_frame)};
    const char *names[] = {"tag-before-RenoDX","tag-frame-before-RenoDX"};
    for (unsigned i=0; i<2; ++i) {
        auto *base = reinterpret_cast<unsigned char *>(g_target_module);
        if (!base) break;
        void *registered = nullptr;
        if (!read(base+slots[i],registered) || !registered || std::memcmp(base+entries[i],prefix,sizeof(prefix))) {
            log_message(reshade::log::level::warning,"NR boundary hook %s disabled: base callback/profile or registration mismatch.",names[i]); continue;
        }
        install_hook(base+entries[i],callbacks[i],originals[i],names[i],1u<<i);
    }
    auto *token = reinterpret_cast<void *>(GetProcAddress(sl,"slGetNewFrameToken"));
    constexpr unsigned char token_prefix[] = {0x48,0x83,0xec,0x28,0x4c,0x8b,0xc2,0x48};
    if (token && !std::memcmp(token,token_prefix,sizeof(token_prefix)))
        install_hook(token,reinterpret_cast<void *>(&token_dispatch),reinterpret_cast<void **>(&original_token),"token",4);
    else log_text(reshade::log::level::warning,"NR boundary hook token disabled: slGetNewFrameToken prologue mismatch; other observers remain independent.");
}

void install(reshade::api::effect_runtime *runtime) {
    // Called by serialized overlay, never while holding render/queue locks.
    static bool queue_attempted = false, tags_attempted = false;
    if (!runtime || runtime->get_device()->get_api() != reshade::api::device_api::d3d12) return;
#ifdef NR_NESTED_SOURCE_GUARD
    install_evaluation_hook(); // Fallback for late attachment to an existing device.
#endif
    auto *queue = runtime->get_command_queue();
    if (!queue_attempted && queue && queue->get_native()) {
        queue_attempted = true;
        install_queue_hooks(reinterpret_cast<ID3D12CommandQueue *>(queue->get_native()));
    }
    if (!tags_attempted) if (auto sl = GetModuleHandleW(L"sl.interposer.dll")) {
        tags_attempted = true; install_streamline_hooks(sl);
        log_text(reshade::log::level::info,"NR boundary probe: independent observers initialized. Manual trace required; image capture NOT enabled.");
        report();
    }
}
}
