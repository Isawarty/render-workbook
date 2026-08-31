rwb_require(imgui)

add_library(p05_imgui_backend STATIC
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(p05_imgui_backend PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(p05_imgui_backend PRIVATE
  IMGUI_IMPL_VULKAN_USE_VOLK
  # ImGui's GLFW backend includes glfw3.h directly. This Vulkan-only target
  # must not let GLFW pull in platform OpenGL headers such as Linux GL/gl.h.
  GLFW_INCLUDE_NONE)
target_link_libraries(p05_imgui_backend PUBLIC imgui glfw volk::volk PRIVATE rwb::warnings)

add_library(p05_lib STATIC src/RenderGraphApp.cpp src/GraphOverlay.cpp)
target_include_directories(p05_lib PUBLIC src)
target_link_libraries(p05_lib PUBLIC rwb::rendergraph p04_lib
  PRIVATE p05_imgui_backend rwb::warnings)

add_executable(p05_render_graph src/main.cpp)
target_link_libraries(p05_render_graph PRIVATE p05_lib rwb::warnings)

add_executable(p05_tests tests/test_p05.cpp)
target_link_libraries(p05_tests PRIVATE p05_lib rwb::testfw rwb::warnings)

foreach(NN 01 02 03 04 05 06)
  rwb_add_test(p05-t${NN} TARGET p05_tests LABELS "l1;l2;p05" ARGS "[t${NN}]")
  set_tests_properties(p05-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
