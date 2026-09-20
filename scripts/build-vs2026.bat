@echo off
setlocal
cd /d "%~dp0.."

rem Use configured paths; otherwise try conventional local locations.
if not defined VCPKG_ROOT if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
  echo Set the VCPKG_ROOT user environment variable to your vcpkg directory.
  goto :failed
)
cmake --preset windows-debug
if errorlevel 1 goto :failed
set "SOVKIT_SOLUTION=%CD%\out\sovkit-devtools.sln"
if exist "%CD%\out\sovkit-devtools.slnx" set "SOVKIT_SOLUTION=%CD%\out\sovkit-devtools.slnx"
if not exist "%SOVKIT_SOLUTION%" goto :failed
echo.
echo Open "%SOVKIT_SOLUTION%" in Visual Studio and select Debug / x64.
echo Set sovkit-devtools as the startup project, build the solution, and press F5.
if /i "%~1"=="--no-open" exit /b 0
start "" "%SOVKIT_SOLUTION%"
exit /b 0

:failed
echo.
echo Solution generation failed. Install Visual Studio 2026 with Desktop development
echo with C++, CMake 4.2 or newer, and Python 3.8 or newer. CMake and Python must be on PATH.
if /i not "%~1"=="--no-open" pause
exit /b 1
