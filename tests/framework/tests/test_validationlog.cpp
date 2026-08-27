#include <catch2/catch_test_macros.hpp>

#include "rwb/core/ValidationLog.h"

// 判分逻辑本身的测试。
//
// 由来: CI 上 Vulkan loader 因为 VK_LOADER_DRIVERS_SELECT 过滤 ICD，
// 通过 debug messenger 发了 7 条 GENERAL 类型的 WARNING。早期实现不分类型地
// 统计，于是一份完全正确的代码在 CI 上被判 7 条 warning 失败。
//
// 判分系统自己出错，比被判分的代码出错更难发现 —— 所以它需要测试。

using rwb::ValidationLog;

namespace {

void record(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
            VkDebugUtilsMessageTypeFlagsEXT        type,
            const char*                            msg) {
    ValidationLog::instance().record(sev, type, msg);
}

constexpr auto kError = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
constexpr auto kWarn  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
constexpr auto kInfo  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

constexpr auto kValidation  = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
constexpr auto kPerformance = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
constexpr auto kGeneral     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;

} // namespace

TEST_CASE("loader 的 GENERAL 消息不参与判分", "[grading]") {
    ValidationLog::instance().reset();

    // 复刻 CI 上真实发生的那 7 条
    for (int i = 0; i < 7; ++i) {
        record(kWarn, kGeneral, "Driver \"radeon_icd.json\" ignored because not selected by env var");
    }

    INFO(ValidationLog::instance().summary());
    CHECK(ValidationLog::instance().errorCount()   == 0);
    CHECK(ValidationLog::instance().warningCount() == 0);
    CHECK(ValidationLog::instance().generalMessageCount() == 7);
}

TEST_CASE("validation 类的 error 与 warning 才判失败", "[grading]") {
    ValidationLog::instance().reset();

    record(kError, kValidation, "VUID-vkQueueSubmit-pCommandBuffers-00071");
    record(kWarn,  kValidation, "SYNC-HAZARD-WRITE-AFTER-READ");
    record(kWarn,  kValidation, "另一条 validation 警告");

    CHECK(ValidationLog::instance().errorCount()   == 1);
    CHECK(ValidationLog::instance().warningCount() == 2);
}

TEST_CASE("性能建议单独计数，不判失败", "[grading]") {
    // 不同驱动给的性能建议差异很大。拿它判分会让判分结果取决于跑在谁家 GPU 上，
    // 而这门课要求同一份代码在 NVIDIA / Apple Silicon / lavapipe 三处判分一致。
    ValidationLog::instance().reset();

    record(kWarn, kPerformance, "小块显存分配, 建议 suballocate");

    CHECK(ValidationLog::instance().errorCount()              == 0);
    CHECK(ValidationLog::instance().warningCount()            == 0);
    CHECK(ValidationLog::instance().performanceWarningCount() == 1);
}

TEST_CASE("INFO 级别不计入任何计数", "[grading]") {
    ValidationLog::instance().reset();
    record(kInfo, kValidation, "只是提示");

    CHECK(ValidationLog::instance().errorCount()   == 0);
    CHECK(ValidationLog::instance().warningCount() == 0);
}

TEST_CASE("reset 清空全部计数", "[grading]") {
    ValidationLog::instance().reset();
    record(kError, kValidation,  "err");
    record(kWarn,  kPerformance, "perf");
    record(kWarn,  kGeneral,     "loader");

    ValidationLog::instance().reset();
    CHECK(ValidationLog::instance().errorCount()              == 0);
    CHECK(ValidationLog::instance().warningCount()            == 0);
    CHECK(ValidationLog::instance().performanceWarningCount() == 0);
    CHECK(ValidationLog::instance().generalMessageCount()     == 0);
    CHECK(ValidationLog::instance().entries().empty());
}

TEST_CASE("summary 在只有 loader 消息时说清楚不判分", "[grading]") {
    ValidationLog::instance().reset();
    record(kWarn, kGeneral, "loader 絮叨");

    const std::string s = ValidationLog::instance().summary();
    INFO(s);
    CHECK(s.find("0 error") != std::string::npos);
    CHECK(s.find("不判分")  != std::string::npos);
}
