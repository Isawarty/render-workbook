# ---------------------------------------------------------------------------
# 测试注册
#
# 三层测试（详见 docs/00-roadmap.md）：
#   L1 validation  — validation layer 零 error/warning
#   L2 readback    — GPU 结果回读，与 CPU 参考实现逐元素比对（确定性最强）
#   L3 golden      — headless 渲染结果与基准图对比
#
# ctest label 约定：每个测试都带 l1/l2/l3 之一 + 项目标签（p00/p01/...）
# 于是可以：ctest -L l2        只跑确定性最强的一层
#           ctest -L p01       只跑项目 1
#           ctest -R p01-t03   只跑某一题
# ---------------------------------------------------------------------------
include(CTest)

# rwb_add_test(<name> TARGET <exe> LABELS <l1|l2|l3> <p0X> ... [ARGS ...])
function(rwb_add_test NAME)
  cmake_parse_arguments(ARG "" "TARGET" "LABELS;ARGS" ${ARGN})

  add_test(NAME ${NAME}
           COMMAND $<TARGET_FILE:${ARG_TARGET}> ${ARG_ARGS})

  set_tests_properties(${NAME} PROPERTIES
    LABELS "${ARG_LABELS}"
    ENVIRONMENT "RWB_GOLDEN_DIR=${CMAKE_SOURCE_DIR}/tests/golden;RWB_OUTPUT_DIR=${CMAKE_BINARY_DIR}/tests/output"
    TIMEOUT 120)
endfunction()
