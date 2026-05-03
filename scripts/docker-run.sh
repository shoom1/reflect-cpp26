#!/usr/bin/env bash
# Run a built target inside the pinned clang-p2996 container.
#
# The binaries in build-docker/ are Linux ELFs and only execute inside the
# container; this wrapper is the friction-free way to invoke them from macOS.
#
# Usage:
#   scripts/docker-run.sh reflect_example_json
#   scripts/docker-run.sh reflect_example_args -- --help
#   scripts/docker-run.sh reflect_test_enum                # ctest target binary
#
# Env overrides:
#   REFLECT_DOCKER_IMAGE   image tag to use (default: reflect-cpp26-dev:local)
#   REFLECT_BUILD_DIR      build directory inside the repo (default: build-docker)
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <target-name> [args...]" >&2
    echo "Example: $0 reflect_example_json" >&2
    exit 64
fi

IMAGE="${REFLECT_DOCKER_IMAGE:-reflect-cpp26-dev:source}"
BUILD_DIR="${REFLECT_BUILD_DIR:-build-docker}"
TARGET="$1"
shift

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Targets land in subdirectories of the build tree; locate the binary.
BIN_REL="$(cd "$REPO_ROOT" && find "$BUILD_DIR" -type f -perm -u+x -name "$TARGET" 2>/dev/null | head -n 1 || true)"

if [[ -z "$BIN_REL" ]]; then
    echo "Binary '$TARGET' not found under $BUILD_DIR/." >&2
    echo "Did you run scripts/docker-build.sh?" >&2
    exit 1
fi

docker run --rm -i \
    --network=none \
    -v "$REPO_ROOT":/work \
    -w /work \
    "$IMAGE" \
    "/work/$BIN_REL" "$@"
