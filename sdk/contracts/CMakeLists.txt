# mobilog_vrp: the vehicle routing problem (VRP) as the app and solver extensions share it: the
# instance and solutions (Vrp), their binary blocks (VrpWire) and the solver interface (Solver.h).
# Native here (the app, mobilog_ext, tests); solver extensions compile the same sources to wasm.
add_library(mobilog_vrp STATIC src/Vrp.cpp src/VrpWire.cpp)
target_include_directories(mobilog_vrp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mobilog_vrp PUBLIC mlx_format)
set_target_properties(mobilog_vrp PROPERTIES POSITION_INDEPENDENT_CODE ON)
