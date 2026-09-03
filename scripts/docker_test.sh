#!/usr/bin/env bash
# =============================================================================
# docker_test.sh — Run Domovoy tests inside Docker containers
#
# Usage:
#   ./scripts/docker_test.sh [OPTIONS]
#
# Options:
#   --image <ubuntu|clang|all>      Image variant to use (default: all)
#   --tag <tag>                     Docker image tag prefix (default: domovoy)
#   --suite <unit|integration|bench|all>
#                                   Test suite to run (default: all)
#   --asan                          Enable AddressSanitizer (clang image only)
#   --tsan                          Enable ThreadSanitizer (clang image only)
#   --ubsan                         Enable UndefinedBehaviorSanitizer
#   --rebuild                       Rebuild Docker image before running
#   --interactive                   Open an interactive shell instead of tests
#   -h, --help                      Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

IMAGE="all"
TAG_PREFIX="domovoy"
SUITE="all"
REBUILD=0
INTERACTIVE=0
SANITIZER_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)       IMAGE="$2"; shift 2 ;;
        --tag)         TAG_PREFIX="$2"; shift 2 ;;
        --suite)       SUITE="$2"; shift 2 ;;
        --asan)        SANITIZER_ARGS+=(--asan); shift ;;
        --tsan)        SANITIZER_ARGS+=(--tsan); shift ;;
        --ubsan)       SANITIZER_ARGS+=(--ubsan); shift ;;
        --rebuild)     REBUILD=1; shift ;;
        --interactive) INTERACTIVE=1; shift ;;
        -h|--help)     grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)             echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Map suite -> script
suite_to_script() {
    local suite="$1"
    case "$suite" in
        unit)        echo "/workspace/scripts/test_unit.sh" ;;
        integration) echo "/workspace/scripts/test_integration.sh" ;;
        bench)       echo "/workspace/scripts/test_bench.sh" ;;
        all)         echo "/workspace/scripts/test_all.sh" ;;
        *)           echo ""; return 1 ;;
    esac
}

run_in_container() {
    local variant="$1"
    local full_tag="${TAG_PREFIX}:${variant}"

    # Auto-build the image if it doesn't exist locally (or --rebuild requested)
    if [[ $REBUILD -eq 1 ]] || ! docker image inspect "${full_tag}" &>/dev/null; then
        echo "🔨  Image '${full_tag}' not found locally — building now..."
        "${SCRIPT_DIR}/docker_build.sh" --image "${variant}" --tag "${TAG_PREFIX}"
    fi

    local script
    script="$(suite_to_script "${SUITE}")" || {
        echo "Unknown suite: ${SUITE}. Choose: unit integration bench all"
        exit 1
    }

    local extra=""
    for arg in "${SANITIZER_ARGS[@]+"${SANITIZER_ARGS[@]}"}"; do
        extra="${extra} ${arg}"
    done

    if [[ $INTERACTIVE -eq 1 ]]; then
        echo "🐚  Opening interactive shell in ${full_tag}..."
        docker run --rm -it \
            -v "${PROJECT_ROOT}:/workspace" \
            "${full_tag}" \
            /bin/bash
        return
    fi

    echo ""
    echo "🐳  Running [${SUITE}] tests in image: ${full_tag}"
    echo "────────────────────────────────────────────────────────"

    docker run --rm \
        -v "${PROJECT_ROOT}:/workspace" \
        --cap-add SYS_PTRACE \
        --security-opt seccomp=unconfined \
        "${full_tag}" \
        /bin/bash -c "${script}${extra}"

    echo "✅  Container test run complete: ${full_tag}"
}

case "$IMAGE" in
    ubuntu) run_in_container ubuntu ;;
    clang)  run_in_container clang ;;
    all)
        run_in_container ubuntu
        run_in_container clang
        ;;
    *)
        echo "Unknown image variant: ${IMAGE}. Choose: ubuntu | clang | all"
        exit 1
        ;;
esac
