// P2 的可执行程序。开个窗口一直画，直到你关掉它。
//
// 想看某一阶段的样子就改 initUpTo 的参数 —— 比如只做到 t03 时传 Stage::PushConstants，
// 传更后面的阶段会因为函数还没实现而抛 NotImplemented。
#include "ResourceApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <cstdlib>
#include <exception>

int main(int argc, char** argv) {
    // 命令行给个阶段号，默认跑到最后一阶段：p02_resources 5
    int stage = 8;
    if (argc > 1) stage = std::atoi(argv[1]);
    if (stage < 1) stage = 1;
    if (stage > 8) stage = 8;

    try {
        p02::AppConfig config;
        config.title = "render-workbook P02 - stage " + std::to_string(stage);

        p02::ResourceApp app(config);
        app.initUpTo(static_cast<p02::Stage>(stage));
        rwb::logInfo(rwb::format("GPU: %s", app.deviceName().c_str()));
        app.run();
    } catch (const rwb::NotImplemented& e) {
        rwb::logError(e.what());
        rwb::logError("这一步还没做。任务书在 projects/p02-resources/docs/ 下。");
        return 10;
    } catch (const std::exception& e) {
        rwb::logError(e.what());
        return 1;
    }
    return 0;
}
