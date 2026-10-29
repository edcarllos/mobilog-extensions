# mobilog_ext: the command-line tool for extension authors and the store.
include(FetchContent)

if(NOT TARGET nanosvg)
  # The app's SVG renderer: `check` draws the icon the way the app will (same commit as the app).
  FetchContent_Declare(nanosvg GIT_REPOSITORY https://github.com/memononen/nanosvg.git
                       GIT_TAG 239e102ec2c691f2902e20ace2ed36ee4a35cfe6 SOURCE_SUBDIR none)
  FetchContent_MakeAvailable(nanosvg)
  add_library(nanosvg INTERFACE)
  target_include_directories(nanosvg SYSTEM INTERFACE ${nanosvg_SOURCE_DIR}/src)
endif()

add_executable(mobilog_ext main.cpp Common.cpp Pack.cpp Build.cpp Check.cpp Catalog.cpp TestHost.cpp Doc.cpp)
target_link_libraries(mobilog_ext PRIVATE mlx_format mlx_json mobilog_vrp nanosvg)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mobilog_ext PRIVATE -Wall -Wextra)
endif()

# `mobilog_ext test` runs extensions in WAMR's interpreter (no JIT: the same everywhere, slow but
# enough to see an extension start, answer its events and stop). Off: the command says so.
if(MLX_TEST_HOST)
  FetchContent_Declare(wamr GIT_REPOSITORY https://github.com/bytecodealliance/wasm-micro-runtime.git
                       GIT_TAG WAMR-2.4.5 SOURCE_SUBDIR none)
  FetchContent_MakeAvailable(wamr)
  add_subdirectory(wamr ${CMAKE_CURRENT_BINARY_DIR}/wamr)
  target_link_libraries(mobilog_ext PRIVATE mlx_wamr)
  target_compile_definitions(mobilog_ext PRIVATE MLX_TEST_HOST=1)
endif()
