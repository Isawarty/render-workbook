#pragma once

#include <volk.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace rwb {

// Vulkan debug messenger 消息的收集点，也是 L1 判分的依据。
//
// 你在 P1-t01 写的回调只需要把参数「原样」转交给 record()，
// 「什么算判分失败」这个策略由这里决定 —— 原因见下。
//
// ## 为什么必须按 messageTypes 分类
//
// debug messenger 收到的不只是 validation layer 的消息，Vulkan loader
// 自己也会通过同一条通道发消息。比如 CI 上用 VK_LOADER_DRIVERS_SELECT
// 只保留 lavapipe 时，loader 会为每个被过滤掉的 ICD 发一条 WARNING：
//
//     Driver "radeon_icd.json" ignored because not selected by env var ...
//
// 这类消息是 GENERAL 类型，和你的代码对错毫无关系。
// 早期版本不分类型地统计，导致同一份正确代码在 CI 上直接 7 条 warning 判失败。
//
// 所以：
//   VALIDATION  类型 -> 计入 errorCount / warningCount，判分失败
//   PERFORMANCE 类型 -> 单独计数，「不」判失败（不同驱动给的性能建议差异很大，
//                       拿它判分会让判分结果依赖于跑在谁家的 GPU 上）
//   GENERAL     类型 -> 只记录不计数（loader 的絮叨、你自己 vkSetDebugUtilsObjectName 的回声等）
class ValidationLog {
public:
    struct Entry {
        VkDebugUtilsMessageSeverityFlagBitsEXT severity{};
        VkDebugUtilsMessageTypeFlagsEXT        types{};
        std::string                            message;

        bool isError()   const { return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   != 0; }
        bool isWarning() const { return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0; }
        bool fromValidation()  const { return (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)  != 0; }
        bool fromPerformance() const { return (types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0; }
    };

    static ValidationLog& instance();

    // 回调里原样转交即可，不需要自己做任何过滤。
    void record(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT        types,
                std::string                            message);

    void reset();

    // 以下两个只统计 VALIDATION 类型 —— 它们是 L1 判分的依据
    std::size_t errorCount() const;
    std::size_t warningCount() const;

    // 单独暴露，供你自己关注；不参与判分
    std::size_t performanceWarningCount() const;
    std::size_t generalMessageCount() const;

    // 测试失败时打出来定位用
    std::string summary(std::size_t maxEntries = 20) const;

    std::vector<Entry> entries() const;

private:
    ValidationLog() = default;

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
    std::size_t        m_errors             = 0;
    std::size_t        m_warnings           = 0;
    std::size_t        m_performanceWarnings = 0;
    std::size_t        m_generalMessages     = 0;
};

} // namespace rwb
