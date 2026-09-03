@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: test_all.bat - Run all Domovoy tests
::
:: Usage:
::   scripts\test_all.bat [OPTIONS]
:: =============================================================================

set "SCRIPT_DIR=%~dp0"

echo ============================================================
echo   Domovoy - Full Test Suite
echo ============================================================

echo.
echo --- 1/3: Unit Tests ---
call "%SCRIPT_DIR%test_unit.bat" %*
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo --- 2/3: Integration Tests ---
call "%SCRIPT_DIR%test_integration.bat" %*
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo --- 3/3: Benchmarks (Release) ---
call "%SCRIPT_DIR%test_bench.bat" --build-type Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo 🎉 All tests passed successfully.
exit /b 0
