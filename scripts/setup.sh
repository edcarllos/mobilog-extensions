#!/usr/bin/env bash
# Prepares this repository for writing extensions (Linux and macOS): downloads the pinned wasi-sdk
# (checked by SHA-256) into external/wasi-sdk and builds mobilog_ext into build/.
#   scripts/setup.sh            everything
#   scripts/setup.sh --sdk      only wasi-sdk
# Needs: curl, tar, cmake (3.24+), a C++20 compiler; ninja if available.
set -euo pipefail
cd "$(dirname "$0")/.."

WASI_SDK_VERSION=34
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)  os=x86_64-linux;  sha=b761e3a0721dbae9c09a0059e5fdb2bf917d1b4a8a7b430fb3b5aafb0984b2c4 ;;
  Linux-aarch64|Linux-arm64) os=arm64-linux; sha=f7e243dff54d60bcc576e94d6166b69f410f2500ae4a9ceef34315be10e77971 ;;
  Darwin-x86_64) os=x86_64-macos;  sha=87d27fa8adc68dee59bfbf2e22a6d34ef717c34d6bf1d8af2a56fc929d9ce0eb ;;
  Darwin-arm64)  os=arm64-macos;   sha=9c59398106b417f8f14913380fdf0097a8cc0ff4af9eb3ce0065a859e88d49e9 ;;
  *) echo "sistema não suportado por este script / unsupported system: $(uname -s)-$(uname -m) (Windows: scripts/setup.ps1)" >&2; exit 1 ;;
esac
asset="wasi-sdk-${WASI_SDK_VERSION}.0-${os}.tar.gz"

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }

if [ -x external/wasi-sdk/bin/clang ] && grep -q "^${WASI_SDK_VERSION}\." external/wasi-sdk/VERSION 2>/dev/null; then
  echo "wasi-sdk ${WASI_SDK_VERSION}: já está em external/wasi-sdk"
else
  mkdir -p external/downloads
  if [ ! -f "external/downloads/$asset" ] || [ "$(sha256 "external/downloads/$asset")" != "$sha" ]; then
    echo "Baixando / downloading $asset (~190 MB)..."
    curl -fL --retry 3 -o "external/downloads/$asset.part" \
      "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VERSION}/${asset}"
    mv "external/downloads/$asset.part" "external/downloads/$asset"
  fi
  [ "$(sha256 "external/downloads/$asset")" = "$sha" ] || { echo "wasi-sdk: SHA-256 não confere / mismatch" >&2; rm -f "external/downloads/$asset"; exit 1; }
  rm -rf external/wasi-sdk && mkdir -p external/wasi-sdk
  tar -xzf "external/downloads/$asset" -C external/wasi-sdk --strip-components=1
  echo "wasi-sdk ${WASI_SDK_VERSION}: external/wasi-sdk"
fi
[ "${1:-}" = "--sdk" ] && exit 0

gen=()
command -v ninja >/dev/null && gen=(-G Ninja)
cmake -S . -B build "${gen[@]}" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --target mobilog_ext
echo
echo "Pronto / ready: $(pwd)/build/tools/mobilog_ext/mobilog_ext"
echo "  export PATH=\"$(pwd)/build/tools/mobilog_ext:\$PATH\""
echo "  mobilog_ext new io.github.<seu-usuario>.<nome>"
