@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI\"
set "NR_OUTPUT=%ROOT%build\dx11-experimental.addon64"
set "NR_PROBE_DEFINE="
set "NR_VALIDATION_FLAGS="
set "NR_PATCH_FLAGS="
set "NR_TEST_DEFINE="
set "NR_VULKAN_INCLUDE="
if /i "%~1"=="game-test" (
 set "NR_OUTPUT=%ROOT%build\dx11-integrated-game-test.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST"
 set "NR_VALIDATION_FLAGS=--dx11-game-test"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
)
if /i "%~1"=="dawnwalker-no-copyback" (
 set "NR_OUTPUT=%ROOT%build\dawnwalker-no-copyback.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_DAWNWALKER_NO_COPYBACK_TEST"
 set "NR_VALIDATION_FLAGS=--dx11-game-test"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
)
if /i "%~1"=="vulkan" (
 set "NR_OUTPUT=%ROOT%build\vulkan-native-1\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\vulkan-native-1" mkdir "%ROOT%build\vulkan-native-1" || exit /b 1
)
if /i "%~1"=="framegen-input-trace" (
 set "NR_OUTPUT=%ROOT%build\dx11-integrated-game-test.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_FRAMEGEN_INPUT_TRACE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --framegen-input-trace"
 set "NR_PATCH_FLAGS=--framegen-input-trace"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
)
if /i "%~1"=="recycle-probe" (
 set "NR_OUTPUT=%ROOT%build\dx11-recycle-probe.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_RECYCLE_PROBE"
)
if /i "%~1"=="framegen-boundary-trace" (
 set "NR_OUTPUT=%ROOT%build\tlou2-boundary-trace-2\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_FRAMEGEN_INPUT_TRACE /DNR_FRAMEGEN_BOUNDARY_TRACE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --framegen-input-trace"
 set "NR_PATCH_FLAGS=--framegen-input-trace"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\tlou2-boundary-trace-2" mkdir "%ROOT%build\tlou2-boundary-trace-2" || exit /b 1
)
if /i "%~1"=="replacement-probe" (
 set "NR_OUTPUT=%ROOT%build\dx11-replacement-probe.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_REPLACEMENT_PROBE"
)
if /i "%~1"=="nested-source" (
 set "NR_OUTPUT=%ROOT%build\tlou2-nested-source-1\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_FRAMEGEN_INPUT_TRACE /DNR_FRAMEGEN_BOUNDARY_TRACE /DNR_NESTED_SOURCE_GUARD"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --framegen-input-trace"
 set "NR_PATCH_FLAGS=--framegen-input-trace"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\tlou2-nested-source-1" mkdir "%ROOT%build\tlou2-nested-source-1" || exit /b 1
)
if /i "%~1"=="core-shutdown-probe" (
 set "NR_OUTPUT=%ROOT%build\dx11-core-shutdown-probe.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_CORE_SHUTDOWN_PROBE"
)
if /i "%~1"=="manager-release" (
 set "NR_OUTPUT=%ROOT%build\manager-1.0.2-addon\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_INTEGRATED_RELEASE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --integrated-release"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\manager-1.0.2-addon" mkdir "%ROOT%build\manager-1.0.2-addon" || exit /b 1
)
if /i "%~1"=="core-recreate-probe" (
 set "NR_OUTPUT=%ROOT%build\dx11-core-recreate-probe.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_CORE_SHUTDOWN_PROBE /DNR_DX11_CORE_RECREATE_PROBE"
)
if /i "%~1"=="pass-controls" (
 set "NR_OUTPUT=%ROOT%build\pass-controls-9\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_PASS_CONTROLS_PREVIEW"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --pass-controls-preview"
 set "NR_PATCH_FLAGS=--addon-build 9"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\pass-controls-9" mkdir "%ROOT%build\pass-controls-9" || exit /b 1
)
if /i "%~1"=="pass-input-trace" (
 set "NR_OUTPUT=%ROOT%build\cp-pass-input-trace-1\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_PASS_CONTROLS_PREVIEW /DNR_PASS_INPUT_TRACE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --pass-controls-preview"
 set "NR_PATCH_FLAGS=--addon-build 10"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\cp-pass-input-trace-1" mkdir "%ROOT%build\cp-pass-input-trace-1" || exit /b 1
)
if /i "%~1"=="pass-history-fix" (
 set "NR_OUTPUT=%ROOT%build\cp-pass-history-fix-1\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_PASS_CONTROLS_PREVIEW /DNR_PASS_HISTORY_FIX"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --pass-controls-preview"
 set "NR_PATCH_FLAGS=--addon-build 11"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\cp-pass-history-fix-1" mkdir "%ROOT%build\cp-pass-history-fix-1" || exit /b 1
)
if /i "%~1"=="manager-release-1.0.3" (
 set "NR_OUTPUT=%ROOT%build\manager-1.0.3-addon\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_PASS_CONTROLS_RELEASE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --pass-controls-release"
 set "NR_PATCH_FLAGS=--addon-build 12 --release-version"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\manager-1.0.3-addon" mkdir "%ROOT%build\manager-1.0.3-addon" || exit /b 1
)
if /i "%~1"=="manager-release-1.0.5" (
 set "NR_OUTPUT=%ROOT%build\manager-1.0.5-addon\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_MOTION_RUNTIME_RELEASE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --motion-runtime-release --addon-version 1.0.5"
 set "NR_PATCH_FLAGS=--addon-build 16 --addon-version 1.0.5 --release-version"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\manager-1.0.5-addon" mkdir "%ROOT%build\manager-1.0.5-addon" || exit /b 1
)
if /i "%~1"=="manager-release-1.0.6" (
 set "NR_OUTPUT=%ROOT%build\manager-1.0.6-addon\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_SLIDER_RESET_RELEASE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --slider-reset-release --addon-version 1.0.6"
 set "NR_PATCH_FLAGS=--addon-build 17 --addon-version 1.0.6 --release-version"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\manager-1.0.6-addon" mkdir "%ROOT%build\manager-1.0.6-addon" || exit /b 1
)
if /i "%~1"=="manager-release-1.0.8" (
 set "NR_OUTPUT=%ROOT%build\manager-1.0.8-addon\renodx-dlss5-super-anus.addon64"
 set "NR_PROBE_DEFINE=/DNR_DX11_GAME_TEST /DNR_EXPERIMENTAL_VULKAN /DNR_NESTED_SOURCE_GUARD /DNR_MULTIPASS_EDGE_RELEASE"
 set "NR_VALIDATION_FLAGS=--dx11-game-test --experimental-vulkan --multipass-edge-release --addon-version 1.0.8"
 set "NR_PATCH_FLAGS=--addon-build 18 --addon-version 1.0.8 --release-version"
 set "NR_TEST_DEFINE=/DNR_DX11_GAME_TEST"
 set NR_VULKAN_INCLUDE=/I "%ROOT%build\vulkan-headers-api\Include"
 if not exist "%ROOT%build\manager-1.0.8-addon" mkdir "%ROOT%build\manager-1.0.8-addon" || exit /b 1
)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe" /nologo /T cs_6_0 /E Resample /Fo "%ROOT%build\neural_resample_v66.cso" "%ROOT%src\neural_resample.hlsl" || exit /b 1
python "%ROOT%tools\binary_to_header.py" "%ROOT%build\neural_resample_v66.cso" "%ROOT%src\neural_resample_shader.hpp" g_neural_resample_shader || exit /b 1
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe" /nologo /D NR_EDGE_DEPTH=1 /T cs_6_0 /E Resample /Fo "%ROOT%build\neural_resample_edge_v66.cso" "%ROOT%src\neural_resample.hlsl" || exit /b 1
python "%ROOT%tools\binary_to_header.py" "%ROOT%build\neural_resample_edge_v66.cso" "%ROOT%src\neural_resample_edge_shader.hpp" g_neural_resample_edge_shader || exit /b 1
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe" /nologo /T cs_5_0 /E main /O3 /Fo "%ROOT%build\screenshot_copy.cso" "%ROOT%src\screenshot_copy.hlsl" || exit /b 1
python "%ROOT%tools\binary_to_header.py" "%ROOT%build\screenshot_copy.cso" "%ROOT%src\screenshot_copy_shader.hpp" g_screenshot_copy_shader || exit /b 1
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe" /nologo /T cs_5_0 /E main /O3 /Fo "%ROOT%build\dx11_depth_convert.cso" "%ROOT%src\dx11_depth_convert.hlsl" || exit /b 1
python "%ROOT%tools\binary_to_header.py" "%ROOT%build\dx11_depth_convert.cso" "%ROOT%src\dx11_depth_convert_shader.hpp" g_dx11_depth_convert_shader || exit /b 1
cl /nologo /std:c++20 /EHsc /W4 /WX /Fo"%ROOT%build\scale_history.obj" /Fe"%ROOT%build\scale_history.exe" "%ROOT%tests\scale_history.cpp" || exit /b 1
"%ROOT%build\scale_history.exe" || exit /b 1
cl /nologo /c /std:c++20 /MD /EHs-c- /O2 /Oi /GS- /GR- /guard:cf- /Zl /Brepro /W4 /WX /DNR_EXPERIMENTAL_DX11 %NR_PROBE_DEFINE% ^
 /I "%ROOT%build\minhook-api\include" /I "%ROOT%build\dlss-sdk-api\include" %NR_VULKAN_INCLUDE% /I "C:\tmp\reshade-source\include" /I "C:\tmp\imgui-source" ^
 /Fo"%ROOT%build\neural_resolution_dx11.obj" "%ROOT%src\neural_resolution_addon.cpp" || exit /b 1
