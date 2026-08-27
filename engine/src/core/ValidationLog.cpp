#include "rwb/core/ValidationLog.h"

#include "rwb/core/Log.h"

namespace rwb {

ValidationLog& ValidationLog::instance() {
    static ValidationLog s_instance;
    return s_instance;
}

void ValidationLog::record(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                           VkDebugUtilsMessageTypeFlagsEXT        types,
                           std::string                            message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Entry entry{severity, types, std::move(message)};

    if (entry.fromValidation()) {
        // 只有 validation layer 的判断才影响判分
        if (entry.isError())   ++m_errors;
        if (entry.isWarning()) ++m_warnings;
    } else if (entry.fromPerformance()) {
        if (entry.isWarning() || entry.isError()) ++m_performanceWarnings;
    } else {
        ++m_generalMessages;
    }

    m_entries.push_back(std::move(entry));
}

void ValidationLog::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_errors              = 0;
    m_warnings            = 0;
    m_performanceWarnings = 0;
    m_generalMessages     = 0;
}

std::size_t ValidationLog::errorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errors;
}

std::size_t ValidationLog::warningCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_warnings;
}

std::size_t ValidationLog::performanceWarningCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_performanceWarnings;
}

std::size_t ValidationLog::generalMessageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generalMessages;
}

std::vector<ValidationLog::Entry> ValidationLog::entries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

std::string ValidationLog::summary(std::size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string out = format(
        "validation: %zu error / %zu warning  (另有 %zu 条性能建议、%zu 条 loader/general 消息, 均不判分)\n",
        m_errors, m_warnings, m_performanceWarnings, m_generalMessages);

    std::size_t shown = 0;
    for (const Entry& e : m_entries) {
        if (!e.fromValidation()) continue;          // 只列会判分的那些
        if (!e.isError() && !e.isWarning()) continue;
        if (shown++ >= maxEntries) {
            out += "  ... (更多消息已省略)\n";
            break;
        }
        out += format("  [%s] %s\n", e.isError() ? "ERROR" : "WARN ", e.message.c_str());
    }

    if (shown == 0 && (m_performanceWarnings || m_generalMessages)) {
        out += "  (没有 validation 类消息; 上面那些计数来自不判分的类别)\n";
    }
    return out;
}

} // namespace rwb
