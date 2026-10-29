# Para quem prefere CMake ao `mobilog_ext build` (que dispensa CMake e é o que a loja usa):
#
# mobilog_ext_add(<nome> SOURCES <arquivos...> MANIFEST <manifest.json> [HELP <help.md>] [ICON <ícone>]
#                 [MEMORY_MB <n>] [KEY <chave.secret>] [VRP])
#
# Compila a extensão (<nome>.wasm) e a empacota em <nome>.mlx com o mobilog_ext (MOBILOG_EXT_TOOL, ou
# achado no PATH). MEMORY_MB deve ser o mesmo de "limits.memoryMB" do manifesto (padrão 64). KEY assina.
# VRP: junta o modelo de roteirização do SDK (resolvedores de rotas).
set(MOBILOG_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
find_program(MOBILOG_EXT_TOOL mobilog_ext)

function(mobilog_ext_add name)
  cmake_parse_arguments(A "VRP" "MANIFEST;HELP;ICON;MEMORY_MB;KEY" "SOURCES" ${ARGN})
  if(NOT A_MEMORY_MB)
    set(A_MEMORY_MB 64)
  endif()
  math(EXPR bytes "${A_MEMORY_MB} * 1048576")
  if(A_VRP)
    list(APPEND A_SOURCES "${MOBILOG_SDK_DIR}/contracts/src/Vrp.cpp" "${MOBILOG_SDK_DIR}/contracts/src/VrpWire.cpp")
  endif()
  add_executable(${name} ${A_SOURCES})
  set_target_properties(${name} PROPERTIES SUFFIX ".wasm")
  target_include_directories(${name} PRIVATE "${MOBILOG_SDK_DIR}/include" "${MOBILOG_SDK_DIR}/contracts/include"
                                             "${MOBILOG_SDK_DIR}/../mlx/include")
  target_link_options(${name} PRIVATE "-Wl,--max-memory=${bytes}")
  set(stage "${CMAKE_CURRENT_BINARY_DIR}/${name}.pacote")
  set(files "${A_MANIFEST}")
  if(A_HELP)
    list(APPEND files "${A_HELP}")
  endif()
  if(A_ICON)
    list(APPEND files "${A_ICON}")
  endif()
  set(sign "")
  if(A_KEY)
    set(sign --key "${A_KEY}")
  endif()
  if(NOT MOBILOG_EXT_TOOL)
    message(WARNING "mobilog_ext não encontrado (MOBILOG_EXT_TOOL): só o .wasm será gerado")
    return()
  endif()
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.mlx"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${stage}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${stage}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}> "${stage}/extension.wasm"
    COMMAND ${CMAKE_COMMAND} -E copy ${files} "${stage}/"
    COMMAND "${MOBILOG_EXT_TOOL}" pack "${stage}" -o "${CMAKE_CURRENT_BINARY_DIR}/${name}.mlx" ${sign}
    DEPENDS ${name} ${files}
    COMMENT "Empacotando ${name}.mlx"
    VERBATIM)
  add_custom_target(${name}_mlx ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.mlx")
endfunction()
