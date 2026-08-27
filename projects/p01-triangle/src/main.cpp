#include "TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"
#include "rwb/core/ValidationLog.h"

#include <cstdio>
#include <cstring>
#include <exception>

int main(int argc, char** argv) {
    p01::AppConfig config;
    int frames = -1;   // -1 = 一直画到关窗

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--no-validation") == 0) {
            config.enableValidation = false;
        }
    }

    try {
        p01::TriangleApp app(config);
        app.initUpTo(p01::Stage::FramesInFlight);
        app.run(frames);
    } catch (const rwb::NotImplemented& e) {
        // 骨架阶段的正常出口：告诉你下一步该写哪里
        std::fprintf(stderr, "\n%s\n\n", e.what());
        std::fprintf(stderr, "这是挖空题还没填完的信号，不是 bug。\n"
                             "去 projects/p01-triangle/docs/ 找对应 task 的任务书。\n");
        return 10;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\n运行失败: %s\n", e.what());
        const auto& log = rwb::ValidationLog::instance();
        if (log.errorCount() || log.warningCount()) {
            std::fprintf(stderr, "\n%s\n", log.summary().c_str());
        }
        return 1;
    }

    const auto& log = rwb::ValidationLog::instance();
    if (log.errorCount() || log.warningCount()) {
        std::fprintf(stderr, "\n%s\n", log.summary().c_str());
        return 2;
    }
    std::printf("正常退出，validation 零报错。\n");
    return 0;
}
