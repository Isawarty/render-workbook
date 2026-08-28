# P3 —— Compute Shader 专项。
#
# 每题输出 buffer，经 L2 回读与 CPU 参考实现逐元素比对；不依赖窗口或 golden image。
rwb_require(vma)

set(P03_STEP_SOURCES
  src/steps/01_saxpy.cpp
  src/steps/02_reduce.cpp
  src/steps/03_scan.cpp
  src/steps/04_bitonic.cpp
  src/steps/05_postprocess.cpp)

add_library(p03_lib STATIC
  src/ComputeApp.cpp
  ${P03_STEP_SOURCES})
target_include_directories(p03_lib PUBLIC src)
target_link_libraries(p03_lib PUBLIC rwb::engine PRIVATE rwb::warnings vma)

rwb_add_shaders(p03_lib SHADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/saxpy.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/reduce_shared.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/reduce_subgroup.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scan_block.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scan_add_offsets.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bitonic.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.frag)

set(P03_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders/p03_lib")
target_compile_definitions(p03_lib PUBLIC RWB_SHADER_DIR="${P03_SHADER_DIR}")

add_executable(p03_compute src/main.cpp)
target_link_libraries(p03_compute PRIVATE p03_lib rwb::warnings)

add_executable(p03_tests tests/test_p03.cpp)
target_link_libraries(p03_tests PRIVATE p03_lib rwb::testfw rwb::warnings)

foreach(NN 01 02 03 04 05)
  rwb_add_test(p03-t${NN} TARGET p03_tests LABELS "l1;l2;p03" ARGS "[t${NN}]")
endforeach()
