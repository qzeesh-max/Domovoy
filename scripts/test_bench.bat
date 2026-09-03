@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: test_bench.bat - Run Domovoy benchmarks
::
:: Usage:
::   scripts\test_bench.bat [OPTIONS]
::
:: Options:
::   --filter <regex>        Google Benchmark filter (e.g. "BM_IsolatedAllocator")
::   --format <json|console> Output format (default: console)
::   --out <file>            Write benchmark results to file (requires --format json)
::   --min-time <seconds>    Minimum time per benchmark (default: 1.0)
::   -h, --help              Show this help
:: =============================================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

if not defined DOMOVOY_BUILD_DIR (
    set "BUILD_DIR=%PROJECT_ROOT%\build"
) else (
    set "BUILD_DIR=%DOMOVOY_BUILD_DIR%"
)

set "FILTER="
set "FORMAT=console"
set "OUT_FILE="
set "MIN_TIME=1.0"
set "BUILD_TYPE=Release"

:parse_args
if "%~1"=="" goto check_args
if /i "%~1"=="--build-type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--filter" (
    set "FILTER=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--format" (
    set "FORMAT=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--out" (
    set "OUT_FILE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--min-time" (
    set "MIN_TIME=%~2"
    shift
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
echo   Domovoy - Benchmarks
echo   Format: %FORMAT%  Min-time: %MIN_TIME%s
echo ============================================================

call "%SCRIPT_DIR%build.bat" --type "%BUILD_TYPE%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "BENCH_BIN=%BUILD_DIR%\tests\bench\%BUILD_TYPE%\bench_core.exe"
if not exist "%BENCH_BIN%" (
    set "BENCH_BIN=%BUILD_DIR%\tests\bench\bench_core.exe"
    if not exist "!BENCH_BIN!" (
        echo ERROR: Benchmark binary not found at %BENCH_BIN%
        exit /b 1
    )
)

set "BENCH_ARGS=--benchmark_min_time=%MIN_TIME%s --benchmark_format=%FORMAT%"
if not "%FILTER%"=="" set "BENCH_ARGS=%BENCH_ARGS% --benchmark_filter=%FILTER%"
if not "%OUT_FILE%"=="" set "BENCH_ARGS=%BENCH_ARGS% --benchmark_out="%OUT_FILE%" --benchmark_out_format=%FORMAT%"

echo.
echo [bench] Running benchmarks...
"%BENCH_BIN%" %BENCH_ARGS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

if not "%OUT_FILE%"=="" (
    echo.
    echo 📊  Results written to: %OUT_FILE%
)

echo.
echo ✅  Benchmarks complete.
exit /b 0
