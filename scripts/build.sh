#!/usr/bin/env bash
# =============================================================================
# build.sh — Main build script for Domovoy
#
# Usage:
#   ./scripts/build.sh [OPTIONS]
#
# Options:
#   --type <Debug|Release|RelWithDebInfo>  Build type (default: Debug)
#   --asan                                 Enable AddressSanitizer
#   --tsan                                 Enable ThreadSanitizer
#   --ubsan                                Enable UndefinedBehaviorSanitizer
#   --msan                                 Enable MemorySanitizer
#   --clean                                Remove existing build directory first
#   --jobs <N>                             Number of parallel jobs (default: nproc)
#   -h, --help                             Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Defaults
BUILD_TYPE="Debug"
ENABLE_ASAN=OFF
ENABLE_TSAN=OFF
ENABLE_UBSAN=OFF
ENABLE_MSAN=OFF
CLEAN=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

usage() {
    sed -n '/^# Usage:/,/^# ====/p' "$0" | grep '^#' | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --type)      BUILD_TYPE="$2"; shift 2 ;;
        --asan)      ENABLE_ASAN=ON; shift ;;
        --tsan)      ENABLE_TSAN=ON; shift ;;
        --ubsan)     ENABLE_UBSAN=ON; shift ;;
        --msan)      ENABLE_MSAN=ON; shift ;;
        --clean)     CLEAN=1; shift ;;
        --jobs)      JOBS="$2"; shift 2 ;;
        -h|--help)   usage ;;
        *)           echo "Unknown option: $1"; usage ;;
    esac
done

BUILD_DIR="${DOMOVOY_BUILD_DIR:-${PROJECT_ROOT}/build}"

echo "============================================================"
echo "  Domovoy Build"
echo "  Type:  ${BUILD_TYPE}"
echo "  ASan:  ${ENABLE_ASAN}  TSan: ${ENABLE_TSAN}"
echo "  UBSan: ${ENABLE_UBSAN} MSan: ${ENABLE_MSAN}"
echo "  Jobs:  ${JOBS}"
echo "  BuildDir: ${BUILD_DIR}"
echo "============================================================"

if [[ $CLEAN -eq 1 && -d "${BUILD_DIR}" ]]; then
    echo "[build] Cleaning ${BUILD_DIR}..."
    rm -rf "${BUILD_DIR}"
fi

cmake \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DENABLE_ASAN="${ENABLE_ASAN}" \
    -DENABLE_TSAN="${ENABLE_TSAN}" \
    -DENABLE_UBSAN="${ENABLE_UBSAN}" \
    -DENABLE_MSAN="${ENABLE_MSAN}" \
    "${PROJECT_ROOT}"

cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo ""
echo "✅  Build complete → ${BUILD_DIR}"
