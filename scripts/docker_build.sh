#!/usr/bin/env bash
# =============================================================================
# docker_build.sh — Build Domovoy Docker images
#
# Usage:
#   ./scripts/docker_build.sh [OPTIONS]
#
# Options:
#   --image <ubuntu|clang|all>   Image to build (default: all)
#   --tag <tag>                  Docker image tag prefix (default: domovoy)
#   --no-cache                   Pass --no-cache to docker build
#   -h, --help                   Show this help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKER_DIR="${PROJECT_ROOT}/docker"

IMAGE="all"
TAG_PREFIX="domovoy"
NO_CACHE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)    IMAGE="$2"; shift 2 ;;
        --tag)      TAG_PREFIX="$2"; shift 2 ;;
        --no-cache) NO_CACHE="--no-cache"; shift ;;
        -h|--help)  grep '^#' "$0" | grep -A50 'Usage:' | sed 's/^# \?//'; exit 0 ;;
        *)          echo "Unknown option: $1"; exit 1 ;;
    esac
done

build_image() {
    local variant="$1"          # ubuntu | clang
    local dockerfile="${DOCKER_DIR}/Dockerfile.${variant}"
    local tag="${TAG_PREFIX}:${variant}"

    echo ""
    echo "🐳  Building image: ${tag}  (${dockerfile})"
    docker build \
        ${NO_CACHE} \
        -f "${dockerfile}" \
        -t "${tag}" \
        "${PROJECT_ROOT}"

    echo "✅  Image built: ${tag}"
}

case "$IMAGE" in
    ubuntu) build_image ubuntu ;;
    clang)  build_image clang ;;
    all)
        build_image ubuntu
        build_image clang
        ;;
    *)
        echo "Unknown image variant: ${IMAGE}. Choose: ubuntu | clang | all"
        exit 1
        ;;
esac
