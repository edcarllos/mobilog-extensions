# Toolchain for Mobilog extensions: WebAssembly 1.0 plus the features the app accepts (bulk-memory,
# sign-ext, nontrapping-fptoint, mutable-globals), nothing else (no SIMD, threads, exceptions).
#
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/wasi-toolchain.cmake
#
# Compiler: wasi-sdk's clang when WASI_SDK_PREFIX is set (with its C/C++ library), else the clang of
# emsdk (EMSDK, or ~/emsdk) without a C library: then the code uses only the SDK (mobilog_ext.h,
# MLX_DEFINE_ALLOCATOR) — that is how the examples are built.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR wasm32)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT WASI_SDK_PREFIX AND DEFINED ENV{WASI_SDK_PREFIX})
  set(WASI_SDK_PREFIX "$ENV{WASI_SDK_PREFIX}")
endif()
if(WASI_SDK_PREFIX)
  set(_bin "${WASI_SDK_PREFIX}/bin")
  set(MOBILOG_EXT_LIBC ON CACHE BOOL "The C/C++ library of wasi-sdk is available" FORCE)
  set(_target wasm32-wasip1)
  set(CMAKE_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot")
else()
  if(DEFINED ENV{EMSDK})
    set(_bin "$ENV{EMSDK}/upstream/bin")
  else()
    set(_bin "$ENV{HOME}/emsdk/upstream/bin")
  endif()
  set(MOBILOG_EXT_LIBC OFF CACHE BOOL "The C/C++ library of wasi-sdk is available" FORCE)
  set(_target wasm32)
endif()
set(CMAKE_C_COMPILER "${_bin}/clang")
set(CMAKE_CXX_COMPILER "${_bin}/clang++")
set(CMAKE_C_COMPILER_TARGET ${_target})
set(CMAKE_CXX_COMPILER_TARGET ${_target})

set(_features "-mcpu=mvp -mbulk-memory -msign-ext -mnontrapping-fptoint -mmutable-globals")
set(CMAKE_C_FLAGS_INIT "${_features}")
set(CMAKE_CXX_FLAGS_INIT "${_features} -fno-exceptions -fno-rtti")
if(NOT MOBILOG_EXT_LIBC)
  string(APPEND CMAKE_C_FLAGS_INIT " -nostdlib -fno-builtin")
  string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdlib -fno-builtin")
endif()
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--no-entry -Wl,--strip-all")
if(MOBILOG_EXT_LIBC)
  # A reactor (no main); no wasm-opt from the PATH (the result would depend on the machine);
  # call_indirect's table index in one byte (wasi-sdk's libraries leave 5, which the app refuses).
  string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -mexec-model=reactor --no-wasm-opt -Wl,--compress-relocations")
endif()
set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
