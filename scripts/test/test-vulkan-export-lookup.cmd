@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI\"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cl /nologo /std:c++20 /MD /EHsc /O2 /W4 /WX ^
 /I "%ROOT%build\minhook-api\include" /I "%ROOT%build\dlss-sdk-api\include" /I "%ROOT%build\vulkan-headers-api\Include" /I "C:\tmp\reshade-source\include" /I "C:\tmp\imgui-source" ^
 /Fo"%ROOT%build\vulkan_export_lookup.obj" /Fe"%ROOT%build\vulkan_export_lookup.exe" "%ROOT%tests\vulkan_export_lookup.cpp" ^
 "%ROOT%build\hook.obj" "%ROOT%build\buffer.obj" "%ROOT%build\trampoline.obj" "%ROOT%build\hde64.obj" ^
 /link d3d11.lib d3d12.lib dxgi.lib user32.lib Psapi.lib ole32.lib windowscodecs.lib uuid.lib bcrypt.lib || exit /b 1
"%ROOT%build\vulkan_export_lookup.exe" || exit /b 1
endlocal
