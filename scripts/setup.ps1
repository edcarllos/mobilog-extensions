# Prepares this repository for writing extensions on Windows: downloads the pinned wasi-sdk (checked
# by SHA-256) into external\wasi-sdk and builds mobilog_ext into build\.
#   powershell -ExecutionPolicy Bypass -File scripts\setup.ps1
# Needs: tar (Windows 10+), cmake 3.24+, Visual Studio 2022 (C++), or Ninja + clang.
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$version = 34
$arch = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "arm64" } else { "x86_64" }
$pins = @{
  "x86_64" = "cccb5c323a9b34f0349a9b09e8804a0a7632c68c3310f4b5f437ed57d7e71d8f";
  "arm64"  = "45e1c71f3e965621e7b98ebe1d37b0e4b1f77f3e8072113ffb4534e67b1a4b7c"
}
$asset = "wasi-sdk-$version.0-$arch-windows.tar.gz"
$file = "external\downloads\$asset"
if (-not (Test-Path "external\wasi-sdk\bin\clang.exe")) {
  New-Item -ItemType Directory -Force external\downloads | Out-Null
  if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $pins[$arch]) {
    Write-Host "Baixando / downloading $asset (~620 MB)..."
    Invoke-WebRequest "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-$version/$asset" -OutFile $file
  }
  if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $pins[$arch]) { Remove-Item $file; throw "wasi-sdk: SHA-256 mismatch" }
  if (Test-Path external\wasi-sdk) { Remove-Item -Recurse -Force external\wasi-sdk }
  New-Item -ItemType Directory -Force external\wasi-sdk | Out-Null
  tar -xzf $file -C external\wasi-sdk --strip-components=1
}
Write-Host "wasi-sdk $version: external\wasi-sdk"
if ($args -contains "--sdk") { exit 0 }
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release | Out-Null
cmake --build build --config Release --target mobilog_ext
Write-Host "Pronto / ready: build\tools\mobilog_ext"
