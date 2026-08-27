#include "rwb/core/Log.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace rwb {

void logMessage(LogLevel level, std::string_view msg) {
    const char* tag = "";
    switch (level) {
        case LogLevel::Trace: tag = "[trace]"; break;
        case LogLevel::Info:  tag = "[info ]"; break;
        case LogLevel::Warn:  tag = "[warn ]"; break;
        case LogLevel::Error: tag = "[error]"; break;
    }
    std::FILE* out = (level == LogLevel::Error || level == LogLevel::Warn) ? stderr : stdout;
    std::fprintf(out, "%s %.*s\n", tag, static_cast<int>(msg.size()), msg.data());
    std::fflush(out);
}

std::string format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        va_end(args);
        return {};
    }
    std::vector<char> buf(static_cast<std::size_t>(n) + 1);
    std::vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);
    return std::string(buf.data(), static_cast<std::size_t>(n));
}

} // namespace rwb
