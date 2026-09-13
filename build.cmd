@echo off
REM Builds OpenDrummer.
REM
REM   build.cmd [Config]      Config defaults to RelWithDebInfo.
REM
REM Needs Visual Studio 2022 (any edition, or Build Tools) with the C++ workload,
REM CMake 3.28+ and Ninja. JUCE and sfizz are downloaded on the first configure.
REM Optional, set before running:
REM   ASIO_SDK_DIR   path to the Steinberg ASIO SDK, for low-latency ASIO output
REM   JUCE_DIR       a local JUCE checkout, to build without downloading
REM   SFIZZ_DIR      a local sfizz checkout (with submodules)
REM
REM CMake cannot drive a Build Tools install through the "Visual Studio"
REM generator, so this enters the MSVC developer environment and uses Ninja.

setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=RelWithDebInfo"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Install Visual Studio 2022 or Build Tools with the C++ workload.
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"

if "%VSINSTALL%"=="" (
    echo ERROR: No Visual Studio install with the C++ tools was found.
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: Failed to enter the MSVC developer environment.
    exit /b 1
)

REM Common install locations for CMake and a winget-installed Ninja, in case
REM they are not already on PATH.
set "PATH=%PATH%;%ProgramFiles%\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links"

set "EXTRA="
if defined ASIO_SDK_DIR set "EXTRA=%EXTRA% -DASIO_SDK_DIR=%ASIO_SDK_DIR%"
if defined JUCE_DIR set "EXTRA=%EXTRA% -DJUCE_DIR=%JUCE_DIR%"
if defined SFIZZ_DIR set "EXTRA=%EXTRA% -DSFIZZ_DIR=%SFIZZ_DIR%"

cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% %EXTRA%
if errorlevel 1 exit /b 1

cmake --build "%~dp0build"
if errorlevel 1 exit /b 1

echo.
echo Built: %~dp0build\OpenDrummer_artefacts\%CONFIG%\OpenDrummer.exe
