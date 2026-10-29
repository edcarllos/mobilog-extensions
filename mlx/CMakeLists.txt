# mlx_format: the .mlx package format (zip, manifest, WebAssembly checks, signatures, catalog).
# The same library the Mobilog app uses to check what it installs, and mobilog_ext to make packages.
# Built inside the app (which already has these dependencies) or on its own (fetched here, same versions).
cmake_minimum_required(VERSION 3.24)
project(mlx_format LANGUAGES C CXX)

include(FetchContent)

if(NOT TARGET monocypher)
  # SOURCE_SUBDIR: only the sources (no CMake project of theirs is added).
  FetchContent_Declare(monocypher GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git GIT_TAG 4.0.3
                       SOURCE_SUBDIR none)
  FetchContent_MakeAvailable(monocypher)
  add_library(monocypher STATIC
    ${monocypher_SOURCE_DIR}/src/monocypher.c
    ${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c)
  target_include_directories(monocypher SYSTEM PUBLIC ${monocypher_SOURCE_DIR}/src ${monocypher_SOURCE_DIR}/src/optional)
  set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT TARGET libdeflate::libdeflate_static)
  set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
  set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(libdeflate GIT_REPOSITORY https://github.com/ebiggers/libdeflate.git GIT_TAG v1.26)
  FetchContent_MakeAvailable(libdeflate)
endif()

if(NOT MLX_JSON_INCLUDE)
  FetchContent_Declare(nlohmann_json URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
                       SOURCE_SUBDIR none)
  FetchContent_MakeAvailable(nlohmann_json)
  set(MLX_JSON_INCLUDE ${nlohmann_json_SOURCE_DIR}/single_include)
endif()

add_library(mlx_format STATIC
  src/Bytes.cpp
  src/Zip.cpp
  src/Manifest.cpp
  src/Signature.cpp
  src/WasmInspect.cpp
  src/Package.cpp
  src/Metering.cpp
  src/Catalog.cpp
  src/Official.cpp
  src/Api.cpp)
target_include_directories(mlx_format PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(mlx_format SYSTEM PRIVATE ${MLX_JSON_INCLUDE})
# nlohmann/json for the tools built with the format (mobilog_ext).
add_library(mlx_json INTERFACE)
target_include_directories(mlx_json SYSTEM INTERFACE ${MLX_JSON_INCLUDE})
target_link_libraries(mlx_format PUBLIC monocypher libdeflate::libdeflate_static)
target_compile_features(mlx_format PUBLIC cxx_std_20)
set_target_properties(mlx_format PROPERTIES POSITION_INDEPENDENT_CODE ON)
