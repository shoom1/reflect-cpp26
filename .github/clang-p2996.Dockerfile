# Dockerfile for Bloomberg's Clang P2996 fork + matching libc++.
#
# This image is consumed by .github/workflows/ci.yml. The first build
# compiles LLVM/Clang from source (~45-60 min on a GitHub-hosted runner);
# subsequent builds reuse the GitHub Actions layer cache and are fast as
# long as P2996_REF and the apt package set don't change.
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

# Build clang + the matching libc++ (stdlib reflection headers ship in
# libc++, NOT libstdc++, so users must compile with -stdlib=libc++).
RUN cmake -S llvm -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-p2996 \
 && cmake --build build --target install \
 && cmake --build build --target install-cxx install-cxxabi \
 && rm -rf /src

ENV PATH=/opt/clang-p2996/bin:$PATH
ENV CC=clang
ENV CXX=clang++

# Sanity check: the toolchain understands -freflection-latest.
RUN echo '#include <meta>\nint main(){}' > /tmp/t.cpp \
 && clang++ -std=c++26 -stdlib=libc++ -freflection-latest \
        -I/opt/clang-p2996/include/c++/v1 \
        -L/opt/clang-p2996/lib -Wl,-rpath,/opt/clang-p2996/lib \
        /tmp/t.cpp -o /tmp/t \
 && /tmp/t \
 && rm /tmp/t.cpp /tmp/t

WORKDIR /work
