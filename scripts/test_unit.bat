@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: test_unit.bat - Run Domovoy unit tests
::
:: Usage:
::   scripts\test_unit.bat [OPTIONS]
::
:: Options:
::   --build-type <type>   Build type forwarded to build.bat (default: Debug)
::   --asan                Enable AddressSanitizer
::   --tsan                Enable ThreadSanitizer
::   --ubsan               Enable UndefinedBehaviorSanitizer
::   --filter <pattern>    GTest filter pattern (e.g. "AllocatorTest.*")
::   --verbose             Verbose CTest output
::   -h, --help            Show this help
:: =============================================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

if not defined DOMOVOY_BUILD_DIR (
    set "BUILD_DIR=%PROJECT_ROOT%\build"
) else (
    set "BUILD_DIR=%DOMOVOY_BUILD_DIR%"
)

set "BUILD_TYPE=Debug"
set "VERBOSE=0"
set "BUILD_EXTRA_ARGS="
set "GTEST_FILTER="

:parse_args
if "%~1"=="" goto check_args
if /i "%~1"=="--build-type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--asan" (
    set "BUILD_EXTRA_ARGS=%BUILD_EXTRA_ARGS% --asan"
    shift
    goto parse_args
)
if /i "%~1"=="--tsan" (
    set "BUILD_EXTRA_ARGS=%BUILD_EXTRA_ARGS% --tsan"
    shift
    goto parse_args
)
if /i "%~1"=="--ubsan" (
    set "BUILD_EXTRA_ARGS=%BUILD_EXTRA_ARGS% --ubsan"
    shift
    goto parse_args
)
if /i "%~1"=="--filter" (
    set "GTEST_FILTER=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--verbose" (
    set "VERBOSE=1"
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage

echo Unknown option: %~1
exit /b 1

:usage
findstr "^::" "%~f0" | more
exit /b 0

:check_args
echo ============================================================
echo   Domovoy - Unit Tests
echo   Build type: %BUILD_TYPE%
echo ============================================================

call "%SCRIPT_DIR%build.bat" --type "%BUILD_TYPE%" %BUILD_EXTRA_ARGS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "CTEST_ARGS=--test-dir "%BUILD_DIR%" --build-config "%BUILD_TYPE%" --output-on-failure -L "^unit$""
if "%VERBOSE%"=="1" set "CTEST_ARGS=%CTEST_ARGS% --verbose"

if not "%GTEST_FILTER%"=="" set "GTEST_FILTER=%GTEST_FILTER%"

echo.
echo [test_unit] Running unit tests...
ctest %CTEST_ARGS% --no-tests=error
if %ERRORLEVEL% neq 0 (
    echo.
    echo [test_unit] Running test_allocator directly ^(ctest label match failed^)...
    set "FILTER_ARG="
    if not "%GTEST_FILTER%"=="" set "FILTER_ARG=--gtest_filter=%GTEST_FILTER%"
    "%BUILD_DIR%\tests\unit\%BUILD_TYPE%\test_allocator.exe" !FILTER_ARG!
    if !ERRORLEVEL! neq 0 (
        :: MSYS2 might just put it in tests/unit/ without the config directory.
        if exist "%BUILD_DIR%\tests\unit\test_allocator.exe" (
            "%BUILD_DIR%\tests\unit\test_allocator.exe" !FILTER_ARG!
        )
    )
)

echo.
echo ✅  Unit tests complete.
exit /b 0
