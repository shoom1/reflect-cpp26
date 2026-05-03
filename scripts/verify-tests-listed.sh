#!/usr/bin/env bash
# Verify every tests/test_*.cpp file on disk is listed in
# CMakeLists.txt's REFLECT_TEST_SOURCES. Without this check, adding a
# new test file but forgetting to register it in CMake silently skips
# the test — the build stays green and ctest reports the same number
# of tests, just minus the new one.
set -euo pipefail

ON_DISK=$(find tests -maxdepth 1 -name 'test_*.cpp' | sort)
LISTED=$(grep -oE 'tests/test_[A-Za-z0-9_]+\.cpp' CMakeLists.txt | sort -u)

MISSING=$(comm -23 <(echo "$ON_DISK") <(echo "$LISTED"))
if [ -n "$MISSING" ]; then
    echo "ERROR: test files on disk but not listed in CMakeLists.txt:" >&2
    echo "$MISSING" >&2
    echo "Add them to REFLECT_TEST_SOURCES." >&2
    exit 1
fi

# Inverse check: catch listed entries whose file no longer exists.
STALE=$(comm -13 <(echo "$ON_DISK") <(echo "$LISTED"))
if [ -n "$STALE" ]; then
    echo "ERROR: CMakeLists.txt lists test files that don't exist:" >&2
    echo "$STALE" >&2
    exit 1
fi

echo "OK: all test files are registered in CMakeLists.txt."
