// Exercise the production interceptor without loading NGX or starting a game.
#define NR_LIFETIME_TEST
#define NR_EXPERIMENTAL_DX11
#define NR_DX11_GAME_TEST
#define NR_EXPERIMENTAL_VULKAN
#include "../src/neural_resolution_addon.cpp"
#include <cassert>
#include <cstdio>

static HMODULE expected_module;
static LPCSTR expected_name;
static FARPROC expected_result;
static unsigned forwarded;
static FARPROC WINAPI lookup(HMODULE module, LPCSTR name)
{
    assert(module == expected_module && name == expected_name);
    ++forwarded;
    SetLastError(ERROR_PROC_NOT_FOUND);
    return expected_result;
}

static DWORD check_ordinal(unsigned ordinal)
{
    expected_name = MAKEINTRESOURCEA(ordinal);
    __try
    {
        assert(vulkan_native::get_proc_address_dispatch(expected_module, expected_name) == expected_result);
        assert(GetLastError() == ERROR_PROC_NOT_FOUND);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

int main()
{
    using namespace vulkan_native;
    expected_module = GetModuleHandleW(L"kernel32.dll");
    expected_result = GetProcAddress(expected_module, "GetCurrentProcessId");
    assert(expected_module && expected_result);
    original_get_proc_address = &lookup;
    // Reproduces the low-address dereference in the crashing production hook.
    const DWORD exception = check_ordinal(2);
    if (exception) { printf("FAIL: ordinal 2 raised 0x%08lX in production interceptor\n", exception); return 1; }
    for (unsigned ordinal = 0; ordinal <= 0xffff; ++ordinal) assert(check_ordinal(ordinal) == 0);
    expected_result = nullptr;
    assert(check_ordinal(2) == 0);
    expected_name = "NVSDK_NGX_VULKAN_EvaluateFeature";
    assert(get_proc_address_dispatch(expected_module, expected_name) == nullptr);
    expected_result = GetProcAddress(expected_module, "GetCurrentProcessId");
    const char *unchanged[] = {"", "GetCurrentProcessId", "NVSDK_NGX_D3D12_EvaluateFeature", "NVSDK_NGX_VULKAN_EvaluateFeature_extra"};
    for (auto name : unchanged) {
        expected_name = name;
        assert(get_proc_address_dispatch(expected_module, name) == expected_result);
        assert(GetLastError() == ERROR_PROC_NOT_FOUND);
    }
    struct Export { const char *name; FARPROC wrapper; };
    const Export intercepted[] = {
        {"NVSDK_NGX_VULKAN_Init_ProjectID", reinterpret_cast<FARPROC>(&vulkan_init_dispatch)},
        {"NVSDK_NGX_VULKAN_CreateFeature1", reinterpret_cast<FARPROC>(&vulkan_create_dispatch)},
        {"NVSDK_NGX_VULKAN_EvaluateFeature", reinterpret_cast<FARPROC>(&vulkan_evaluate_dispatch)},
        {"NVSDK_NGX_VULKAN_EvaluateFeature_C", reinterpret_cast<FARPROC>(&vulkan_evaluate_c_dispatch)},
        {"NVSDK_NGX_VULKAN_ReleaseFeature", reinterpret_cast<FARPROC>(&vulkan_release_dispatch)}};
    for (auto entry : intercepted) {
        expected_name = entry.name;
        assert(get_proc_address_dispatch(expected_module, entry.name) == entry.wrapper);
    }
    assert(forwarded == 65548);
    original_get_proc_address = &GetProcAddress;
    assert(get_proc_address_dispatch(expected_module, "GetCurrentProcessId") == expected_result);
    puts("PASS: production export interceptor; all 65536 ordinal values, missing exports, named pass-through, five Vulkan wrappers and real Windows lookup. No GPU/NGX.");
}