ml64 /nologo /c /Fo"%ROOT%build\embedded_bridges_dx11.obj" "%ROOT%src\embedded_bridges.asm" || exit /b 1
cl /nologo /c /MD /O2 /GS- /Zl /Brepro /Fo"%ROOT%build\\" "%ROOT%build\minhook-api\src\hook.c" "%ROOT%build\minhook-api\src\buffer.c" "%ROOT%build\minhook-api\src\trampoline.c" "%ROOT%build\minhook-api\src\hde\hde64.c" || exit /b 1
for %%T in (dx11_preinit_rollback dx11_native_registry dx11_replacement_session dx11_core_shutdown framegen_boundary) do (
cl /nologo /std:c++20 /MD /EHsc /O2 /W4 /WX %NR_TEST_DEFINE% ^
 /I "%ROOT%build\minhook-api\include" /I "%ROOT%build\dlss-sdk-api\include" /I "C:\tmp\reshade-source\include" /I "C:\tmp\imgui-source" ^
 /Fo"%ROOT%build\%%T.obj" /Fe"%ROOT%build\%%T.exe" "%ROOT%tests\%%T.cpp" ^
 "%ROOT%build\hook.obj" "%ROOT%build\buffer.obj" "%ROOT%build\trampoline.obj" "%ROOT%build\hde64.obj" ^
 /link d3d11.lib d3d12.lib dxgi.lib user32.lib Psapi.lib ole32.lib windowscodecs.lib uuid.lib || exit /b 1
"%ROOT%build\%%T.exe" || exit /b 1
)
link /nologo /dll /nodefaultlib /entry:combined_entry /dynamicbase /incremental:no /Brepro /opt:ref /opt:icf ^
 /map:"%ROOT%build\neural_resolution_dx11.map" /out:"%ROOT%build\neural_resolution_dx11_embedded.dll" ^
 "%ROOT%build\neural_resolution_dx11.obj" "%ROOT%build\embedded_bridges_dx11.obj" "%ROOT%build\hook.obj" "%ROOT%build\buffer.obj" "%ROOT%build\trampoline.obj" "%ROOT%build\hde64.obj" kernel32.lib Psapi.lib User32.lib ucrt.lib ole32.lib windowscodecs.lib uuid.lib bcrypt.lib || exit /b 1
