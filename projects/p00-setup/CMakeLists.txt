# P0 —— 环境自检。
# 这个项目没有挖空题，它的唯一职责是让三台机器都确认工具链是通的。

add_executable(p00_setup src/main.cpp src/EnvProbe.cpp)
target_link_libraries(p00_setup PRIVATE rwb::engine rwb::warnings)
target_include_directories(p00_setup PRIVATE src)
rwb_add_shaders(p00_setup SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/probe.comp)

# 环境自检的测试版本（同样的探测逻辑，但用 Catch2 断言）
add_executable(p00_tests tests/test_env.cpp src/EnvProbe.cpp)
target_link_libraries(p00_tests PRIVATE rwb::testfw rwb::warnings)
target_include_directories(p00_tests PRIVATE src)
rwb_add_shaders(p00_tests SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/probe.comp)

rwb_add_test(p00-env-vulkan  TARGET p00_tests LABELS "l1;p00" ARGS "[vulkan]")
rwb_add_test(p00-env-shaders TARGET p00_tests LABELS "l1;p00" ARGS "[shaders]")
