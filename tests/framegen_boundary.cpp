// Synthetic ABI/forwarding and live WARP hook checks. Never launches a game.
#define NR_LIFETIME_TEST
#define NR_EXPERIMENTAL_DX11
#define NR_FRAMEGEN_BOUNDARY_TRACE
#define NR_NESTED_SOURCE_GUARD
#include "../src/neural_resolution_addon.cpp"
#include <cassert>
#include <cstdio>

namespace {
unsigned calls = 0;
void *const command = reinterpret_cast<void *>(0x1000);
void *const frame = reinterpret_cast<void *>(0x2000);
boundary_probe::ViewportData viewport{{nullptr,boundary_probe::viewport_guid,1},7};
boundary_probe::ResourceData resource{{nullptr,boundary_probe::resource_guid,1},0,0,command,nullptr,nullptr,0x40};
boundary_probe::TagData tag{{nullptr,boundary_probe::tag_guid,1},&resource,3,1,0,0,1920,1080};
unsigned forward_tag(const void *vp, const void *tags, unsigned count, void *cmd) {
    assert(vp == &viewport && tags == &tag && count == 1 && cmd == command);
    ++calls; return 17; // Rejection must not be logged as accepted tags.
}
unsigned forward_frame(const void *token, const void *vp, const void *tags, unsigned count, void *cmd) {
    assert(token == frame); return forward_tag(vp,tags,count,cmd);
}
unsigned forward_token(void **token, const unsigned *index) {
    assert(index && *index == 42); *token = frame; ++calls; return 0;
}
boundary_probe::Tag existing_real_tag = nullptr;
boundary_probe::TagFrame existing_real_frame = nullptr;
unsigned existing_calls = 0;
__declspec(noinline) unsigned tag_export(const void *vp, const void *tags, unsigned count, void *cmd) {
    return forward_tag(vp,tags,count,cmd);
}
__declspec(noinline) unsigned frame_export(const void *token, const void *vp, const void *tags, unsigned count, void *cmd) {
    return forward_frame(token,vp,tags,count,cmd);
}
__declspec(noinline) unsigned existing_tag_hook(const void *vp, const void *tags, unsigned count, void *cmd) {
    ++existing_calls; return existing_real_tag(vp,tags,count,cmd);
}
__declspec(noinline) unsigned existing_frame_hook(const void *token, const void *vp, const void *tags, unsigned count, void *cmd) {
    ++existing_calls; return existing_real_frame(token,vp,tags,count,cmd);
}
void STDMETHODCALLTYPE forward_execute(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *lists) {
    assert(queue == reinterpret_cast<ID3D12CommandQueue *>(0x3000));
    assert(count == 1 && lists[0] == command); ++calls;
}
HRESULT STDMETHODCALLTYPE forward_fence(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value) {
    assert(queue == reinterpret_cast<ID3D12CommandQueue *>(0x3000) && !fence && value == 9);
    ++calls; return E_FAIL;
}
unsigned source_calls = 0, vendor_calls = 0, source_passes = 1;
unsigned source_result = 1;
unsigned mock_dispatch(boundary_probe::VendorEvaluate vendor, void *cmd, std::uint64_t feature, void *params, void *progress) {
    const auto result = vendor(cmd,feature,params,progress);
    if (!boundary_probe::reject_duplicate(1,false)) {
        source_calls += source_passes;
        boundary_probe::complete_source(1,source_result);
        source_result = 1;
    }
    return result;
}
unsigned leaf(void *cmd, std::uint64_t feature, void *params, void *progress) {
    assert(cmd == command && feature == 42 && params == &tag && progress == frame);
    ++vendor_calls; return 17;
}
unsigned nested(void *cmd, std::uint64_t feature, void *params, void *progress) {
    return boundary_probe::evaluate_dispatch(leaf,cmd,feature,params,progress);
}
unsigned siblings(void *cmd, std::uint64_t feature, void *params, void *progress) {
    assert(nested(cmd,feature,params,progress) == 17);
    return nested(cmd,feature,params,progress);
}
void check_nested_sources() {
    using namespace boundary_probe;
    evaluation_tls = TlsAlloc(); assert(evaluation_tls != TLS_OUT_OF_INDEXES);
    original_evaluate = mock_dispatch;
    assert(evaluate_dispatch(nested,command,42,&tag,frame) == 17);
    assert(source_calls == 1 && vendor_calls == 1 && duplicates_suppressed == 1 && !evaluation_scope());
    source_passes = 3;
    assert(evaluate_dispatch(nested,command,42,&tag,frame) == 17);
    assert(source_calls == 4 && vendor_calls == 2); // Sequential identical calls remain independent; all passes execute.
    assert(evaluate_dispatch(siblings,command,42,&tag,frame) == 17);
    assert(source_calls == 10 && vendor_calls == 4); // Two real child evaluations must not share a completed flag.
    source_result = 0;
    assert(evaluate_dispatch(nested,command,42,&tag,frame) == 17);
    assert(source_calls == 16 && vendor_calls == 5); // Failed child does not suppress its ancestor.
    EvaluationScope parent{nullptr,command,43,&tag};
    assert(TlsSetValue(evaluation_tls,&parent));
    assert(evaluate_dispatch(leaf,command,42,&tag,frame) == 17 && !parent.completed);
    parent.feature = 42; parent.command = frame;
    assert(evaluate_dispatch(leaf,command,42,&tag,frame) == 17 && !parent.completed);
    parent.command = command; parent.parameters = &resource;
    assert(evaluate_dispatch(leaf,command,42,&tag,frame) == 17 && !parent.completed);
    parent.parameters = &tag;
    assert(evaluate_dispatch(leaf,command,42,&tag,frame) == 17 && parent.completed);
    assert(reject_duplicate(1,false) && !reject_duplicate(1,true) && !reject_duplicate(2,false));
    const auto thread = CreateThread(nullptr,0,[](void *) -> DWORD {
        assert(!boundary_probe::evaluation_scope() && !boundary_probe::reject_duplicate(1,false));
        assert(boundary_probe::evaluate_dispatch(nested,command,42,&tag,frame) == 17);
        assert(!boundary_probe::evaluation_scope());
        return 0;
    },nullptr,0,nullptr);
    assert(thread && WaitForSingleObject(thread,5000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    assert(evaluation_scope() == &parent && parent.completed); // Independent caller thread cannot replace this scope.
    evaluation_tracking = false; assert(!reject_duplicate(1,false)); // TLS failure disables suppression, not vendor work.
    assert(TlsSetValue(evaluation_tls,nullptr));
    TlsFree(evaluation_tls); evaluation_tls = TLS_OUT_OF_INDEXES;
    assert(!reject_duplicate(1,false));
    evaluation_tracking = true;
    puts("Nested source guard: nested/duplicate returns, separate and sibling calls, argument/result preservation, multipass, failed parents, distinct identities, retry and fail-open checks passed.");
}
}
int main() {
    check_nested_sources();
    nr::BoundaryIdentities ids;
    ids.begin(1);
    assert(!ids.add(ids.commands,0));
    for (unsigned i=1; i<=128; ++i) assert(ids.add(ids.commands,i));
    assert(ids.add(ids.commands,1) && !ids.add(ids.commands,129));
    assert(ids.add(ids.queues,5));
    ids.begin(1); assert(ids.contains(ids.commands,128));
    ids.begin(2); assert(!ids.contains(ids.commands,128) && !ids.contains(ids.queues,5));

    using namespace boundary_probe;
    original_tag = forward_tag; original_tag_frame = forward_frame; original_token = forward_token;
    original_execute = forward_execute; original_signal = forward_fence; original_wait = forward_fence;
    assert(tag_dispatch(&viewport,&tag,1,command) == 17 && calls == 1);
    assert(tag_serial.load() == 0); // No capture: no decoding or observations.
    const auto now = GetTickCount64();
    assert(g_frame_trace.start(now,10000));
    assert(tag_dispatch(&viewport,&tag,1,command) == 17);
    assert(tag_frame_dispatch(frame,&viewport,&tag,1,command) == 17);
    const auto before = missed.load();
    tag.base.version = 99;
    assert(tag_dispatch(&viewport,&tag,1,command) == 17 && missed.load() == before+1);
    tag.base.version = 1;
    void *token = nullptr; unsigned index = 42;
    assert(token_dispatch(&token,&index) == 0 && token == frame);
    ID3D12CommandList *list = reinterpret_cast<ID3D12CommandList *>(command);
    auto *queue = reinterpret_cast<ID3D12CommandQueue *>(0x3000);
    execute_dispatch(queue,1,&list);
    assert(signal_dispatch(queue,nullptr,9) == E_FAIL);
    assert(wait_dispatch(queue,nullptr,9) == E_FAIL);
    assert(calls == 8);
    nr::FrameTraceEvent event;
    unsigned tags = 0, returns = 0, tokens = 0, submits = 0, fences = 0;
    while (g_frame_trace.pop(now+10001,&event)) {
        switch (event.kind) {
        case nr::TraceKind::resource_tag:
            ++tags; assert(event.callback && event.source == 7 && event.color == 0x1000);
            assert(event.result == 0x40 && event.width == 1920 && event.height == 1080); break;
        case nr::TraceKind::tag_return: ++returns; assert(event.result == 17); break;
        case nr::TraceKind::frame_token: ++tokens; assert(event.feature == 0x2000 && event.frame == 42); break;
        case nr::TraceKind::native_submit: ++submits; assert(event.command == 0x1000 && event.queue == 0x3000); break;
        case nr::TraceKind::native_fence: ++fences; assert(event.output == UINT64_MAX && event.result == static_cast<unsigned>(E_FAIL)); break;
        default: assert(false);
        }
    }
    assert(tags == 2 && returns == 3 && tokens == 1 && submits == 1 && fences == 2 && !g_frame_trace.dropped());
    // Reproduce already-detoured exports. Install our production observer on
    // the existing callback, not the export or its owner's Real_* slot.
    assert(MH_Initialize() == MH_OK);
    void *exports[] = {reinterpret_cast<void *>(&tag_export),reinterpret_cast<void *>(&frame_export)};
    void *older_hooks[] = {reinterpret_cast<void *>(&existing_tag_hook),reinterpret_cast<void *>(&existing_frame_hook)};
    std::array<unsigned char,8> pristine{};
    std::memcpy(pristine.data(),exports[0],pristine.size());
    assert(MH_CreateHook(exports[0],older_hooks[0],reinterpret_cast<void **>(&existing_real_tag)) == MH_OK);
    assert(MH_CreateHook(exports[1],older_hooks[1],reinterpret_cast<void **>(&existing_real_frame)) == MH_OK);
    for (auto target : exports) assert(MH_EnableHook(target) == MH_OK);
    assert(std::memcmp(pristine.data(),exports[0],pristine.size()) != 0);
    const auto saved_real_tag = existing_real_tag; const auto saved_real_frame = existing_real_frame;
    assert(install_hook(older_hooks[0],reinterpret_cast<void *>(&tag_dispatch),reinterpret_cast<void **>(&original_tag),"test-tag",1));
    assert(install_hook(older_hooks[1],reinterpret_cast<void *>(&tag_frame_dispatch),reinterpret_cast<void **>(&original_tag_frame),"test-tag-frame",2));
    assert(existing_real_tag == saved_real_tag && existing_real_frame == saved_real_frame && hook_mask == 3);
    const auto chained = GetTickCount64()+1;
    assert(g_frame_trace.start(chained,10000));
    Tag volatile call_tag = tag_export; TagFrame volatile call_frame = frame_export;
    assert(call_tag(&viewport,&tag,1,command) == 17);
    assert(call_frame(frame,&viewport,&tag,1,command) == 17);
    assert(calls == 10 && existing_calls == 2);
    tags = returns = 0;
    while (g_frame_trace.pop(chained+10001,&event)) {
        if (event.kind == nr::TraceKind::resource_tag) ++tags;
        if (event.kind == nr::TraceKind::tag_return) { ++returns; assert(event.result == 17); }
    }
    assert(tags == 2 && returns == 2 && !g_frame_trace.dropped());
    for (auto target : older_hooks) { assert(MH_DisableHook(target) == MH_OK); assert(MH_RemoveHook(target) == MH_OK); }
    assert(call_tag(&viewport,&tag,1,command) == 17);
    assert(call_frame(frame,&viewport,&tag,1,command) == 17);
    assert(calls == 12 && existing_calls == 4); // Prior chain survives observer removal.
    for (auto target : exports) { assert(MH_DisableHook(target) == MH_OK); assert(MH_RemoveHook(target) == MH_OK); }
    assert(MH_Uninitialize() == MH_OK);
    hook_mask = 0;
    install_streamline_hooks(GetModuleHandleW(nullptr)); // Unsupported image rejects only SL observers.
    assert(hook_mask == 0);
    // Actual D3D12 interception on WARP, isolated from games and NGX. The host
    // submits an empty list and owns the fences; observers add no GPU work.
    IDXGIFactory4 *factory = nullptr; IDXGIAdapter *adapter = nullptr;
    ID3D12Device *device = nullptr; ID3D12CommandQueue *native_queue = nullptr;
    ID3D12CommandAllocator *allocator = nullptr; ID3D12GraphicsCommandList *native_list = nullptr;
    ID3D12Fence *fence = nullptr;
    assert(SUCCEEDED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    assert(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    assert(SUCCEEDED(D3D12CreateDevice(adapter,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    D3D12_COMMAND_QUEUE_DESC desc{};
    assert(SUCCEEDED(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&native_queue))));
    assert(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    assert(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,nullptr,IID_PPV_ARGS(&native_list))));
    assert(SUCCEEDED(native_list->Close()));
    assert(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))));
    auto **vt = *reinterpret_cast<void ***>(native_queue);
    void *targets[] = {vt[10],vt[14],vt[15]};
    install_queue_hooks(native_queue);
    assert(hook_mask == (8|16|32)); // Queue capture survives unsupported tag setup.
    const auto second = GetTickCount64()+1;
    assert(g_frame_trace.start(second,10000));
    watch_command(reinterpret_cast<std::uint64_t>(native_list));
    ID3D12CommandList *submitted = native_list;
    native_queue->ExecuteCommandLists(1,&submitted);
    assert(SUCCEEDED(native_queue->Signal(fence,1)));
    HANDLE completed = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    assert(completed && SUCCEEDED(fence->SetEventOnCompletion(1,completed)));
    assert(WaitForSingleObject(completed,5000) == WAIT_OBJECT_0);
    assert(SUCCEEDED(native_queue->Wait(fence,1)));
    for (auto target : targets) { assert(MH_DisableHook(target) == MH_OK); assert(MH_RemoveHook(target) == MH_OK); }
    assert(MH_Uninitialize() == MH_OK);
    submits = fences = 0;
    while (g_frame_trace.pop(second+10001,&event)) {
        if (event.kind == nr::TraceKind::native_submit) { ++submits; assert(event.command == reinterpret_cast<std::uint64_t>(native_list)); }
        if (event.kind == nr::TraceKind::native_fence) {
            ++fences; assert(event.frame == 1 && event.result == S_OK);
            if (event.source == 1) assert(event.output == 1);
        }
    }
    assert(submits == 1 && fences == 2 && !g_frame_trace.dropped());
    CloseHandle(completed); fence->Release(); native_list->Release(); allocator->Release();
    native_queue->Release(); device->Release(); adapter->Release(); factory->Release();
    std::puts("Boundary probe: ABI checks, already-detoured exports, unchanged prior hook slots/forwarding, observer removal, independent queue setup and live WARP Execute/Signal/Wait passed. No games/NGX.");
}
