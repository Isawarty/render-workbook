#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rwb/core/Log.h"

namespace rwb::test {

// L2 判分：把 GPU 算出来的 buffer 回读后，与 CPU 参考实现逐元素比对。
//
// 这是全套测试里唯一 100% 确定性的一层 —— 整数完全相等，
// 浮点也只受 GPU 浮点规则影响，不受光栅化/驱动差异影响。
// P3（Compute Shader 专项）的每一题都靠它判分。

struct FloatTolerance {
    // 相对误差 + 绝对误差的组合，跟 numpy.isclose 一个路子：
    //   |a - b| <= atol + rtol * |b|
    double rtol = 1e-5;
    double atol = 1e-6;

    // GPU 上做大规模归约时误差会累积，量级和 sqrt(N) 相关。
    // 与其一刀切放宽阈值，不如按元素数量给出合理界限。
    static FloatTolerance forReduction(std::size_t elementCount) {
        const double scale = std::sqrt(static_cast<double>(elementCount));
        return FloatTolerance{1e-5 * scale, 1e-6 * scale};
    }
};

struct ElementMismatch {
    std::size_t index    = 0;
    double      actual   = 0.0;
    double      expected = 0.0;
};

struct BufferCompareResult {
    bool                         passed = false;
    std::size_t                  mismatchCount = 0;
    std::size_t                  totalCount    = 0;
    std::vector<ElementMismatch> firstMismatches; // 最多记前 8 个
    std::string                  message;
};

template <typename T>
BufferCompareResult compareExact(const std::vector<T>& actual, const std::vector<T>& expected) {
    BufferCompareResult r;
    r.totalCount = expected.size();

    if (actual.size() != expected.size()) {
        r.message = format("元素个数不一致: 实际 %zu, 期望 %zu", actual.size(), expected.size());
        return r;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            ++r.mismatchCount;
            if (r.firstMismatches.size() < 8) {
                r.firstMismatches.push_back(
                    ElementMismatch{i, static_cast<double>(actual[i]), static_cast<double>(expected[i])});
            }
        }
    }
    r.passed  = (r.mismatchCount == 0);
    r.message = format("%zu/%zu 个元素不一致", r.mismatchCount, r.totalCount);
    for (const ElementMismatch& m : r.firstMismatches) {
        r.message += format("\n  [%zu] 实际 %g != 期望 %g", m.index, m.actual, m.expected);
    }
    return r;
}

BufferCompareResult compareApprox(const std::vector<float>& actual,
                                  const std::vector<float>& expected,
                                  FloatTolerance tol = FloatTolerance{});

} // namespace rwb::test
