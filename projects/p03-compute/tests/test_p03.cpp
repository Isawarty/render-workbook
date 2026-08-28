#include <catch2/catch_test_macros.hpp>

#include "BufferAssert.h"
#include "ComputeApp.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

namespace gt = rwb::test;
using namespace p03;

namespace {

struct ValidationGuard {
    ValidationGuard() { rwb::ValidationLog::instance().reset(); }
    void requireClean() const {
        const auto& log = rwb::ValidationLog::instance();
        INFO(log.summary());
        REQUIRE(log.errorCount() == 0);
        REQUIRE(log.warningCount() == 0);
    }
};

std::vector<std::uint32_t> cpuExclusiveScan(const std::vector<std::uint32_t>& input) {
    std::vector<std::uint32_t> out(input.size());
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        out[i] = sum;
        sum += input[i];
    }
    return out;
}

std::vector<float> cpuPostprocess(const std::vector<float>& in,
                                  std::uint32_t width, std::uint32_t height) {
    std::vector<float> out(in.size(), 1.0f);
    const int weights[3] = {1, 2, 1};
    auto clampCoord = [](int v, int max) { return std::max(0, std::min(v, max - 1)); };
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (int c = 0; c < 3; ++c) {
                float sum = 0.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const auto sx = static_cast<std::uint32_t>(
                            clampCoord(static_cast<int>(x) + dx, static_cast<int>(width)));
                        const auto sy = static_cast<std::uint32_t>(
                            clampCoord(static_cast<int>(y) + dy, static_cast<int>(height)));
                        sum += in[(static_cast<std::size_t>(sy) * width + sx) * 4 + c] *
                               static_cast<float>(weights[dx + 1] * weights[dy + 1]);
                    }
                }
                const float blurred = sum / 16.0f;
                out[(static_cast<std::size_t>(y) * width + x) * 4 + c] =
                    blurred / (1.0f + blurred);
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("t01 headless compute 能启动，saxpy 逐元素正确", "[t01]") {
    ValidationGuard guard;
    auto app = std::make_unique<ComputeApp>();
    REQUIRE(app->ctx().config().headless);
    REQUIRE(app->ctx().window() == nullptr);
    REQUIRE(app->ctx().computeQueue() != VK_NULL_HANDLE);

    // 65537 = 256 * 256 + 1：强制最后一个 workgroup 只有一个有效线程。
    constexpr std::size_t n = 65537;
    std::vector<float> x(n), y(n), expected(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(static_cast<int>(i % 101) - 50) * 0.25f;
        y[i] = static_cast<float>(i % 37) * 0.5f;
        expected[i] = 1.75f * x[i] + y[i];
    }
    const auto actual = app->runSaxpy(1.75f, x, y);
    const auto result = gt::compareApprox(actual, expected);
    INFO("设备: " << app->deviceName());
    INFO(result.message);
    REQUIRE(result.passed);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t02 subgroup 能力选择是设备无关的纯逻辑", "[t02]") {
    REQUIRE(ComputeApp::choosePath(VK_SUBGROUP_FEATURE_ARITHMETIC_BIT,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 32) ==
            ReducePath::Subgroup);
    REQUIRE(ComputeApp::choosePath(0, VK_SHADER_STAGE_COMPUTE_BIT, 32) ==
            ReducePath::Shared);
    REQUIRE(ComputeApp::choosePath(VK_SUBGROUP_FEATURE_ARITHMETIC_BIT,
                                   VK_SHADER_STAGE_FRAGMENT_BIT, 32) == ReducePath::Shared);
    REQUIRE(ComputeApp::choosePath(VK_SUBGROUP_FEATURE_ARITHMETIC_BIT,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 2) == ReducePath::Shared);
}

