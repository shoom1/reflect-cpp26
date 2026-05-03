# Benchmarks

## What's here

`bench_json.cpp` measures `reflect::to_json` / `from_json` on small,
medium, large, and nested structs, plus enum string conversion. Build
and run via:

```bash
./scripts/docker-build.sh docker-release
./scripts/docker-run.sh reflect_bench_json
```

## What's intentionally missing: comparison numbers

Numbers vs. nlohmann/json, magic_enum, Boost.PFR, and glaze are **not**
in the default build, because the pinned toolchain image doesn't ship
those libraries. Adding them would bloat the image and tie the
benchmark target to specific dependency versions.

To add a comparison locally:

1. Install the library inside the container (one-off, not committed):
   ```bash
   docker run -it --rm reflect-cpp26-dev:local \
       bash -c "apt-get update && apt-get install -y nlohmann-json3-dev"
   ```
2. Uncomment the `#include <nlohmann/json.hpp>` line at the top of
   `bench_json.cpp` and add comparison cases alongside the existing
   `reflect::to_json` calls.
3. Re-run `./scripts/docker-build.sh docker-release`.

Numbers should be published after the toolchain stabilizes (mainline
compilers ship P2996, optimization is mature). Until then, comparisons
are noise — Bloomberg's clang fork is not optimized for codegen the
way mainline Clang is.

## Methodology notes

- Always build with `docker-release` (Release + benchmarks). Debug
  builds are 10–100× slower and meaningless.
- `bench()` runs a 10% warmup pass before the timed loop.
- Numbers are nanoseconds per operation. Lower is better.
- Run on a quiet machine; close browsers, IDEs, etc.
