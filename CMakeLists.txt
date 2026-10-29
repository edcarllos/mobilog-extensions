# Mobilog extensions: the package format (mlx/), the SDK (sdk/), the tool mobilog_ext (tools/) and
# the extensions themselves (extensions/<id>, built to WebAssembly by `mobilog_ext build`).
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
#   → build/mobilog_ext (and the tests: ctest --test-dir build)
cmake_minimum_required(VERSION 3.24)
project(mobilog_extensions LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MLX_TEST_HOST "mobilog_ext test: runs extensions without the app (WAMR interpreter, fetched)" ON)
option(MLX_TESTS "Unit tests of the format and the tool" ON)

add_subdirectory(mlx)
add_subdirectory(sdk/contracts)
add_subdirectory(tools/mobilog_ext)
if(MLX_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
