#!/usr/bin/env bash
# Verify that the clang-p2996 image tag pinned in .github/workflows/ci.yml
# matches the upstream P2996 ref pinned in Dockerfile. Catches the human
# error of bumping one without the other (drift between the two files).
set -euo pipefail

DOCKERFILE_REF=$(grep -E '^ARG P2996_REF=' Dockerfile | head -1 | cut -d= -f2)
EXPECTED_TAG="p2996-${DOCKERFILE_REF:0:12}"

# Check every clang-p2996 tag reference in ci.yml — there are multiple
# container jobs, and one could drift while another stays correct.
count=0
bad=0
while IFS= read -r tag; do
    count=$((count + 1))
    if [ "$tag" != "$EXPECTED_TAG" ]; then
        echo "ERROR: ci.yml has clang-p2996:$tag" >&2
        bad=1
    fi
done < <(grep -oE 'clang-p2996:[A-Za-z0-9._-]+' .github/workflows/ci.yml | cut -d: -f2)

if [ "$count" -eq 0 ]; then
    echo "ERROR: no clang-p2996:<tag> references found in ci.yml" >&2
    exit 1
fi

if [ "$bad" -ne 0 ]; then
    echo "       Dockerfile pins P2996_REF=$DOCKERFILE_REF" >&2
    echo "       expected all ci.yml tags to be: $EXPECTED_TAG" >&2
    exit 1
fi

echo "OK: $count ci.yml tag(s) all match Dockerfile P2996_REF '$DOCKERFILE_REF'"
