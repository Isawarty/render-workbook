// t07 —— indirect dispatch
// 任务书: projects/p03-compute/docs/t07-indirect.md
// 判分:   python rwb.py test p03-t07
#include "../ComputeApp.h"
#include "rwb/core/Todo.h"
namespace p03 {
std::vector<float> ComputeApp::runIndirectScale(const std::vector<float>& data, float factor) {
    // TODO(p03-t07): GPU 写 command；compute-write 到 indirect-read barrier；
    // vkCmdDispatchIndirect 执行并回读 command 与结果。
    RWB_TODO("p03-t07 ComputeApp::runIndirectScale");
}
} // namespace p03
