#!/usr/bin/env bash
# =============================================================================
# test_unit.sh — Run Domovoy unit tests
#
# Usage:
#   ./scripts/test_unit.sh [OPTIONS]
#
# Options:
#   --build-type <type>   Build type forwarded to build.sh (default: Debug)
#   --asan                Enable AddressSanitizer
#   --tsan                Enable ThreadSanitizer
#   --ubsan               Enable UndefinedBehaviorSanitizer
#   --filter <pattern>    GTest filter pattern (e.g. "AllocatorTest.*")
#   --verbose             Verbose CTest output
#   -h, --help            Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${DOMOVOY_BUILD_DIR:-${PROJECT_ROOT}/build}"

BUILD_TYPE="Debug"
SANITIZER_FLAGS=()
GTEST_FILTER=""
VERBOSE=0
BUILD_EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        --asan)       SANITIZER_FLAGS+=(--asan); BUILD_EXTRA_ARGS+=(--asan); shift ;;
        --tsan)       SANITIZER_FLAGS+=(--tsan); BUILD_EXTRA_ARGS+=(--tsan); shift ;;
        --ubsan)      SANITIZER_FLAGS+=(--ubsan); BUILD_EXTRA_ARGS+=(--ubsan); shift ;;
        --filter)     GTEST_FILTER="$2"; shift 2 ;;
        --verbose)    VERBOSE=1; shift ;;
        -h|--help)    grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)            echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "============================================================"
echo "  Domovoy — Unit Tests"
echo "  Build type: ${BUILD_TYPE}"
echo "============================================================"

# Ensure the project is built
"${SCRIPT_DIR}/build.sh" --type "${BUILD_TYPE}" "${BUILD_EXTRA_ARGS[@]+"${BUILD_EXTRA_ARGS[@]}"}"

CTEST_ARGS=(--test-dir "${BUILD_DIR}" --output-on-failure -L "^unit$")

if [[ $VERBOSE -eq 1 ]]; then
    CTEST_ARGS+=(--verbose)
fi

if [[ -n "$GTEST_FILTER" ]]; then
    export GTEST_FILTER
fi

echo ""
echo "[test_unit] Running unit tests..."
# --no-tests=error makes ctest exit non-zero if nothing matched the label
ctest "${CTEST_ARGS[@]}" --no-tests=error || {
    echo ""
    echo "[test_unit] Running test_allocator directly (ctest label match failed)..."
    FILTER_ARG=""
    [[ -n "$GTEST_FILTER" ]] && FILTER_ARG="--gtest_filter=${GTEST_FILTER}"
    "${BUILD_DIR}/tests/unit/test_allocator" $FILTER_ARG
}

echo ""
echo "✅  Unit tests complete."
