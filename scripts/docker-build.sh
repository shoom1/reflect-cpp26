#!/usr/bin/env bash
# Build reflect_cpp26 inside the pinned clang-p2996 container.
#
# Usage:
#   scripts/docker-build.sh                  # build the 'docker' preset (Debug)
#   scripts/docker-build.sh docker-release   # build the 'docker-release' preset
#
# Env overrides:
#   REFLECT_DOCKER_IMAGE   image tag to use (default: reflect-cpp26-dev:local)
#   REFLECT_DOCKER_REBUILD set to 1 to force a docker build before running
set -euo pipefail

IMAGE="${REFLECT_DOCKER_IMAGE:-reflect-cpp26-dev:source}"
PRESET="${1:-docker}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "${REFLECT_DOCKER_REBUILD:-0}" == "1" ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">>> Building Docker image: $IMAGE"
    docker build -t "$IMAGE" "$REPO_ROOT"
fi

echo ">>> Configuring + building preset: $PRESET"
# --network=none: the compile step needs no network access; this prevents
# the container (and anything in the toolchain image) from reaching the
# internet during a build. Image build above still has network for apt-get.
docker run --rm \
    --network=none \
    -v "$REPO_ROOT":/work \
    -w /work \
    "$IMAGE" \
    bash -c 'cmake --preset "$1" && cmake --build --preset "$1"' bash "$PRESET"
