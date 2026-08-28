// t03 —— push constant
//
// 任务书: projects/p02-resources/docs/t03-pushconstants.md
// 判分:   ctest --preset win-msvc -R p02-t03
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"

namespace p02 {

std::vector<VkPushConstantRange> ResourceApp::pushConstantRanges() const {
    // TODO(p02-t03):
    //   声明一段 push constant: offset 0、大小 sizeof(ObjectPush)、
    //   stageFlags 至少包含 VERTEX（model 矩阵在顶点着色器里用）。
    //
    //   规范只保证 maxPushConstantsSize >= 128 字节，而且那是所有阶段加起来的总量。
    //   测试会检查你声明的总量没超过 128。
    RWB_TODO("p02-t03 ResourceApp::pushConstantRanges");
}

void ResourceApp::pushObjectData(VkCommandBuffer cmd, const ObjectPush& data) const {
    // TODO(p02-t03):
    //   vkCmdPushConstants 把 data 塞进命令流。
    //   stageFlags 必须是上面 range 里声明过的阶段的子集，offset/size 也要对得上。
    //
    //   想清楚：为什么这里一个同步原语都不需要，而 UBO 却要每个在飞帧一份？
    RWB_TODO("p02-t03 ResourceApp::pushObjectData");
}

} // namespace p02
