// t02 —— 并行归约 / subgroup 降级路径
//
// 任务书: projects/p03-compute/docs/t02-reduce.md
// 判分:   python rwb.py test p03-t02

#include "../ComputeApp.h"

#include "rwb/core/Todo.h"

namespace p03 {

ReducePath ComputeApp::choosePath(VkSubgroupFeatureFlags supportedOperations,
                                  VkShaderStageFlags supportedStages,
                                  std::uint32_t subgroupSize) {
    // TODO(p03-t02): subgroup arithmetic 必须同时支持 compute stage，
    // 且 shader 的 subgroupSums[64] 必须装得下所有 subgroup。
    RWB_TODO("p03-t02 ComputeApp::choosePath");
}

ReducePath ComputeApp::chooseReducePath() const {
    // TODO(p03-t02): 用 Context::subgroupProperties() 调上面的纯函数。
    RWB_TODO("p03-t02 ComputeApp::chooseReducePath");
}

float ComputeApp::runReduce(const std::vector<float>& data, ReducePath path) {
    // TODO(p03-t02): 多轮 dispatch，每轮 ceil(count/256) 个输出；
    // 中间是 shader-write -> shader-read，末轮是 shader-write -> transfer-read。
    RWB_TODO("p03-t02 ComputeApp::runReduce");
}

} // namespace p03
