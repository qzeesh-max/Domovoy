@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: build.bat - Main build script for Domovoy on Windows
::
:: Usage:
::   scripts\build.bat [OPTIONS]
::
:: Options:
::   --type <Debug|Release|RelWithDebInfo>  Build type (default: Debug)
::   --asan                                 Enable AddressSanitizer
::   --tsan                                 Enable ThreadSanitizer
::   --ubsan                                Enable UndefinedBehaviorSanitizer
::   --msan                                 Enable MemorySanitizer
::   --clean                                Remove existing build directory first
::   --jobs <N>                             Number of parallel jobs (default: 4)
::   -h, --help                             Show this help
:: =============================================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

:: Defaults
set "BUILD_TYPE=Debug"
set "ENABLE_ASAN=OFF"
set "ENABLE_TSAN=OFF"
set "ENABLE_UBSAN=OFF"
set "ENABLE_MSAN=OFF"
set "CLEAN=0"
set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"

:parse_args
if "%~1"=="" goto check_args
if /i "%~1"=="--type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--asan" (
    set "ENABLE_ASAN=ON"
    shift
    goto parse_args
)
if /i "%~1"=="--tsan" (
    set "ENABLE_TSAN=ON"
    shift
    goto parse_args
)
if /i "%~1"=="--ubsan" (
    set "ENABLE_UBSAN=ON"
    shift
    goto parse_args
)
if /i "%~1"=="--msan" (
    set "ENABLE_MSAN=ON"
    shift
    goto parse_args
)
if /i "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto parse_args
)
if /i "%~1"=="--jobs" (
    set "JOBS=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage

echo Unknown option: %~1
goto usage

:usage
findstr "^::" "%~f0" | more
exit /b 0

:check_args
if not defined DOMOVOY_BUILD_DIR (
    set "BUILD_DIR=%PROJECT_ROOT%\build"
) else (
    set "BUILD_DIR=%DOMOVOY_BUILD_DIR%"
)

echo ============================================================
echo   Domovoy Build
echo   Type:  %BUILD_TYPE%
echo   ASan:  %ENABLE_ASAN%  TSan: %ENABLE_TSAN%
echo   UBSan: %ENABLE_UBSAN% MSan: %ENABLE_MSAN%
echo   Jobs:  %JOBS%
echo   BuildDir: %BUILD_DIR%
echo ============================================================

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo [build] Cleaning %BUILD_DIR%...
        rmdir /s /q "%BUILD_DIR%"
    )
)

cmake -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" ^
    -DENABLE_ASAN="%ENABLE_ASAN%" ^
    -DENABLE_TSAN="%ENABLE_TSAN%" ^
    -DENABLE_UBSAN="%ENABLE_UBSAN%" ^
    -DENABLE_MSAN="%ENABLE_MSAN%" ^
    "%PROJECT_ROOT%"

if %ERRORLEVEL% neq 0 (
    echo cmake configure failed
    exit /b %ERRORLEVEL%
)

cmake --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --parallel "%JOBS%"

if %ERRORLEVEL% neq 0 (
    echo cmake build failed
    exit /b %ERRORLEVEL%
)

echo.
echo ✅  Build complete -^> %BUILD_DIR%
exit /b 0
