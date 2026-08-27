#include "BufferAssert.h"

#include <cmath>
#include <limits>

namespace rwb::test {

BufferCompareResult compareApprox(const std::vector<float>& actual,
                                  const std::vector<float>& expected,
                                  FloatTolerance tol) {
    BufferCompareResult r;
    r.totalCount = expected.size();

    if (actual.size() != expected.size()) {
        r.message = format("元素个数不一致: 实际 %zu, 期望 %zu", actual.size(), expected.size());
        return r;
    }

    double worstAbs = 0.0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const double a = static_cast<double>(actual[i]);
        const double e = static_cast<double>(expected[i]);

        // NaN 永远算不一致，即使两边都是 NaN —— 出现 NaN 基本等于算错了
        const bool bothFinite = std::isfinite(a) && std::isfinite(e);
        const double diff = bothFinite ? std::fabs(a - e)
                                       : std::numeric_limits<double>::infinity();
        const double limit = tol.atol + tol.rtol * std::fabs(e);

        if (bothFinite) {
            worstAbs = std::max(worstAbs, diff);
        }
        if (!bothFinite || diff > limit) {
            ++r.mismatchCount;
            if (r.firstMismatches.size() < 8) {
                r.firstMismatches.push_back(ElementMismatch{i, a, e});
            }
        }
    }

    r.passed  = (r.mismatchCount == 0);
    r.message = format("%zu/%zu 个元素超差 (最大绝对误差 %g, 容差 atol=%g rtol=%g)",
                       r.mismatchCount, r.totalCount, worstAbs, tol.atol, tol.rtol);
    for (const ElementMismatch& m : r.firstMismatches) {
        r.message += format("\n  [%zu] 实际 %.9g != 期望 %.9g", m.index, m.actual, m.expected);
    }
    return r;
}

} // namespace rwb::test
