# Dockerfile for testing Sidechain plugin Linux builds
#
# Usage with docker-compose (from plugin/):
#   docker-compose up --build
#   docker-compose run --rm plugin   # for interactive
#
# Usage standalone (from project root):
#   docker build -f plugin/Dockerfile -t sidechain-plugin .
#   docker run --rm -v $(pwd):/sidechain:ro -v sidechain-build:/build -v sidechain-cache:/cache sidechain-plugin

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies (cached layer)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    ccache \
    git \
    pkg-config \
    libasound2-dev \
    libjack-jackd2-dev \
    ladspa-sdk \
    libcurl4-openssl-dev \
    libfreetype6-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxext-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxrender-dev \
    libwebkit2gtk-4.1-dev \
    libglu1-mesa-dev \
    mesa-common-dev \
    libgtk-3-dev \
    libfftw3-dev \
    libssl-dev \
    clang \
    lld \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++
ENV CCACHE_DIR=/cache/ccache
ENV BUILD_DIR=/build
ENV CACHE_DIR=/cache

WORKDIR /sidechain

# Build script - uses external volumes for build and cache
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
# Ensure cache directories exist\n\
mkdir -p "$BUILD_DIR" "$CACHE_DIR/ccache" "$CACHE_DIR/cmake"\n\
\n\
# Show ccache stats at start\n\
echo "==> ccache stats (before):"\n\
ccache -s 2>/dev/null | head -5 || true\n\
\n\
# Configure if needed (check for build.ninja in the volume)\n\
if [ ! -f "$BUILD_DIR/build.ninja" ]; then\n\
    echo ""\n\
    echo "==> Configuring CMake (first run)..."\n\
    cmake -S /sidechain/plugin -B "$BUILD_DIR" \\\n\
        -G Ninja \\\n\
        -DCMAKE_BUILD_TYPE=Release \\\n\
        -DSIDECHAIN_BUILD_STANDALONE=ON \\\n\
        -DSIDECHAIN_BUILD_PLUGIN_HOST=OFF \\\n\
        -DSIDECHAIN_DEPS_CACHE_DIR="$CACHE_DIR/cmake"\n\
else\n\
    echo "==> Using cached CMake config (incremental build)"\n\
fi\n\
\n\
echo ""\n\
echo "==> Building..."\n\
cmake --build "$BUILD_DIR" --parallel\n\
\n\
echo ""\n\
echo "==> ccache stats (after):"\n\
ccache -s 2>/dev/null | grep -E "^(Hits|Misses|Hit rate)" || true\n\
\n\
echo ""\n\
echo "==> Build complete!"\n\
ls -la "$BUILD_DIR/src/core/Sidechain_artefacts/Release/" 2>/dev/null || echo "Artifacts not found"\n\
' > /build.sh && chmod +x /build.sh

CMD ["/build.sh"]
