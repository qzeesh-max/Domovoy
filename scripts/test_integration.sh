#!/usr/bin/env bash
# =============================================================================
# test_integration.sh — Run Domovoy integration tests
#
# Usage:
#   ./scripts/test_integration.sh [OPTIONS]
#
# Options:
#   --build-type <type>   Build type forwarded to build.sh (default: Debug)
#   --asan                Enable AddressSanitizer
#   --tsan                Enable ThreadSanitizer
#   --ubsan               Enable UndefinedBehaviorSanitizer
#   --test <name>         Run a specific test (leak|cpu|io|crash|all; default: all)
#   --verbose             Verbose CTest output
#   -h, --help            Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${DOMOVOY_BUILD_DIR:-${PROJECT_ROOT}/build}"

BUILD_TYPE="Debug"
BUILD_EXTRA_ARGS=()
TARGET_TEST="all"
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        --asan)       BUILD_EXTRA_ARGS+=(--asan); shift ;;
        --tsan)       BUILD_EXTRA_ARGS+=(--tsan); shift ;;
        --ubsan)      BUILD_EXTRA_ARGS+=(--ubsan); shift ;;
        --test)       TARGET_TEST="$2"; shift 2 ;;
        --verbose)    VERBOSE=1; shift ;;
        -h|--help)    grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)            echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "============================================================"
echo "  Domovoy — Integration Tests"
echo "  Build type:  ${BUILD_TYPE}"
echo "  Target test: ${TARGET_TEST}"
echo "============================================================"

"${SCRIPT_DIR}/build.sh" --type "${BUILD_TYPE}" "${BUILD_EXTRA_ARGS[@]+"${BUILD_EXTRA_ARGS[@]}"}"

CTEST_ARGS=(--test-dir "${BUILD_DIR}" --output-on-failure)
[[ $VERBOSE -eq 1 ]] && CTEST_ARGS+=(--verbose)

declare -A TEST_MAP=(
    [leak]="LeakTest"
    [cpu]="CpuTest"
    [io]="IoTest"
    [crash]="CrashTest"
)

run_test() {
    local name="$1"
    local label="${TEST_MAP[$name]:-}"
    if [[ -z "$label" ]]; then
        echo "Unknown test: $name. Valid: leak cpu io crash all"
        exit 1
    fi
    echo ""
    echo "[integration] Running ${label}..."
    ctest "${CTEST_ARGS[@]}" --no-tests=error -R "^${label}$"
}

if [[ "$TARGET_TEST" == "all" ]]; then
    for t in leak cpu io crash; do
        run_test "$t"
    done
else
    run_test "$TARGET_TEST"
fi

echo ""
echo "✅  Integration tests complete."
