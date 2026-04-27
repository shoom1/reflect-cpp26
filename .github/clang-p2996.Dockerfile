# Dockerfile for Bloomberg's Clang P2996 fork + matching libc++.
#
# Multi-stage layering: each major step is a separate RUN so that a
# failure in (e.g.) the runtimes configure doesn't invalidate the
# 2-hour clang build cache.
#
# To rebuild manually:
#   docker build -f .github/clang-p2996.Dockerfile -t clang-p2996 .

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    python3 \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Pin to a known-good revision of the Bloomberg P2996 fork. Bump this
# when you want to track upstream; CI will exercise the new revision on
# the next PR. Override at build time with --build-arg P2996_REF=...
ARG P2996_REF=p2996

RUN git clone --depth 1 --branch ${P2996_REF} \
    https://github.com/bloomberg/clang-p2996.git /src/llvm

WORKDIR /src/llvm

# Step 1: configure. Cheap; isolating it lets cache survive bad flags.
# libunwind is required because libcxxabi defaults LIBCXXABI_USE_LLVM_UNWINDER=ON
# in this fork. Skipping runtime tests saves ~10 min and a lot of disk.
RUN cmake -S llvm -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF \
    -DLIBCXXABI_INCLUDE_TESTS=OFF \
    -DLIBUNWIND_INCLUDE_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-p2996

# Step 2: build clang and the runtimes (the long step, ~2h).
RUN cmake --build build

# Step 3: install. Fast.
RUN cmake --build build --target install \
 && rm -rf /src

ENV PATH=/opt/clang-p2996/bin:$PATH
ENV CC=clang
ENV CXX=clang++

# Sanity check: clang understands -freflection-latest and finds its own
# libc++ headers. We rely on auto-discovery rather than hardcoding paths
# because LLVM_ENABLE_PER_TARGET_RUNTIME_DIR puts headers under
# include/<triple>/c++/v1, which varies by host.
RUN printf '#include <meta>\nint main(){}\n' > /tmp/t.cpp \
 && clang++ -std=c++26 -stdlib=libc++ -freflection-latest /tmp/t.cpp -o /tmp/t \
 && /tmp/t \
 && rm /tmp/t.cpp /tmp/t

WORKDIR /work
