@echo off
rem Builds and runs a console test target from a build-<name> folder.
rem Usage: run-test-one.bat <name> <TestTarget>
setlocal
call "%~dp0build-one.bat" %1 %2 || exit /b 1
set EXE=
for /f "delims=" %%f in ('dir /s /b "D:\woody-plugins\build-%1\%2.exe" 2^>nul') do set EXE=%%f
if "%EXE%"=="" (
  echo %2.exe not found
  exit /b 1
)
echo.
"%EXE%"
exit /b %errorlevel%
