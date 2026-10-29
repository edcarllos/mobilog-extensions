# The tool end to end: new, build, check and test on both templates, and the refusals (needs
# wasi-sdk: scripts/setup.sh --sdk). The format library itself is tested in the Mobilog app.
if(NOT WIN32)
  add_test(NAME mobilog_ext_selftest
           COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/selftest.sh $<TARGET_FILE:mobilog_ext> ${PROJECT_SOURCE_DIR})
endif()
