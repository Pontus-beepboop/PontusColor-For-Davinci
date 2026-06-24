@echo off
REM ============================================================================
REM  PontusColor - Windows build  (right-click -> Run, or run from a terminal)
REM  Produces build\PontusColor.ofx.bundle
REM
REM  Prerequisites (one-time):
REM    * Visual Studio 2022 with "Desktop development with C++"
REM    * CMake  (https://cmake.org/download/  -> add to PATH)
REM    * An OpenCL SDK. The easiest is the NVIDIA CUDA Toolkit (also enables the
REM      optional CUDA backend). AMD/Intel users: install the OCL-SDK or the
REM      Intel oneAPI, which provide CL\cl.h and OpenCL.lib.
REM
REM  To also build the CUDA backend, run:   build_windows.bat cuda
REM ============================================================================
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake not found on PATH. Install from https://cmake.org/download/
    exit /b 1
)

set CUDA_FLAG=-DPONTUS_WITH_CUDA=OFF
if /I "%1"=="cuda" set CUDA_FLAG=-DPONTUS_WITH_CUDA=ON

echo ==^> Configuring
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 %CUDA_FLAG%
if errorlevel 1 exit /b 1

echo ==^> Building
cmake --build build --config Release
if errorlevel 1 exit /b 1

echo.
echo ==^> Built: build\PontusColor.ofx.bundle
echo.
echo To install, copy that .ofx.bundle folder into:
echo     C:\Program Files\Common Files\OFX\Plugins
echo (create the OFX\Plugins folders if they do not exist), then restart Resolve.
echo Or run  install_windows.bat  from an Administrator terminal to do it for you.
endlocal
