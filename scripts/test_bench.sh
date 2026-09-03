#!/usr/bin/env bash
# =============================================================================
# test_bench.sh — Run Domovoy benchmarks
#
# Usage:
#   ./scripts/test_bench.sh [OPTIONS]
#
# Options:
#   --filter <regex>        Google Benchmark filter (e.g. "BM_IsolatedAllocator")
#   --format <json|console> Output format (default: console)
#   --out <file>            Write benchmark results to file (requires --format json)
#   --min-time <seconds>    Minimum time per benchmark (default: 1.0)
#   -h, --help              Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BENCH_BIN="${BUILD_DIR}/tests/bench/bench_core"

FILTER=""
FORMAT="console"
OUT_FILE=""
MIN_TIME="1.0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --filter)   FILTER="$2"; shift 2 ;;
        --format)   FORMAT="$2"; shift 2 ;;
        --out)      OUT_FILE="$2"; shift 2 ;;
        --min-time) MIN_TIME="$2"; shift 2 ;;
        -h|--help)  grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)          echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "============================================================"
echo "  Domovoy — Benchmarks"
echo "  Format: ${FORMAT}  Min-time: ${MIN_TIME}s"
echo "============================================================"

# Build in Release mode for meaningful benchmark numbers
"${SCRIPT_DIR}/build.sh" --type Release

if [[ ! -x "${BENCH_BIN}" ]]; then
    echo "ERROR: Benchmark binary not found at ${BENCH_BIN}"
    exit 1
fi

BENCH_ARGS=(
    "--benchmark_min_time=${MIN_TIME}"
    "--benchmark_format=${FORMAT}"
)

[[ -n "$FILTER" ]]   && BENCH_ARGS+=("--benchmark_filter=${FILTER}")
[[ -n "$OUT_FILE" ]] && BENCH_ARGS+=("--benchmark_out=${OUT_FILE}" "--benchmark_out_format=${FORMAT}")

echo ""
echo "[bench] Running benchmarks..."
"${BENCH_BIN}" "${BENCH_ARGS[@]}"

if [[ -n "$OUT_FILE" ]]; then
    echo ""
    echo "📊  Results written to: ${OUT_FILE}"
fi

echo ""
echo "✅  Benchmarks complete."
