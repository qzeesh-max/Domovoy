#!/usr/bin/env bash
# =============================================================================
# test_all.sh — Run the full Domovoy test suite (unit + integration + bench)
#
# Usage:
#   ./scripts/test_all.sh [OPTIONS]
#
# Options:
#   --asan        Enable AddressSanitizer for unit + integration tests
#   --tsan        Enable ThreadSanitizer for unit + integration tests
#   --ubsan       Enable UndefinedBehaviorSanitizer for unit + integration tests
#   --skip-bench  Skip benchmarks
#   --verbose     Verbose output
#   -h, --help    Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EXTRA_FLAGS=()
SKIP_BENCH=0
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --asan)       EXTRA_FLAGS+=(--asan); shift ;;
        --tsan)       EXTRA_FLAGS+=(--tsan); shift ;;
        --ubsan)      EXTRA_FLAGS+=(--ubsan); shift ;;
        --skip-bench) SKIP_BENCH=1; shift ;;
        --verbose)    VERBOSE=1; EXTRA_FLAGS+=(--verbose); shift ;;
        -h|--help)    grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)            echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "============================================================"
echo "  Domovoy — Full Test Suite"
echo "============================================================"

PASS=0
FAIL=0
RESULTS=()

run_step() {
    local label="$1"; shift
    echo ""
    echo "──────────────────────────────────────────"
    echo "  STEP: ${label}"
    echo "──────────────────────────────────────────"
    if "$@"; then
        RESULTS+=("✅  ${label}")
        ((PASS++)) || true
    else
        RESULTS+=("❌  ${label}")
        ((FAIL++)) || true
    fi
}

run_step "Unit Tests"        "${SCRIPT_DIR}/test_unit.sh"        "${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"}"
run_step "Integration Tests" "${SCRIPT_DIR}/test_integration.sh" "${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"}"

if [[ $SKIP_BENCH -eq 0 ]]; then
    run_step "Benchmarks" "${SCRIPT_DIR}/test_bench.sh" --min-time 0.1
fi

echo ""
echo "============================================================"
echo "  SUMMARY"
echo "============================================================"
for r in "${RESULTS[@]}"; do
    echo "  ${r}"
done
echo ""
echo "  Passed: ${PASS}   Failed: ${FAIL}"
echo "============================================================"

[[ $FAIL -eq 0 ]]
