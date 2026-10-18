@echo off
set MSVC=D:\DevTools\VSBuildTools2022\VC\Tools\MSVC\14.44.35207
set SDK=C:\Program Files (x86)\Windows Kits\10
set SDKV=10.0.22621.0
set PATH=%MSVC%\bin\Hostx64\x64;%SDK%\bin\%SDKV%\x64;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin;%PATH%
rem JUCE's VST3 manifest helper is a Debug build and needs the debug runtime DLLs
set PATH=%PATH%;D:\DevTools\VSBuildTools2022\VC\Redist\MSVC\14.44.35112\debug_nonredist\x64\Microsoft.VC143.DebugCRT;%SDK%\bin\%SDKV%\x64\ucrt
set INCLUDE=%MSVC%\include;%SDK%\Include\%SDKV%\ucrt;%SDK%\Include\%SDKV%\um;%SDK%\Include\%SDKV%\shared;%SDK%\Include\%SDKV%\winrt;%SDK%\Include\%SDKV%\cppwinrt
set LIB=%MSVC%\lib\x64;%SDK%\Lib\%SDKV%\ucrt\x64;%SDK%\Lib\%SDKV%\um\x64
set CMAKE=cmake
cd /d D:\woody-plugins
if not exist build\CMakeCache.txt (
  %CMAKE% -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DWOODY_PLUGINS="drum-woodys;synth-woodys;eq-woodys" || exit /b 1
)
%CMAKE% -B build -DWOODY_PLUGINS="drum-woodys;synth-woodys;eq-woodys" >nul || exit /b 1
%CMAKE% --build build --target DrumWoodys_VST3 DrumWoodys_Standalone SynthWoodys_VST3 SynthWoodys_Standalone EqWoodys_VST3 EqWoodys_Standalone || exit /b 1
