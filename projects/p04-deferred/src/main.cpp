#include "DeferredApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <cstdlib>
#include <exception>
#include <string>

int main(int argc, char** argv) {
    int frames = -1;
    int stage = 7;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--frames") frames = std::atoi(argv[i + 1]);
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (!arg.empty() && arg[0] != '-') stage = std::atoi(arg.c_str());
    }
    if (stage < 1) stage = 1;
    if (stage > 7) stage = 7;

    try {
        p04::DeferredApp app;
        app.initUpTo(static_cast<p04::Stage>(stage));
        app.run(frames);
    } catch (const rwb::NotImplemented& e) {
        rwb::logError(e.what());
        rwb::logError("这一步还没做。任务书在 projects/p04-deferred/docs/ 下。");
        return 10;
    } catch (const std::exception& e) {
        rwb::logError(e.what());
        return 1;
    }
    return 0;
}
