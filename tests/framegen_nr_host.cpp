// Controlled native-DX12 NR test through the real FrameGen observer. The FG
// inputs are synthetic; this does NOT generate or certify interpolated frames.
#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <intrin.h>
#include <cmath>
#include <vector>
#include <MinHook.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>
#include "native_capture_trigger.hpp"
#include "../src/neural_pass_controls.hpp"
#include "../src/native_feature_slots.hpp"
static void check(HRESULT hr) { if (FAILED(hr)) { printf("FAIL HRESULT %08lx\n",hr); ExitProcess(2); } }
static void ngx(NVSDK_NGX_Result r) { if (NVSDK_NGX_FAILED(r)) { printf("FAIL NGX %08x\n",r); ExitProcess(3); } }
static std::uintptr_t address(HMODULE module,const char *name) {
    char value[32]={}; if (!GetEnvironmentVariableA(name,value,sizeof(value))) ExitProcess(4);
    const auto rva=std::strtoull(value,nullptr,16);
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    const auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    const auto *nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew);
    if (rva<0x292000 || rva+32>=nt->OptionalHeader.SizeOfImage) ExitProcess(5);
    return base+rva;
}
static unsigned memory_headroom_mib = 0;
static bool recovery_pressure = false;
static unsigned pressure_frame = 0;
// Test-owned process only: keep real SR input fixed and isolate the metadata
// delivered to NR. This models post-SR colour, not a captured Cyberpunk frame.
static unsigned temporal_mode = 0, temporal_passes = 2;
static unsigned temporal_calls = 0;
static bool official_baseline = false, temporal_history = false, temporal_toggle = false;
static ID3D12GraphicsCommandList *temporal_list = nullptr;
static ID3D12Resource *temporal_frozen_color = nullptr;
static std::uint64_t (__fastcall *temporal_original)(void *) = nullptr;
static std::uint64_t __fastcall temporal_evaluate(void *input) {
    ++temporal_calls;
    alignas(16) unsigned char copy[0xB0]={}; std::memcpy(copy,input,0xA5);
    const float jitter[][2]={{-.259979f,-.312103f},{.494904f,.118042f},{.249786f,-.451782f}};
    const float zero[2]={};
    std::memcpy(copy+0x4C,temporal_mode>=2?zero:jitter[pressure_frame%3],8);
    if(temporal_mode==4) {
        const unsigned width=*reinterpret_cast<unsigned *>(copy+0x44),height=*reinterpret_cast<unsigned *>(copy+0x48);
        for(unsigned offset:{0x80u,0x90u}) {
            std::memset(copy+offset,0,8);
            std::memcpy(copy+offset+8,&width,4); std::memcpy(copy+offset+12,&height,4);
        }
        const float scale[]={float(width),float(height)}; std::memcpy(copy+0x54,scale,8);
    }
    if(temporal_mode>=7) copy[0x5D]=1; // Diagnostic only: never a proposed production policy.
    if((temporal_mode==5 || temporal_mode==7) && *reinterpret_cast<unsigned *>(copy+8)==0) {
        auto *color=*reinterpret_cast<ID3D12Resource **>(copy+0x10);
        if(!temporal_frozen_color) {
            ID3D12Device *device=nullptr; check(color->GetDevice(IID_PPV_ARGS(&device)));
            auto desc=color->GetDesc(); D3D12_HEAP_PROPERTIES heap={}; heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
                D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&temporal_frozen_color)));
            device->Release();
            D3D12_RESOURCE_BARRIER barrier={}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition={color,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
            temporal_list->ResourceBarrier(1,&barrier); temporal_list->CopyResource(temporal_frozen_color,color);
            std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter); temporal_list->ResourceBarrier(1,&barrier);
            barrier.Transition={temporal_frozen_color,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
            temporal_list->ResourceBarrier(1,&barrier);
        }
        std::memcpy(copy+0x10,&temporal_frozen_color,8);
    }
    const auto result=temporal_original(copy);
    if((result&255)!=1) { puts("FAIL: temporal NR evaluation failed"); ExitProcess(39); }
    if(temporal_history) {
        auto *base=reinterpret_cast<unsigned char *>(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"));
        const unsigned pass=*reinterpret_cast<unsigned *>(copy+8);
        NativeFeatureSlot slot;
        if(pass==0) {
            slot.parameters=*reinterpret_cast<void **>(base+0x26D820);
            slot.handle=*reinterpret_cast<void **>(base+0x26D828);
        } else {
            auto **bounds=reinterpret_cast<NativeFeatureSlot **>(base+0x26D8D0);
            const auto count=feature_slot_count(reinterpret_cast<std::uintptr_t>(bounds[0]),
                reinterpret_cast<std::uintptr_t>(bounds[1]),reinterpret_cast<std::uintptr_t>(bounds[2]));
            if(count==SIZE_MAX || pass>count) ExitProcess(37);
            slot=bounds[0][pass-1];
        }
        int reset=-1;
        if(!slot.parameters || !slot.handle) ExitProcess(37);
        const auto reset_result=static_cast<NVSDK_NGX_Parameter *>(slot.parameters)->Get("DLSSNR.Reset",&reset);
        if(NVSDK_NGX_FAILED(reset_result)) ExitProcess(37);
        printf("History frame=%u pass=%u handle=%p params=%p command=%p color=%p output=%p input-reset=%u stored-reset=%d get=%x result=%llx\n",
            pressure_frame,pass,slot.handle,slot.parameters,*reinterpret_cast<void **>(copy),
            *reinterpret_cast<void **>(copy+0x10),*reinterpret_cast<void **>(copy+0x18),copy[0x5D],reset,
            unsigned(reset_result),static_cast<unsigned long long>(result));
    }
    return result;
}
static float unpack_channel(unsigned value,unsigned mantissa) {
    const unsigned exponent=value>>mantissa, fraction=value&((1u<<mantissa)-1);
    if(exponent==31) { puts("FAIL: non-finite temporal output"); ExitProcess(36); }
    return exponent ? std::ldexp(1.f+float(fraction)/float(1u<<mantissa),int(exponent)-15)
        : std::ldexp(float(fraction),-14-int(mantissa));
}
// Deterministic admission pressure, not physical VRAM consumption. Only this
// test-owned process replaces its embedded query function before the first NR call.
static bool pressure_query(void *, std::uint64_t &usage, std::uint64_t &budget) {
    if (recovery_pressure && pressure_frame>=8 && pressure_frame<24) {
        budget=7211ull<<20; usage=8774ull<<20; return true;
    }
    if (recovery_pressure && pressure_frame>=24 && pressure_frame<48) {
        budget=14357ull<<20; usage=13116ull<<20; return true;
    }
    budget = 16ull << 30;
    usage = budget - budget / 12 - (std::uint64_t(memory_headroom_mib) << 20);
    return true;
}
static void install_pressure_query(HMODULE module) {
    auto *target = reinterpret_cast<void *>(address(module,"NR_TEST_MEMORY_QUERY_RVA"));
    unsigned char jump[12]={0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0};
    const auto fn = reinterpret_cast<std::uintptr_t>(&pressure_query);
    std::memcpy(jump+2,&fn,sizeof(fn));
    DWORD protection=0, ignored=0;
    if (!VirtualProtect(target,sizeof(jump),PAGE_EXECUTE_READWRITE,&protection)) ExitProcess(23);
    std::memcpy(target,jump,sizeof(jump));
    if (!VirtualProtect(target,sizeof(jump),protection,&ignored)) ExitProcess(23);
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(jump));
}
// Test-owned process only: reproduce a forwarded SR call whose two return
// callbacks observe different application frames. No game/global clock writes.
static NVSDK_NGX_D3D12_DLSS_Eval_Params *nested_eval = nullptr;
static std::uint64_t *nested_frame = nullptr;
static unsigned forwarded_sr(void *command, std::uint64_t feature, void *parameters, void *) {
    const auto result = NGX_D3D12_EVALUATE_DLSS_EXT(static_cast<ID3D12GraphicsCommandList *>(command),
        reinterpret_cast<NVSDK_NGX_Handle *>(feature), static_cast<NVSDK_NGX_Parameter *>(parameters), nested_eval);
    ++*nested_frame;
    return static_cast<unsigned>(result);
}
int main(int argc,char **argv) {
    const bool recovery = argc>1 && !strcmp(argv[1],"recovery");
    char stress_value[16]={};
    const unsigned stress_passes=GetEnvironmentVariableA("NR_TEST_MULTIPASS_STRESS",stress_value,sizeof(stress_value)) ?
        static_cast<unsigned>(std::strtoul(stress_value,nullptr,10)) : 0;
    if (stress_passes && (!recovery || stress_passes<2 || stress_passes>10)) return 20;
    const unsigned width=stress_passes?3440:1920, height=stress_passes?1440:1080;
    char pressure_value[16]={};
    if (GetEnvironmentVariableA("NR_TEST_MEMORY_HEADROOM_MIB",pressure_value,sizeof(pressure_value)))
        memory_headroom_mib=static_cast<unsigned>(std::strtoul(pressure_value,nullptr,10));
    recovery_pressure=GetEnvironmentVariableA("NR_TEST_RECOVERY_PRESSURE",pressure_value,sizeof(pressure_value))!=0;
    if(recovery_pressure) memory_headroom_mib=512;
    const bool transition_stress=memory_headroom_mib!=0;
    const bool pass_toggle=GetEnvironmentVariableA("NR_TEST_PASS_TOGGLE",pressure_value,sizeof(pressure_value))!=0;
    if(GetEnvironmentVariableA("NR_TEST_TEMPORAL_INPUT",pressure_value,sizeof(pressure_value)))
        temporal_mode=static_cast<unsigned>(std::strtoul(pressure_value,nullptr,10));
    if(GetEnvironmentVariableA("NR_TEST_TEMPORAL_PASSES",pressure_value,sizeof(pressure_value)))
        temporal_passes=static_cast<unsigned>(std::strtoul(pressure_value,nullptr,10));
    official_baseline=GetEnvironmentVariableA("NR_TEST_OFFICIAL_BASELINE",pressure_value,sizeof(pressure_value))!=0;
    temporal_history=GetEnvironmentVariableA("NR_TEST_TEMPORAL_HISTORY",pressure_value,sizeof(pressure_value))!=0;
    temporal_toggle=GetEnvironmentVariableA("NR_TEST_TEMPORAL_TOGGLE",pressure_value,sizeof(pressure_value))!=0;
    if((official_baseline || temporal_history || temporal_toggle) && !temporal_mode) return 32;
    if(official_baseline && temporal_mode!=5 && temporal_mode!=6 && temporal_mode!=7) return 32;
    if(temporal_mode && (!recovery || stress_passes!=2 || transition_stress || pass_toggle ||
        temporal_mode>8 || temporal_passes<1 || temporal_passes>2)) return 32;
    const unsigned render_width=temporal_mode?2293:width, render_height=temporal_mode?960:height;
    if(pass_toggle && (!stress_passes || transition_stress)) return 31;
    if (transition_stress && (!stress_passes || memory_headroom_mib>512)) return 24;
    const unsigned frame_count=temporal_mode?90:recovery_pressure?100:transition_stress?80:stress_passes?60:400;
    char churn_value[8]={};
    char trace_value[8]={};
    const bool input_trace=GetEnvironmentVariableA("NR_TEST_INPUT_TRACE",trace_value,sizeof(trace_value))!=0;
    const bool boundary_trace=GetEnvironmentVariableA("NR_TEST_BOUNDARY_TRACE",trace_value,sizeof(trace_value))!=0;
    const bool nested_source=GetEnvironmentVariableA("NR_TEST_NESTED_SOURCE",trace_value,sizeof(trace_value))!=0;
    const bool scale_churn=GetEnvironmentVariableA("NR_TEST_SCALE_CHURN",churn_value,sizeof(churn_value))!=0;
    char mfg_value[8]={};
    const bool mfg_cadence=GetEnvironmentVariableA("NR_TEST_MFG_CADENCE",mfg_value,sizeof(mfg_value))!=0;
    char initial_scale_value[8]={};
    const unsigned initial_scale=GetEnvironmentVariableA("NR_TEST_INITIAL_SCALE",initial_scale_value,sizeof(initial_scale_value)) ?
        static_cast<unsigned>(std::strtoul(initial_scale_value,nullptr,10)) : 0;
    WNDCLASSW cls={}; cls.lpfnWndProc=DefWindowProcW; cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"NRFrameGenFixture";
    RegisterClassW(&cls);
    auto window=CreateWindowW(cls.lpszClassName,L"NR FrameGen callback test",WS_OVERLAPPEDWINDOW,100,100,360,240,nullptr,nullptr,cls.hInstance,nullptr);
    if (!window) return 6;
    ShowWindow(window,temporal_mode?SW_HIDE:SW_SHOWNOACTIVATE);
    IDXGIFactory4 *factory=nullptr; check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ID3D12Device *device=nullptr; check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    ID3D12CommandQueue *queue=nullptr; D3D12_COMMAND_QUEUE_DESC q={}; check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
    DXGI_SWAP_CHAIN_DESC1 sc={}; sc.Width=320; sc.Height=180; sc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sc.SampleDesc.Count=1;
    sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sc.BufferCount=2; sc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *swapchain=nullptr; check(factory->CreateSwapChainForHwnd(queue,window,&sc,nullptr,nullptr,&swapchain));
    wchar_t here[MAX_PATH]={}; GetModuleFileNameW(nullptr,here,MAX_PATH); if (auto *slash=wcsrchr(here,L'\\')) *(slash+1)=0;
    const wchar_t *paths[]={here}; NVSDK_NGX_FeatureCommonInfo ci={}; ci.PathListInfo.Path=paths; ci.PathListInfo.Length=1;
    ngx(NVSDK_NGX_D3D12_Init_with_ProjectID("a7d3f0c8-6b21-4e5a-9f14-3c07b1e9d240",NVSDK_NGX_ENGINE_TYPE_CUSTOM,"1.0",here,device,&ci,NVSDK_NGX_Version_API));
    NVSDK_NGX_Parameter *parameters=nullptr; ngx(NVSDK_NGX_D3D12_AllocateParameters(&parameters));
    ID3D12DescriptorHeap *heap=nullptr; D3D12_DESCRIPTOR_HEAP_DESC hd={}; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors=3;
    check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
    const auto start=heap->GetCPUDescriptorHandleForHeapStart(); const auto stride=device->GetDescriptorHandleIncrementSize(hd.Type);
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R10G10B10A2_UNORM,
        recovery_pressure||pass_toggle||temporal_mode?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R32_FLOAT};
    ID3D12Resource *resources[3]={}; D3D12_HEAP_PROPERTIES memory={}; memory.Type=D3D12_HEAP_TYPE_DEFAULT;
    for (unsigned i=0;i<3;++i) {
        D3D12_RESOURCE_DESC desc={}; desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width=render_width; desc.Height=render_height;
        if(temporal_mode==4 && i!=0) {desc.Width=width; desc.Height=height;}
        desc.DepthOrArraySize=desc.MipLevels=1; desc.Format=formats[i]; desc.SampleDesc.Count=1;
        desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        check(device->CreateCommittedResource(&memory,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&resources[i])));
        device->CreateRenderTargetView(resources[i],nullptr,{start.ptr+i*stride});
    }
    parameters->Set("DLSSG.Backbuffer",resources[0]); parameters->Set("DLSSG.HUDLess",static_cast<ID3D12Resource *>(nullptr));
    parameters->Set("DLSSG.MVecs",resources[1]); parameters->Set("DLSSG.Depth",resources[2]);
    parameters->Set("DLSSG.MultiFrameIndex",1u);
    ID3D12CommandAllocator *allocator=nullptr; check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ID3D12GraphicsCommandList *list=nullptr; check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,nullptr,IID_PPV_ARGS(&list))); check(list->Close());
    ID3D12Fence *fence=nullptr; check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    ID3D12Resource *sr_output=nullptr;
    NVSDK_NGX_Handle *sr_handle=nullptr;
    NVSDK_NGX_Parameter *sr_parameters=nullptr;
    if (recovery) {
        auto desc=resources[0]->GetDesc(); desc.Width=width; desc.Height=height;
        desc.Format=transition_stress||pass_toggle||temporal_mode ? DXGI_FORMAT_R11G11B10_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        check(device->CreateCommittedResource(&memory,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&sr_output)));
        ngx(NVSDK_NGX_D3D12_AllocateParameters(&sr_parameters));
    }
    ID3D12CommandAllocator *allocators[3]={allocator};
    ID3D12GraphicsCommandList *lists[3]={list};
    ID3D12Resource *sr_outputs[3]={sr_output};
    if (stress_passes) for (unsigned i=1;i<3;++i) {
        check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators[i])));
        check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[i],nullptr,IID_PPV_ARGS(&lists[i])));
        check(lists[i]->Close());
        const auto desc=sr_output->GetDesc();
        check(device->CreateCommittedResource(&memory,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&sr_outputs[i])));
    }
    auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    ID3D12Resource *readback=nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};
    if (recovery_pressure || temporal_mode) {
        const auto desc=sr_output->GetDesc(); UINT64 bytes=0;
        device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
        D3D12_HEAP_PROPERTIES read_heap={}; read_heap.Type=D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer={}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width=bytes; buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
        buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        check(device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,&buffer,
            D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    }
    std::uint64_t baseline_pixels=0; unsigned image_comparisons=0;
    std::vector<float> previous_pixels;
    double temporal_error=0; unsigned temporal_pairs=0;
    if (input_trace) printf("Input trace fixture: SR=%llx FG=%llx motion=%llx depth=%llx\n",
        reinterpret_cast<unsigned long long>(sr_output), reinterpret_cast<unsigned long long>(resources[0]),
        reinterpret_cast<unsigned long long>(resources[1]), reinterpret_cast<unsigned long long>(resources[2]));
    unsigned evaluated=0, native_recovered=0, late_fg_evals=0, healthy_fg_evals=0, manual_fg_evals=0;
    unsigned scaled_begin=0, fg_scaled_begin=0, transition_begin=0, prewarm_begin=0, retired_begin=0, manual_fg_scaled=0; unsigned long long bypass_begin=0;
    bool counters_initialized=false, initial_scale_applied=initial_scale==0;
    bool pressure_installed=false;
    unsigned drain_frames=0, deferred_frames=0;
    unsigned mfg_callbacks[5]={};
    for (unsigned frame=0;frame<frame_count;++frame) {
        const unsigned current_passes=temporal_mode?(temporal_toggle?(frame>=30 && frame<60?2u:1u):temporal_passes):pass_toggle && frame>=20 && frame<40 ? 1u : stress_passes;
        pressure_frame=frame;
        MSG msg; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (stress_passes) {
            if (frame>=3) {
                check(fence->SetEventOnCompletion(frame-2,event));
                if (WaitForSingleObject(event,10000)!=WAIT_OBJECT_0) return 8;
            }
            allocator=allocators[frame%3]; list=lists[frame%3];
            sr_output=sr_outputs[temporal_mode || (recovery_pressure && frame<8)?0:frame%3];
        }
        check(allocator->Reset()); check(list->Reset(allocator,nullptr));
        temporal_list=list;
        for (unsigned i=0;i<3;++i) {
            D3D12_RESOURCE_BARRIER b={}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource=resources[i];
            b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; b.Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
            list->ResourceBarrier(1,&b);
            const float colors[3][4]={{.5f,.4f,.3f,1.f},{0,0,0,0},{1,1,1,1}};
            list->ClearRenderTargetView({start.ptr+i*stride},colors[i],0,nullptr);
            if(temporal_mode && i==2) {
                const float plane[4]={.5f,.5f,.5f,.5f};
                list->ClearRenderTargetView({start.ptr+i*stride},plane,0,nullptr);
            }
            if (recovery_pressure && i==0) for (unsigned y=0;y<height;y+=64) for(unsigned x=0;x<width;x+=64) {
                const float tile[4]={.15f+.1f*((x/64+y/64)%2),.25f+.03f*((x/64)%5),.4f,1.f};
                const D3D12_RECT rect={LONG(x),LONG(y),LONG((std::min)(x+64,width)),LONG((std::min)(y+64,height))};
                list->ClearRenderTargetView(start,tile,1,&rect);
            }
            if(temporal_mode && i==0) for(unsigned y=0;y<render_height;y+=16) for(unsigned x=0;x<render_width;x+=16) {
                const float tile[4]={.15f+.35f*((x/16+y/16)%2),.2f+.05f*((x/16)%5),.35f,1.f};
                const D3D12_RECT rect={LONG(x),LONG(y),LONG((std::min)(x+16,render_width)),LONG((std::min)(y+16,render_height))};
                list->ClearRenderTargetView(start,tile,1,&rect);
            }
            b.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; list->ResourceBarrier(1,&b);
        }
        auto module=GetModuleHandleW(L"renodx-dlss5-super-anus.addon64");
        if (module) {
            if(temporal_mode && !temporal_original) {
                auto *target=reinterpret_cast<unsigned char *>(module)+0x5D880;
                const unsigned char expected[]={0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53};
                if(std::memcmp(target,expected,sizeof(expected))!=0) return 35;
                if(MH_Initialize()!=MH_OK || MH_CreateHook(target,
                    reinterpret_cast<void *>(&temporal_evaluate),reinterpret_cast<void **>(&temporal_original))!=MH_OK ||
                    MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) return 33;
                if(!official_baseline) {
                reinterpret_cast<std::atomic_int *>(address(module,"NR_TEST_SHARPNESS_RVA"))->store(0);
                reinterpret_cast<std::atomic_int *>(address(module,"NR_TEST_COLOR_RVA"))->store(50);
                *reinterpret_cast<nr::PassControls *>(address(module,"NR_TEST_PASSES_RVA"))={};
                if(temporal_mode>=5) {
                    reinterpret_cast<std::atomic_int *>(address(module,"NR_TEST_TRANSFER_RVA"))->store(100);
                    reinterpret_cast<std::atomic_int *>(address(module,"NR_TEST_COLOR_RVA"))->store(100);
                    reinterpret_cast<nr::PassControls *>(address(module,"NR_TEST_PASSES_RVA"))->extra[0].color=100;
                }
                if(temporal_mode==3) {
                    reinterpret_cast<std::atomic_int *>(address(module,"NR_TEST_TRANSFER_RVA"))->store(0);
                    reinterpret_cast<nr::PassControls *>(address(module,"NR_TEST_PASSES_RVA"))->extra[0].transfer=0;
                }
                }
            }
            if(official_baseline) {
                // Unmodified official image, same SR feed; select its existing
                // Upscaled hook using the pinned profile's settings, not our routes.
                auto *base=reinterpret_cast<unsigned char *>(module);
                base[0x27100C]=0; base[0x27100E]=1; base[0x27100F]=2;
                *reinterpret_cast<float *>(base+0x270FB0)=2.f;
                *reinterpret_cast<unsigned *>(base+0x266FA4)=current_passes;
                *reinterpret_cast<int *>(base+0x2677F8)=1;
                if(!sr_handle) {
                    NVSDK_NGX_DLSS_Create_Params create={};
                    create.Feature={render_width,render_height,width,height,NVSDK_NGX_PerfQuality_Value_MaxQuality};
                    create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes|NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
                    ngx(NGX_D3D12_CREATE_DLSS_EXT(list,1,1,&sr_handle,sr_parameters,&create));
                }
                NVSDK_NGX_D3D12_DLSS_Eval_Params eval={};
                eval.Feature.pInColor=resources[0]; eval.Feature.pInOutput=sr_output;
                eval.pInDepth=resources[2]; eval.pInMotionVectors=resources[1];
                eval.InRenderSubrectDimensions={render_width,render_height}; eval.InReset=frame==0;
                eval.InMVScaleX=float(render_width); eval.InMVScaleY=float(render_height);
                const auto before=temporal_calls;
                ngx(NGX_D3D12_EVALUATE_DLSS_EXT(list,sr_handle,sr_parameters,&eval));
                if(temporal_calls-before!=current_passes) { printf("FAIL official frame=%u calls=%u expected=%u\n",frame,temporal_calls-before,current_passes); return 38; }
                evaluated+=temporal_calls-before;
            } else {
            if (transition_stress && !pressure_installed) {
                install_pressure_query(module); pressure_installed=true;
            }
            const unsigned transition_scales[]={100,70,101,100};
            if (transition_stress && !recovery_pressure && frame%20==0)
                reinterpret_cast<void (*)(int)>(address(module,"NR_TEST_SCALE_RVA"))(transition_scales[frame/20]);
            if (boundary_trace && input_trace && frame == 10)
                reinterpret_cast<std::atomic_bool *>(address(module,"NR_TEST_TRACE_REQUEST_RVA"))->store(true);
            if (!initial_scale_applied) {
                reinterpret_cast<void (*)(int)>(address(module,"NR_TEST_SCALE_RVA"))(initial_scale);
                initial_scale_applied=true;
            }
            // Test-owned controls, after recovery without any scale/epoch reset.
            // Change one knob on one pass and read the actual final SR texture.
            if (recovery_pressure && frame>=70 && frame<94) {
                const unsigned test=(frame-70)/4, pass=test/3, knob=test%3;
                const bool changed=(frame-70)%4>=2;
                auto *extra=reinterpret_cast<nr::PassControls *>(address(module,"NR_TEST_PASSES_RVA"));
                *extra={};
                int values[]={100,50,0};
                const int edited[]={0,0,100};
                if(changed && pass==0) values[knob]=edited[knob];
                const char *keys[]={"NR_TEST_TRANSFER_RVA","NR_TEST_COLOR_RVA","NR_TEST_SHARPNESS_RVA"};
                for(unsigned k=0;k<3;++k) reinterpret_cast<std::atomic_int *>(address(module,keys[k]))->store(values[k]);
                if(changed && pass==1) {
                    if(knob==0) extra->extra[0].transfer=edited[knob];
                    if(knob==1) extra->extra[0].color=edited[knob];
                    if(knob==2) extra->extra[0].sharpness=edited[knob];
                }
            }
            auto *success=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_SUCCESS_RVA"));
            auto *scaled=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_SCALED_RVA"));
            auto *fg_scaled=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_FG_SCALED_RVA"));
            auto *fg_bypass=reinterpret_cast<std::atomic_ullong *>(address(module,"NR_FINAL_FG_BYPASS_RVA"));
            auto *transition_native=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_TRANSITION_NATIVE_RVA"));
            if (!counters_initialized) {
                scaled_begin=scaled->load(); fg_scaled_begin=fg_scaled->load();
                transition_begin=transition_native->load(); bypass_begin=fg_bypass->load(); counters_initialized=true;
                prewarm_begin=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_PREWARM_RVA"))->load();
                retired_begin=reinterpret_cast<std::atomic_uint *>(address(module,"NR_FINAL_RETIRED_RVA"))->load();
            }
            const auto before=success->load();
            const auto before_scaled=scaled->load();
            using Callback=std::uint64_t (*)(void *,std::uint64_t,void *);
            auto callback=reinterpret_cast<Callback>(address(module,"NR_FG_CALLBACK_RVA"));
            if (recovery) {
                // Test-owned process ONLY: simulate Auto's stale FG-active flag.
                // Production preset transactions still control passes and NR off.
                auto *base=reinterpret_cast<unsigned char *>(module);
                const bool manual_fg=stress_passes || (frame>=290 && frame<310);
                const bool manual_sr=frame>=310 && frame<320;
                base[0x27100C]=(manual_fg || manual_sr)?0:1;
                *reinterpret_cast<float *>(base+0x270FB0)=manual_fg?3.f:manual_sr?2.f:1.f;
                base[0x27100E]=(frame>=200 && frame<220)?0:1;
                if (*reinterpret_cast<int *>(base+0x2677F8)!=0) base[0x27100F]=manual_sr?2:3;
                if (manual_fg) *reinterpret_cast<unsigned *>(base+0x266FA4)=2;
                else if (frame==310) *reinterpret_cast<unsigned *>(base+0x266FA4)=1;
                if (stress_passes) {
                    *reinterpret_cast<unsigned *>(base+0x266FA4)=current_passes;
                    *reinterpret_cast<int *>(base+0x2677F8)=1;
                }
                if (!sr_handle) {
                    NVSDK_NGX_DLSS_Create_Params create={};
                    create.Feature.InWidth=create.Feature.InTargetWidth=width;
                    create.Feature.InHeight=create.Feature.InTargetHeight=height;
                    if(temporal_mode) {
                        create.Feature.InWidth=render_width; create.Feature.InHeight=render_height;
                    }
                    create.Feature.InPerfQualityValue=temporal_mode?NVSDK_NGX_PerfQuality_Value_MaxQuality:NVSDK_NGX_PerfQuality_Value_DLAA;
                    create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
                    if(temporal_mode) create.InFeatureCreateFlags|=NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
                    ngx(NGX_D3D12_CREATE_DLSS_EXT(list,1,1,&sr_handle,sr_parameters,&create));
                    if (input_trace) {
                        printf("Input trace SR identity: feature=%llx params=%llx\n",
                            reinterpret_cast<unsigned long long>(sr_handle), reinterpret_cast<unsigned long long>(sr_parameters));
                        // Reproduce shared/stale FG keys on an actual SR feature.
                        // Diagnostics must not classify this handle as FG by keys alone.
                        sr_parameters->Set("DLSSG.Backbuffer",resources[0]);
                        sr_parameters->Set("DLSSG.HUDLess",static_cast<ID3D12Resource *>(nullptr));
                        sr_parameters->Set("DLSSG.MVecs",resources[1]);
                        sr_parameters->Set("DLSSG.Depth",resources[2]);
                        sr_parameters->Set("DLSSG.MultiFrameIndex",3u);
                    }
                }
                const auto before_fg_route=fg_scaled->load();
                NVSDK_NGX_D3D12_DLSS_Eval_Params eval={};
                eval.Feature.pInColor=resources[0]; eval.Feature.pInOutput=sr_output;
                eval.pInDepth=resources[2]; eval.pInMotionVectors=resources[1];
                eval.InRenderSubrectDimensions={width,height}; eval.InReset=frame==0;
                if(temporal_mode) {
                    eval.InRenderSubrectDimensions={render_width,render_height};
                    eval.InMVScaleX=float(render_width); eval.InMVScaleY=float(render_height);
                }
                if (nested_source) {
                    // Verified base profile: EC910 reads the static TLS frame
                    // override at +478 first; 4B140 is the common dispatcher.
                    const auto tls_index=*reinterpret_cast<unsigned *>(base+0x26A264);
                    auto **tls=reinterpret_cast<unsigned char **>(__readgsqword(0x58));
                    nested_frame=reinterpret_cast<std::uint64_t *>(tls[tls_index]+0x478);
                    const auto saved_frame=*nested_frame;
                    *nested_frame=1000000+frame*4;
                    nested_eval=&eval;
                    using Dispatch=unsigned (*)(decltype(&forwarded_sr),void *,std::uint64_t,void *,void *);
                    const auto result=reinterpret_cast<Dispatch>(base+0x4B140)(forwarded_sr,list,
                        reinterpret_cast<std::uint64_t>(sr_handle),sr_parameters,nullptr);
                    *nested_frame=saved_frame;
                    ngx(static_cast<NVSDK_NGX_Result>(result));
                } else ngx(NGX_D3D12_EVALUATE_DLSS_EXT(list,sr_handle,sr_parameters,&eval));
                const auto native_delta=success->load()-before;
                native_recovered+=native_delta;
                if (frame>=180 || nested_source) {
                    const auto enabled=*reinterpret_cast<int *>(base+0x2677F8)!=0;
                    const auto expected=enabled ? *reinterpret_cast<unsigned *>(base+0x266FA4) : 0;
                    if (native_delta!=expected) { printf("FAIL: frame %u native passes %u expected %u\n",frame,native_delta,expected); return 11; }
                }
                if (manual_fg) manual_fg_scaled+=fg_scaled->load()-before_fg_route;
                // FrameGen callbacks remain NR-free. The 20-frame manual window
                // issues 10,000 callbacks while two-pass/scale transitions run
                // on the upstream native-SR path.
                if (frame<40 || frame>=180) {
                    const auto after_native=success->load();
                    const unsigned repeats=manual_fg?500u:1u;
                    for (unsigned repeat=0;repeat<repeats;++repeat) {
                        const unsigned index=repeat%4+1;
                        parameters->Set("DLSSG.MultiFrameIndex",index);
                        if ((callback(list,0x12345678,parameters)&255)!=1 || success->load()!=after_native)
                            { puts("FAIL: FrameGen callback performed NR work"); return 12; }
                        ID3D12Resource *color=nullptr,*motion=nullptr,*depth=nullptr;
                        unsigned actual_index=0;
                        parameters->Get("DLSSG.Backbuffer",&color);
                        parameters->Get("DLSSG.MVecs",&motion);
                        parameters->Get("DLSSG.Depth",&depth);
                        parameters->Get("DLSSG.MultiFrameIndex",&actual_index);
                        if (color!=resources[0] || motion!=resources[1] || depth!=resources[2] || actual_index!=index)
                            { puts("FAIL: FrameGen callback changed vendor parameters"); return 13; }
                    }
                    const auto extra=success->load()-after_native;
                    if (frame<40) healthy_fg_evals+=extra;
                    else if (manual_fg) {
                        manual_fg_evals+=extra;
                    }
                    else late_fg_evals+=extra;
                }
            } else {
                const unsigned generated=mfg_cadence ? (frame<134?2u:(frame<267?3u:4u)) : 1u;
                for (unsigned index=1;index<=generated;++index) {
                    parameters->Set("DLSSG.MultiFrameIndex",index);
                    const auto result=callback(list,0x12345678,parameters);
                    if ((result&255)!=1) return 7;
                    ++mfg_callbacks[index];
                }
            }
            evaluated+=success->load()-before;
            const bool draining=stress_passes && reinterpret_cast<std::atomic_uint *>(address(module,"NR_TEST_QUIESCE_RVA"))->load()!=0;
            drain_frames=draining ? drain_frames+1 : 0;
            if (draining) ++deferred_frames;
            if (drain_frames>12) { puts("FAIL: compact admission fence drain did not complete within 12 frames"); return 26; }
            const unsigned expected_resolves=temporal_mode>=5?current_passes-1:current_passes;
            if (stress_passes && !draining && (recovery_pressure ? frame>=70 : transition_stress||pass_toggle ? frame%20>=8 : temporal_toggle ? frame%30>=8 : frame>=8) && scaled->load()-before_scaled!=expected_resolves) {
                printf("FAIL: sustained resolve frame=%u adjusted=%u expected=%u\n",frame,scaled->load()-before_scaled,expected_resolves);
                return 22;
            }
            if(recovery_pressure && frame>=70 && success->load()-before!=stress_passes) {
                puts("FAIL: recovered NR evaluations missing"); return 29;
            }
            if (transition_stress && !recovery_pressure && !draining && frame%20>=8) {
                const unsigned requested=transition_scales[frame/20];
                const unsigned effective=reinterpret_cast<std::atomic_uint *>(address(module,"NR_TEST_EFFECTIVE_SCALE_RVA"))->load();
                const unsigned expected=memory_headroom_mib==90 && requested>100 ? 100 : requested;
                if(effective!=expected) { printf("FAIL: requested=%u effective=%u expected=%u\n",requested,effective,expected); return 25; }
            }
            if (frame%30==0) printf("frame=%u NR-evaluations=%u\n",frame,evaluated);
            }
        }
        const bool capture_pixels=temporal_mode?frame>=30:recovery_pressure && frame>=70 && frame<94 && frame%2==1;
        if(capture_pixels) {
            D3D12_RESOURCE_BARRIER b={}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition={sr_output,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
            list->ResourceBarrier(1,&b);
            D3D12_TEXTURE_COPY_LOCATION from={}; from.pResource=sr_output; from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            D3D12_TEXTURE_COPY_LOCATION to={}; to.pResource=readback; to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint=footprint;
            list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
            std::swap(b.Transition.StateBefore,b.Transition.StateAfter); list->ResourceBarrier(1,&b);
        }
        check(list->Close()); ID3D12CommandList *submit[]={list}; queue->ExecuteCommandLists(1,submit);
        check(swapchain->Present(0,0)); check(queue->Signal(fence,frame+1));
        if(capture_pixels) {
            check(fence->SetEventOnCompletion(frame+1,event));
            if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0) return 8;
            unsigned char *pixels=nullptr; check(readback->Map(0,nullptr,reinterpret_cast<void **>(&pixels)));
            if(temporal_mode) {
                std::vector<float> current; current.reserve(width*height*3/16);
                for(unsigned y=2;y<height;y+=4) for(unsigned x=2;x<width;x+=4) {
                    const auto packed=*reinterpret_cast<const unsigned *>(pixels+footprint.Offset+y*footprint.Footprint.RowPitch+x*4);
                    current.push_back(unpack_channel(packed&2047,6));
                    current.push_back(unpack_channel((packed>>11)&2047,6));
                    current.push_back(unpack_channel(packed>>22,5));
                }
                if(!previous_pixels.empty()) {
                    double delta=0;
                    for(std::size_t p=0;p<current.size();++p) delta+=std::abs(current[p]-previous_pixels[p]);
                    delta/=double(current.size()); temporal_error+=delta; ++temporal_pairs;
                    printf("Temporal frame=%u jitter-mode=%u passes=%u mean-absolute-delta=%.9f\n",frame,temporal_mode,current_passes,delta);
                }
                previous_pixels=std::move(current);
            }
            std::uint64_t hash=14695981039346656037ull;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width*4;++x)
                hash=(hash^pixels[footprint.Offset+y*footprint.Footprint.RowPitch+x])*1099511628211ull;
            if(temporal_mode) printf("Temporal pixels frame=%u fnv64=%llx\n",frame,static_cast<unsigned long long>(hash));
            D3D12_RANGE empty={0,0}; readback->Unmap(0,&empty);
            if(!temporal_mode && (frame-70)%4==1) baseline_pixels=hash;
            else if(!temporal_mode) {
                printf("Recovered final image: pass=%u knob=%u baseline=%llx changed=%llx\n",(frame-70)/12+1,((frame-70)/4)%3,baseline_pixels,hash);
                if(hash==baseline_pixels) { puts("FAIL: recovered final output unchanged"); return 27; }
                ++image_comparisons;
            }
        }
        if (!stress_passes) {
            check(fence->SetEventOnCompletion(frame+1,event));
            if (WaitForSingleObject(event,10000)!=WAIT_OBJECT_0) return 8;
            NativeCaptureTestTrigger(frame+1);
        }
        if (scale_churn && (frame+1)%45==0) {
            const unsigned scales[]={50,75,99,100,101,125,150,75};
            const auto index=(frame+1)/45-1;
            if (module && index<8) {
                reinterpret_cast<void (*)(int)>(address(module,"NR_TEST_SCALE_RVA"))(scales[index]);
                printf("TEST ONLY: applied NR scale %u%% at frame %u\n",scales[index],frame+1);
            }
        }
        if (!stress_passes || recovery_pressure) Sleep(recovery_pressure?20:10);
    }
    check(fence->SetEventOnCompletion(frame_count,event));
    if (WaitForSingleObject(event,10000)!=WAIT_OBJECT_0) return 8;
    printf("FrameGen callback fixture: actual NR evaluations=%u; synthetic FG parameters, no generated frames.\n",evaluated);
    if (input_trace) {
        auto module=GetModuleHandleW(L"renodx-dlss5-super-anus.addon64");
        auto *status=reinterpret_cast<std::atomic_uint *>(address(module,"NR_TEST_TRACE_STATUS_RVA"));
        const auto deadline=GetTickCount64()+30000;
        while (status->load()!=0 && GetTickCount64()<deadline) {
            reinterpret_cast<void (*)()>(address(module,"NR_TEST_TRACE_PUMP_RVA"))();
            Sleep(25);
        }
        if (status->load()!=0) { puts("FAIL: input trace did not drain"); return 14; }
    }
    const unsigned scaled_total=counters_initialized ? reinterpret_cast<std::atomic_uint *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_SCALED_RVA"))->load()-scaled_begin : 0;
    const unsigned fg_scaled_total=counters_initialized ? reinterpret_cast<std::atomic_uint *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_FG_SCALED_RVA"))->load()-fg_scaled_begin : 0;
    const unsigned transition_total=counters_initialized ? reinterpret_cast<std::atomic_uint *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_TRANSITION_NATIVE_RVA"))->load()-transition_begin : 0;
    const unsigned long long bypass_total=counters_initialized ? reinterpret_cast<std::atomic_ullong *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_FG_BYPASS_RVA"))->load()-bypass_begin : 0;
    const unsigned prewarm_total=counters_initialized ? reinterpret_cast<std::atomic_uint *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_PREWARM_RVA"))->load()-prewarm_begin : 0;
    const unsigned retired_total=counters_initialized ? reinterpret_cast<std::atomic_uint *>(address(GetModuleHandleW(L"renodx-dlss5-super-anus.addon64"),"NR_FINAL_RETIRED_RVA"))->load()-retired_begin : 0;
    printf("Scale routing: scaled=%u FrameGen-upstream=%u transition-native=%u FrameGen-bypassed=%llu.\n",scaled_total,fg_scaled_total,transition_total,bypass_total);
    printf("Resource churn: prewarmed=%u retired=%u.\n",prewarm_total,retired_total);
    if (mfg_cadence) printf("MFG cadence callbacks: index1=%u index2=%u index3=%u index4=%u.\n",
        mfg_callbacks[1],mfg_callbacks[2],mfg_callbacks[3],mfg_callbacks[4]);
    if (recovery) printf("Upstream routing: native NR=%u healthy FG callback NR=%u late FG callback NR=%u manual FG callback NR=%u redirected=%u\n",native_recovered,healthy_fg_evals,late_fg_evals,manual_fg_evals,manual_fg_scaled);
    if (sr_handle) ngx(NVSDK_NGX_D3D12_ReleaseFeature(sr_handle));
    if (sr_parameters) ngx(NVSDK_NGX_D3D12_DestroyParameters(sr_parameters));
    ngx(NVSDK_NGX_D3D12_DestroyParameters(parameters)); ngx(NVSDK_NGX_D3D12_Shutdown1(device));
    CloseHandle(event); fence->Release();
    if(readback) readback->Release();
    for (unsigned i=0;i<(stress_passes?3u:1u);++i) {
        lists[i]->Release(); allocators[i]->Release();
        if (sr_outputs[i]) sr_outputs[i]->Release();
    }
    for (auto *resource:resources) resource->Release();
    if(temporal_mode) { MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); }
    if(temporal_frozen_color) temporal_frozen_color->Release();
    heap->Release(); swapchain->Release(); queue->Release(); device->Release(); factory->Release(); DestroyWindow(window);
    if(temporal_mode) {
        printf("Temporal summary: mode=%u passes=%u pairs=%u mean-absolute-delta=%.9f NR=%u resolves=%u shim-calls=%u. Synthetic static post-SR scene, not game acceptance.\n",
            temporal_mode,temporal_passes,temporal_pairs,temporal_pairs?temporal_error/temporal_pairs:0,evaluated,scaled_total,temporal_calls);
        return temporal_pairs==59 && temporal_calls==evaluated && evaluated==(temporal_toggle?120:frame_count*temporal_passes) &&
            (official_baseline || scaled_total>=(temporal_toggle?22:(frame_count-4)*(temporal_mode>=5?temporal_passes-1:temporal_passes))) ? 0 : 34;
    }
    if (stress_passes) {
        if(recovery_pressure && image_comparisons!=6) return 28;
        const unsigned minimum_resolves=pass_toggle ? 12*(stress_passes*2+1) : (recovery_pressure ? 30u : transition_stress ? frame_count/2 : frame_count-4-(deferred_frames>12?12:deferred_frames))*stress_passes;
        printf("Multipass stress: %ux%u passes=%u frames=%u rotating-outputs=3 in-flight=3 resolved=%u expected>=%u\n",
            width,height,stress_passes,frame_count,scaled_total,minimum_resolves);
        // Pressure intentionally suppresses unsafe later passes. Every settled
        // recovery frame above still requires all NR evaluations AND resolves.
        const bool evaluation_count_ok=recovery_pressure ?
            evaluated>=frame_count+30*(stress_passes-1) && evaluated<=frame_count*stress_passes :
            evaluated==(pass_toggle ? 40*stress_passes+20 : frame_count*stress_passes);
        return scaled_total>=minimum_resolves && evaluation_count_ok ? 0 : 21;
    }
    const bool scale_ok=!scale_churn || (scaled_total>0 && fg_scaled_total>0 && transition_total>0);
    const bool native_transparent_ok=recovery || scale_churn || bypass_total>=370;
    const bool scaled_activity_ok=!scale_churn || evaluated>0;
    const bool manual_route_ok=!recovery || (manual_fg_scaled>0 && manual_fg_evals==0 && bypass_total>=10000);
    const bool recovery_ok=!recovery || (native_recovered>=370 && healthy_fg_evals==0 && late_fg_evals==0 && manual_route_ok);
    const bool cadence_ok=!mfg_cadence || (mfg_callbacks[1]&&mfg_callbacks[2]&&mfg_callbacks[3]&&mfg_callbacks[4]);
    return native_transparent_ok && scaled_activity_ok && scale_ok && recovery_ok && cadence_ok ? 0 : 9;
}
