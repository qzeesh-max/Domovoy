# Domovoy Integration Tests

The integration tests in this directory are designed to run Domovoy end-to-end to ensure that features execute properly and generate expected observability reports.

## JSON Output Validation
We use the cross-platform `test_utils.h` helper functions to inspect the JSON files that Domovoy writes to the output directory:
- `CleanUpOldReports()` is called at the beginning of each test to sweep for leftover JSON artifacts from previous runs.
- `FindAndParseReport(prefix)` locates the newest JSON artifact for a given test (e.g. `domovoy_leaks_*`), parses it via `nlohmann::json`, deletes the file, and returns the payload.

The tests then run specific `ASSERT` calls to ensure that the correct JSON payload was delivered.

## Platform Quirks & Exceptions

Some integrations behave differently across OS architectures, which is reflected in our tests:

### Crash Handler Validation (`test_crash.cpp`)
The JSON validation in `test_crash` is also disabled on Windows. 
**Why?** The Google Test framework handles `EXPECT_DEATH` scenarios on Windows by spinning up a new child process wrapped in Structured Exception Handling (`__try / __except`). Because `__try / __except` intercepts access violations natively before they can reach unhandled exception filters, our crash handler (`SetUnhandledExceptionFilter`) is never invoked. As a result, the test framework's architecture suppresses the crash report generation on Windows. (The crash handler behaves normally in production, but cannot be validated under Windows `EXPECT_DEATH`).

### I/O Leak Monitoring (`test_io.cpp`)
On Windows, `test_io` leverages the native `CreateFileA()` Win32 API to intentionally trigger a file handle leak instead of standard `open()`. This is because Domovoy uses Microsoft Detours to hook `CreateFileA/W` to monitor I/O on Windows platforms. Using the native API guarantees the hook captures the leak effectively across all compiler toolchains (MSVC, MSYS2 UCRT).