python "%ROOT%tools\patch_v6_addon.py" --base "%ROOT%updated-official-renodx-dlss.addon64" ^
 --embedded "%ROOT%build\neural_resolution_dx11_embedded.dll" --map "%ROOT%build\neural_resolution_dx11.map" ^
 --section-name .nr-dx11 --screenshot-capture %NR_PATCH_FLAGS% --output "%NR_OUTPUT%" || exit /b 1
python "%ROOT%tools\validate_v6_addon.py" --base "%ROOT%updated-official-renodx-dlss.addon64" ^
 --addon "%NR_OUTPUT%" --version V6.6 --experimental-dx11 --screenshot-capture %NR_VALIDATION_FLAGS% || exit /b 1
if /i "%~1"=="vulkan" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="framegen-input-trace" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="framegen-boundary-trace" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="nested-source" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="manager-release" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="pass-controls" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="pass-history-fix" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="manager-release-1.0.3" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="manager-release-1.0.5" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="manager-release-1.0.6" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
if /i "%~1"=="manager-release-1.0.8" call "%ROOT%scripts\test\test-vulkan-export-lookup.cmd" || exit /b 1
copy /y "%ROOT%build\minhook-api\LICENSE.txt" "%ROOT%build\dx11-experimental-minhook-LICENSE.txt" >nul || exit /b 1
endlocal
