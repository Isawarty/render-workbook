#include "RenderGraphApp.h"

#include "rwb/core/Log.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    int frames = -1;
    std::string dotPath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (arg == "--dot" && i + 1 < argc) dotPath = argv[++i];
    }
    try {
        p05::RenderGraphApp app;
        app.init();
        if (!dotPath.empty()) {
            std::ofstream output(dotPath);
            output << app.graphviz();
            if (!output) throw std::runtime_error("无法写入 DOT 文件: " + dotPath);
        }
        app.run(frames);
    } catch (const std::exception& error) {
        rwb::logError(error.what());
        return 1;
    }
    return 0;
}
