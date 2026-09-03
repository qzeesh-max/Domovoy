@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: test_integration.bat - Run Domovoy integration tests
::
:: Usage:
::   scripts\test_integration.bat [OPTIONS]
::
:: Options:
::   --build-type <type>   Build type forwarded to build.bat (default: Debug)
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

:parse_args
if "%~1"=="" goto check_args
if /i "%~1"=="--build-type" (
    set "BUILD_TYPE=%~2"
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
echo   Domovoy - Integration Tests
echo   Build type: %BUILD_TYPE%
echo ============================================================

call "%SCRIPT_DIR%build.bat" --type "%BUILD_TYPE%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "CTEST_ARGS=--test-dir "%BUILD_DIR%" --build-config "%BUILD_TYPE%" --output-on-failure -L "^integration$""
if "%VERBOSE%"=="1" set "CTEST_ARGS=%CTEST_ARGS% --verbose"

echo.
echo [test_integration] Running integration tests...
ctest %CTEST_ARGS% --no-tests=error
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo ✅  Integration tests complete.
exit /b 0
