# Everything to write Mobilog extensions: compilers, CMake, Ninja; scripts/setup.sh then fetches the
# pinned wasi-sdk and builds mobilog_ext (postCreateCommand).
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
 && apt-get install -y --no-install-recommends build-essential cmake ninja-build curl ca-certificates git python3 \
 && rm -rf /var/lib/apt/lists/*
