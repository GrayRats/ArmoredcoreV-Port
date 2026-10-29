FROM node:24-bookworm-slim AS node
FROM ubuntu:24.04
LABEL org.opencontainers.image.title="ACV Linux build tools"
LABEL org.opencontainers.image.description="Clang 18 and dependencies for ACV Linux source builds; no game files"
LABEL org.opencontainers.image.source="https://github.com/GrayRats/ArmoredcoreV-Port"
COPY --from=node /usr/local/ /usr/local/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates git git-lfs build-essential clang-18 lld-18 cmake ninja-build \
    python3 pkg-config curl unzip xz-utils zstd patchelf \
    libvulkan-dev libgl1-mesa-dev libgl1-mesa-dri libegl1-mesa-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev \
    libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev \
    fonts-dejavu-core xvfb xauth xdotool imagemagick \
    && ln -s /usr/bin/clang-18 /usr/local/bin/clang \
    && ln -s /usr/bin/clang++-18 /usr/local/bin/clang++ \
    && ln -s /usr/bin/lld-18 /usr/local/bin/lld \
    && rm -rf /var/lib/apt/lists/*
ENV CC=clang CXX=clang++
WORKDIR /work
CMD ["bash"]
