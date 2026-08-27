# P1 —— 三角形。全套里唯一一次从零写 Vulkan 初始化。
#
# 源码按 task 切成 src/steps/0N_*.cpp，每个文件对应一道题。
# 这样 `git diff done/p01-t03 -- src/steps/03_swapchain.cpp` 就只看得到那一题的答案。

set(P01_STEP_SOURCES
  src/steps/01_instance.cpp
  src/steps/02_device.cpp
  src/steps/03_swapchain.cpp
  src/steps/04_renderpass.cpp
  src/steps/05_pipeline.cpp
  src/steps/06_commands.cpp
  src/steps/07_sync.cpp
  src/steps/08_resize.cpp
  src/steps/09_frames_in_flight.cpp)

# 核心逻辑做成库，让 exe 和测试都能链接同一份代码 ——
# 测试驱动的是你真正写的那份实现，不是复制品。
add_library(p01_lib STATIC
  src/TriangleApp.cpp
  src/Capture.cpp
  ${P01_STEP_SOURCES})

target_include_directories(p01_lib PUBLIC src)
target_link_libraries(p01_lib PUBLIC rwb::engine PRIVATE rwb::warnings)
rwb_add_shaders(p01_lib SHADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag)

# rwb_add_shaders 把 RWB_SHADER_DIR 定义成 PRIVATE，
# 但测试目标也要读同一批 .spv，所以再显式传一次。
set(P01_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders/p01_lib")
target_compile_definitions(p01_lib PUBLIC RWB_SHADER_DIR="${P01_SHADER_DIR}")

add_executable(p01_triangle src/main.cpp)
target_link_libraries(p01_triangle PRIVATE p01_lib rwb::warnings)

add_executable(p01_tests tests/test_p01.cpp)
target_link_libraries(p01_tests PRIVATE p01_lib rwb::testfw rwb::warnings)

# 每道题一个 ctest 条目：`ctest -R p01-t03` 只跑第三题。
foreach(NN 01 02 03 04 05 06 07 08 09)
  rwb_add_test(p01-t${NN} TARGET p01_tests LABELS "l1;l3;p01" ARGS "[t${NN}]")
endforeach()
