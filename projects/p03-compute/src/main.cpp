#include "ComputeApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <cstdlib>
#include <exception>
#include <numeric>
#include <vector>

int main(int argc, char** argv) {
    int task = argc > 1 ? std::atoi(argv[1]) : 1;
    if (task < 1) task = 1;
    if (task > 7) task = 7;

    try {
        p03::ComputeApp app;
        if (task == 1) {
            const std::vector<float> x{1, 2, 3, 4};
            const std::vector<float> y{10, 20, 30, 40};
            const auto result = app.runSaxpy(2.0f, x, y);
            rwb::logInfo(rwb::format("saxpy: %.1f %.1f %.1f %.1f", result[0], result[1],
                                     result[2], result[3]));
        } else if (task == 2) {
            std::vector<float> values(4097);
            std::iota(values.begin(), values.end(), 1.0f);
            const auto path = app.chooseReducePath();
            rwb::logInfo(rwb::format("reduce(%s): %.1f", p03::toString(path),
                                     app.runReduce(values, path)));
        } else if (task == 3) {
            const std::vector<std::uint32_t> values{3, 1, 4, 1, 5};
            const auto result = app.runScan(values);
            rwb::logInfo(rwb::format("scan: %u %u %u %u %u", result[0], result[1], result[2],
                                     result[3], result[4]));
        } else if (task == 4) {
            const auto result = app.runBitonicSort({9, 1, 4, 1, 5, 2, 6});
            rwb::logInfo(rwb::format("sort: %u %u %u %u %u %u %u", result[0], result[1],
                                     result[2], result[3], result[4], result[5], result[6]));
        } else if (task == 5) {
            const std::vector<float> pixels{4, 1, 0, 1, 0, 2, 1, 1, 1, 0, 3, 1, 2, 2, 2, 1};
            const auto result = app.runPostprocess(pixels, 2, 2);
            rwb::logInfo(rwb::format("postprocess first pixel: %.3f %.3f %.3f", result[0],
                                     result[1], result[2]));
        } else if (task == 6) {
            const auto result = app.runParticles({0, 0, 0, 1, 1, 1, 0, 1}, 0.25f, -0.5f);
            rwb::logInfo(rwb::format("particles first: %.3f %.3f", result[0], result[1]));
        } else {
            const auto result = app.runIndirectScale({1, 2, 3, 4}, 2.5f);
            rwb::logInfo(rwb::format("indirect scale: %.1f %.1f %.1f %.1f", result[0],
                                     result[1], result[2], result[3]));
        }
    } catch (const rwb::NotImplemented& e) {
        rwb::logError(e.what());
        rwb::logError("这一步还没做。任务书在 projects/p03-compute/docs/ 下。");
        return 10;
    } catch (const std::exception& e) {
        rwb::logError(e.what());
        return 1;
    }
    return 0;
}
