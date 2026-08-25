@echo off
REM Build script for ACEcollective
REM Usage: build.bat [Release|Debug|Clean]

setlocal

set BUILD_TYPE=Release
if not "%1"=="" set BUILD_TYPE=%1

if /I "%BUILD_TYPE%"=="Clean" (
    echo Cleaning build directory...
    if exist build rmdir /s /q build
    echo Clean complete.
    goto :end
)

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found in PATH
    exit /b 1
)

if not exist build (
    echo Configuring CMake project...
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo CMake configuration failed.
        exit /b 1
    )
)

echo ========================================
echo Building ACEcollective - %BUILD_TYPE% Configuration
echo ========================================
echo.

cmake --build build --config %BUILD_TYPE%

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED!
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL!
echo Executable location: build\%BUILD_TYPE%\ACEcollective.exe

:end
endlocal
