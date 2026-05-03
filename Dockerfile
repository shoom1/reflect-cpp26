# Toolchain image for reflect_cpp26 — used by both local dev (via
# scripts/docker-build.sh) and CI (via .github/workflows/build-image.yml).
#
# Builds Bloomberg's clang-p2996 fork from source. Trust chain:
#   Docker Desktop / GH runner → ubuntu:24.04 → bloomberg/clang-p2996 source.
#
# Cost on default settings:
#   - First build: ~30–90 minutes (depends on COMPILE_JOBS + RAM)
#   - Disk during build: ~25 GB intermediate, ~4–5 GB final image
#
# Build (the wrapper script does this automatically when missing):
#   ./scripts/docker-build.sh
#
# Power-user knobs (build args):
#   P2996_REF=<branch|sha>   pin to a specific upstream commit
#   COMPILE_JOBS=N           parallel compile jobs (default 2 — safe for
#                            Docker Desktop default RAM; bump to 4–8 if
#                            you bumped Docker Desktop RAM allocation)
#   LINK_JOBS=N              parallel link jobs (default 1 — linking is
#                            the heaviest RAM phase, ~4–8 GB per job)
#
# Example fast rebuild on a beefy local box:
#   docker build --build-arg COMPILE_JOBS=8 --build-arg LINK_JOBS=4 \
#                -t reflect-cpp26-dev:source .

# ---------------------------------------------------------------------------
# Builder stage: compile clang-p2996 from source.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        python3 \
        ca-certificates \
        libxml2-dev \
        zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

# Pinned to a specific upstream commit for reproducible builds. Bumping
# this is a deliberate three-step change:
#   1. update the SHA below
#   2. rebuild via the "Build clang-p2996 image" workflow
#      (it tags the image :p2996-<short SHA>)
#   3. update the matching tag in .github/workflows/ci.yml
#
# To find the latest commit on the p2996 branch:
#   git ls-remote https://github.com/bloomberg/clang-p2996.git refs/heads/p2996
ARG P2996_REF=9ffb96e3ce362289008e14ad2a79a249f58aa90a
ARG INSTALL_PREFIX=/opt/clang-p2996
ARG COMPILE_JOBS=2
ARG LINK_JOBS=1

WORKDIR /src

# Shallow-clone the p2996 branch tip (~80% smaller than full history),
# then fetch and check out the pinned commit. Direct shallow clone of an
# arbitrary SHA isn't supported by GitHub's smart-HTTP protocol, so the
# branch hop is necessary.
RUN git clone --depth 1 --branch p2996 \
        https://github.com/bloomberg/clang-p2996.git . \
 && git fetch --depth 1 origin "${P2996_REF}" \
 && git checkout "${P2996_REF}"

# Configure: clang frontend only, libc++/libc++abi/libunwind runtimes,
# x86_64 target only, Release mode. This trims the build by an order of
# magnitude vs a full LLVM build.
RUN cmake -S llvm -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
        -DLLVM_ENABLE_PROJECTS="clang" \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
        -DLLVM_TARGETS_TO_BUILD="X86" \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_ENABLE_ASSERTIONS=OFF \
        -DCLANG_INCLUDE_TESTS=OFF \
        -DLIBCXX_INCLUDE_TESTS=OFF \
        -DLIBCXXABI_INCLUDE_TESTS=OFF \
        -DLIBUNWIND_INCLUDE_TESTS=OFF \
        -DLLVM_PARALLEL_LINK_JOBS=${LINK_JOBS} \
        -DLLVM_PARALLEL_COMPILE_JOBS=${COMPILE_JOBS}

# COMPILE_JOBS / LINK_JOBS conservatism keeps peak RAM under ~6 GB on the
# default Docker Desktop allocation. Bump both (and Docker Desktop's RAM)
# for a faster rebuild — see the header comment for examples.
RUN cmake --build build --target install --parallel ${COMPILE_JOBS} \
 && rm -rf /src/build /src/.git

# ---------------------------------------------------------------------------
# Runtime stage: thin image with just the toolchain.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
# build-essential: clang relies on gcc's libgcc startup files (crtbeginS.o,
# crtendS.o) and the system linker for the final link step. Multi-stage
# trimming dropped these by default. ~200 MB cost; alternative would be
# building compiler-rt as part of the LLVM build (another ~30 min).
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        libxml2 \
        zlib1g \
 && rm -rf /var/lib/apt/lists/*

ARG INSTALL_PREFIX=/opt/clang-p2996
COPY --from=builder ${INSTALL_PREFIX} ${INSTALL_PREFIX}

# Put clang on PATH and register libc++ with the dynamic linker.
ENV PATH=${INSTALL_PREFIX}/bin:${PATH}
ENV CC=clang
ENV CXX=clang++
RUN set -e \
 && LIBCXX="$(clang++ -print-file-name=libc++.so.1)" \
 && test -f "$LIBCXX" \
 && dirname "$LIBCXX" > /etc/ld.so.conf.d/clang-p2996.conf \
 && ldconfig

# Make clang++ default to libc++ instead of GCC's libstdc++. Without this,
# CMake's compiler check fails ("cannot find -lstdc++") because we
# deliberately did not install libstdc++-dev to keep the image slim.
# The .cfg file is auto-loaded for any `clang++` invocation; `clang` (C
# compiler) is unaffected.
RUN echo '-stdlib=libc++' > ${INSTALL_PREFIX}/bin/clang++.cfg

# Sanity check: clang understands -freflection-latest, can find its libc++
# headers, and the resulting binary actually loads at runtime. Catches
# broken images before they get tagged or pushed to a registry.
RUN printf '#include <meta>\nint main(){}\n' > /tmp/t.cpp \
 && clang++ -std=c++26 -freflection-latest /tmp/t.cpp -o /tmp/t \
 && /tmp/t \
 && rm /tmp/t.cpp /tmp/t

WORKDIR /work
