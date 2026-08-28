# P2 —— 资源与场景。
#
# P1 的三角形顶点硬编码在 shader 里；这里开始建真正的数据管线：
# 显存分配、staging 上传、descriptor、纹理、深度、模型加载、多重采样。
#
# 和 P1 一样，源码按 task 切成 src/steps/0N_*.cpp，
# 于是 `git diff done/p02-t04 -- src/steps/04_texture.cpp` 只看得到那一题的答案。
rwb_require(vma tinygltf)

set(P02_STEP_SOURCES
  src/steps/01_buffers.cpp
  src/steps/02_uniforms.cpp
  src/steps/03_pushconstants.cpp
  src/steps/04_texture.cpp
  src/steps/05_sampler.cpp
  src/steps/06_depth.cpp
  src/steps/07_model.cpp
  src/steps/08_msaa.cpp)

add_library(p02_lib STATIC
  src/ResourceApp.cpp
  ${P02_STEP_SOURCES})

target_include_directories(p02_lib PUBLIC src)
target_link_libraries(p02_lib PUBLIC rwb::engine PRIVATE rwb::warnings vma tinygltf)

rwb_add_shaders(p02_lib SHADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_basic.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_basic.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_ubo.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_ubo.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_push.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_push.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_tex.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_tex.frag)

# rwb_add_shaders 把 RWB_SHADER_DIR 定义成 PRIVATE，测试目标也要读同一批 .spv
set(P02_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders/p02_lib")
target_compile_definitions(p02_lib PUBLIC
  RWB_SHADER_DIR="${P02_SHADER_DIR}"
  RWB_P02_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_executable(p02_resources src/main.cpp)
target_link_libraries(p02_resources PRIVATE p02_lib rwb::warnings)

add_executable(p02_tests tests/test_p02.cpp)
target_link_libraries(p02_tests PRIVATE p02_lib rwb::testfw rwb::warnings)

foreach(NN 01 02 03 04 05 06 07 08)
  rwb_add_test(p02-t${NN} TARGET p02_tests LABELS "l1;l3;p02" ARGS "[t${NN}]")
endforeach()
