#!/usr/bin/env bash
# Verify that the clang-p2996 image tag pinned in .github/workflows/ci.yml
# matches the upstream P2996 ref pinned in Dockerfile. Catches the human
# error of bumping one without the other (drift between the two files).
set -euo pipefail

DOCKERFILE_REF=$(grep -E '^ARG P2996_REF=' Dockerfile | head -1 | cut -d= -f2)
EXPECTED_TAG="p2996-${DOCKERFILE_REF:0:12}"
ACTUAL_TAG=$(grep -oE 'clang-p2996:[A-Za-z0-9._-]+' .github/workflows/ci.yml \
             | head -1 | cut -d: -f2)

if [ "$EXPECTED_TAG" != "$ACTUAL_TAG" ]; then
    echo "ERROR: ci.yml pulls clang-p2996:$ACTUAL_TAG" >&2
    echo "       Dockerfile pins P2996_REF=$DOCKERFILE_REF" >&2
    echo "       expected ci.yml tag: $EXPECTED_TAG" >&2
    exit 1
fi

echo "OK: ci.yml tag '$ACTUAL_TAG' matches Dockerfile P2996_REF '$DOCKERFILE_REF'"
