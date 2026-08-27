# 三层测试的公共基建。
#
# L1 (validation) 的收集点在 engine 里（rwb::ValidationLog），因为被测代码本身要用；
# L2 (readback)  = BufferAssert
# L3 (golden)    = ImageCompare
rwb_require(stb)

add_library(rwb_testfw STATIC
  ImageCompare.cpp
  BufferAssert.cpp
  NoDialogs.cpp)

target_include_directories(rwb_testfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(rwb_testfw PRIVATE rwb::warnings)
target_link_libraries(rwb_testfw PUBLIC
  rwb::engine
  Catch2::Catch2WithMain
  PRIVATE stb)

add_library(rwb::testfw ALIAS rwb_testfw)

# 判分逻辑自己的单元测试。
# 判分系统出错比被判分的代码出错更难发现 —— CI 上就真出过一次
# (loader 的 GENERAL 消息被当成 validation warning, 正确代码被判失败)。
add_executable(framework_tests tests/test_validationlog.cpp)
target_link_libraries(framework_tests PRIVATE rwb::testfw rwb::warnings)

rwb_add_test(framework-grading TARGET framework_tests LABELS "l0;framework" ARGS "[grading]")
