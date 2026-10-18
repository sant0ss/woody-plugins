@echo off
rem Builds some targets in their own build folder, so several people can build at once.
rem Usage: build-one.bat <name> <target> [<target> ...]     (folder: D:\woody-plugins\build-<name>)
setlocal
set NAME=%1
shift
set MSVC=D:\DevTools\VSBuildTools2022\VC\Tools\MSVC\14.44.35207
set SDK=C:\Program Files (x86)\Windows Kits\10
set SDKV=10.0.22621.0
set PATH=%MSVC%\bin\Hostx64\x64;%SDK%\bin\%SDKV%\x64;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin;%PATH%
rem JUCE's VST3 manifest helper is a Debug build and needs the debug runtime DLLs
set PATH=%PATH%;D:\DevTools\VSBuildTools2022\VC\Redist\MSVC\14.44.35112\debug_nonredist\x64\Microsoft.VC143.DebugCRT;%SDK%\bin\%SDKV%\x64\ucrt
set INCLUDE=%MSVC%\include;%SDK%\Include\%SDKV%\ucrt;%SDK%\Include\%SDKV%\um;%SDK%\Include\%SDKV%\shared;%SDK%\Include\%SDKV%\winrt;%SDK%\Include\%SDKV%\cppwinrt
set LIB=%MSVC%\lib\x64;%SDK%\Lib\%SDKV%\ucrt\x64;%SDK%\Lib\%SDKV%\um\x64
cd /d D:\woody-plugins
set BUILD=build-%NAME%
if not exist %BUILD%\CMakeCache.txt (
  cmake -B %BUILD% -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DWOODY_PLUGINS=%NAME%-woodys || exit /b 1
) else (
  cmake -B %BUILD% -DWOODY_PLUGINS=%NAME%-woodys >nul || exit /b 1
)
cmake --build %BUILD% --target %1 %2 %3 %4 %5 %6 || exit /b 1
