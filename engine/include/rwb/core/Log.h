#pragma once
#include <string>
#include <string_view>

namespace rwb {

enum class LogLevel { Trace, Info, Warn, Error };

void logMessage(LogLevel level, std::string_view msg);

inline void logTrace(std::string_view m) { logMessage(LogLevel::Trace, m); }
inline void logInfo (std::string_view m) { logMessage(LogLevel::Info,  m); }
inline void logWarn (std::string_view m) { logMessage(LogLevel::Warn,  m); }
inline void logError(std::string_view m) { logMessage(LogLevel::Error, m); }

// printf 风格，避免为了打个日志引入 fmt 依赖
std::string format(const char* fmt, ...);

} // namespace rwb