TEST_CASE("t02 多轮归约与 CPU 求和一致，能力不足时有明确降级", "[t02]") {
    ValidationGuard guard;
    auto app = std::make_unique<ComputeApp>();

    constexpr std::size_t n = 65537;
    std::vector<float> values(n);
    for (std::size_t i = 0; i < n; ++i) {
        values[i] = static_cast<float>(static_cast<int>(i % 17) - 8) * 0.125f;
    }
    const float expected = std::accumulate(values.begin(), values.end(), 0.0f);
    const auto tol = gt::FloatTolerance::forReduction(n);

    const float shared = app->runReduce(values, ReducePath::Shared);
    const auto sharedResult = gt::compareApprox({shared}, {expected}, tol);
    INFO("shared: " << sharedResult.message);
    REQUIRE(sharedResult.passed);

    const ReducePath selected = app->chooseReducePath();
    const auto& sub = app->ctx().subgroupProperties();
    INFO("设备: " << app->deviceName() << " subgroupSize=" << sub.subgroupSize
                  << " path=" << toString(selected));
    REQUIRE(selected == ComputeApp::choosePath(sub.supportedOperations, sub.supportedStages,
                                                sub.subgroupSize));
    if (selected == ReducePath::Subgroup) {
        const float subgroup = app->runReduce(values, ReducePath::Subgroup);
        const auto subgroupResult = gt::compareApprox({subgroup}, {expected}, tol);
        INFO("subgroup: " << subgroupResult.message);
        REQUIRE(subgroupResult.passed);
    } else {
        // 不是 SKIP：仍然验证真实设备的能力查询确实不满足 subgroup 路径契约。
        const bool capable =
            (sub.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0 &&
            (sub.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0 && sub.subgroupSize >= 4;
        REQUIRE_FALSE(capable);
    }

    app.reset();
    guard.requireClean();
}

TEST_CASE("t03 exclusive scan 覆盖空输入、跨块边界与最大两级规模", "[t03]") {
    ValidationGuard guard;
    auto app = std::make_unique<ComputeApp>();
    REQUIRE(app->runScan({}).empty());

    for (const std::size_t n : {std::size_t{1}, std::size_t{257},
                                static_cast<std::size_t>(kScanMaxElements)}) {
        std::vector<std::uint32_t> input(n);
        for (std::size_t i = 0; i < n; ++i) input[i] = static_cast<std::uint32_t>(i % 7);
        const auto expected = cpuExclusiveScan(input);
        const auto actual   = app->runScan(input);
        const auto result   = gt::compareExact(actual, expected);
        INFO("n=" << n << " | " << result.message);
        REQUIRE(result.passed);
    }

    app.reset();
    guard.requireClean();
}

TEST_CASE("t04 bitonic sort 覆盖非二次幂、重复值与 padding 哨兵", "[t04]") {
    ValidationGuard guard;
    auto app = std::make_unique<ComputeApp>();
    REQUIRE(app->runBitonicSort({}).empty());

    for (const std::size_t n : {std::size_t{1}, std::size_t{257}, std::size_t{1023}}) {
        std::vector<std::uint32_t> input(n);
        for (std::size_t i = 0; i < n; ++i) {
            input[i] = static_cast<std::uint32_t>((n - i) % 73);
        }
        auto expected = input;
        std::sort(expected.begin(), expected.end());
        const auto actual = app->runBitonicSort(input);
        const auto result = gt::compareExact(actual, expected);
        INFO("n=" << n << " | " << result.message);
        REQUIRE(result.passed);
    }

    app.reset();
    guard.requireClean();
}

TEST_CASE("t05 Gaussian+tonemap 由 graphics pass 消费，barrier 契约正确", "[t05]") {
    ValidationGuard guard;
    auto app = std::make_unique<ComputeApp>();
    REQUIRE(app->runPostprocess({}, 0, 0).empty());

    constexpr std::uint32_t width = 17, height = 11; // 两个方向都不是 local size 8 的倍数
    std::vector<float> input(static_cast<std::size_t>(width) * height * 4, 1.0f);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 4;
            input[i + 0] = static_cast<float>((x * 3 + y) % 9) * 0.5f;
            input[i + 1] = static_cast<float>((x + y * 5) % 7) * 0.75f;
            input[i + 2] = static_cast<float>((x * 2 + y * 3) % 11) * 0.25f;
        }
    }
    const auto expected = cpuPostprocess(input, width, height);
    const auto actual = app->runPostprocess(input, width, height);
    const auto result = gt::compareApprox(actual, expected, {2e-5, 2e-6});
    INFO(result.message);
    REQUIRE(result.passed);

    const auto& b = app->lastComputeToGraphicsBarrier();
    REQUIRE(b.srcStage == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    REQUIRE(b.dstStage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    REQUIRE(b.srcAccess == VK_ACCESS_SHADER_WRITE_BIT);
    REQUIRE(b.dstAccess == VK_ACCESS_SHADER_READ_BIT);

    app.reset();
    guard.requireClean();
}
