// t05 —— Gaussian blur / tonemap，compute → graphics barrier
// 任务书: projects/p03-compute/docs/t05-postprocess.md
// 判分:   python rwb.py test p03-t05
#include "../ComputeApp.h"
#include "rwb/core/Todo.h"
namespace p03 {
std::vector<float> ComputeApp::runPostprocess(const std::vector<float>& rgba,
                                              std::uint32_t width,
                                              std::uint32_t height) {
    // TODO(p03-t05): compute filter → barrier → fullscreen graphics consumer → readback。
    RWB_TODO("p03-t05 ComputeApp::runPostprocess");
}
} // namespace p03
