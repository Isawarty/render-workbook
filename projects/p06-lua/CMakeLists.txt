rwb_require(lua glm)

add_library(p06_lib STATIC
  src/LuaVm.cpp
  src/LuaMath.cpp
  src/SceneScript.cpp
  src/LuaRenderGraph.cpp
  src/ScriptRuntime.cpp)
target_include_directories(p06_lib PUBLIC src)
target_link_libraries(p06_lib
  PUBLIC lua::lua glm::glm rwb::rendergraph
  PRIVATE rwb::warnings)

add_executable(p06_tests tests/test_p06.cpp)
target_link_libraries(p06_tests PRIVATE p06_lib rwb::testfw rwb::warnings)
add_dependencies(p06_tests rwb_lua_cli)

add_test(NAME p06-t00
  COMMAND $<TARGET_FILE:rwb_lua_cli> ${CMAKE_CURRENT_SOURCE_DIR}/language/runner.lua)
set_tests_properties(p06-t00 PROPERTIES
  LABELS "l2;p06"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  TIMEOUT 30)

foreach(NN 01 02 03 04 05)
  rwb_add_test(p06-t${NN} TARGET p06_tests LABELS "l2;p06" ARGS "[t${NN}]")
  set_tests_properties(p06-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
