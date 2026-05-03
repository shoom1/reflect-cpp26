#!/usr/bin/env bash
# Run ctest inside the pinned clang-p2996 container.
set -euo pipefail

IMAGE="${REFLECT_DOCKER_IMAGE:-reflect-cpp26-dev:source}"
PRESET="${1:-docker}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

docker run --rm \
    --network=none \
    -v "$REPO_ROOT":/work \
    -w /work \
    "$IMAGE" \
    ctest --preset "$PRESET"
