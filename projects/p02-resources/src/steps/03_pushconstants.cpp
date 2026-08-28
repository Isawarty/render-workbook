// t03 —— push constant
//
// 和 t02 的 UBO 做对比：同样是给 shader 送数据，push constant 走的是完全不同的路径。
// 它不占显存、不需要 descriptor、不需要同步，代价是极小的容量上限。
#include "../ResourceApp.h"

namespace p02 {

// push constant 的「范围」声明：从第几字节起、多少字节、哪些阶段能读。
//
// 规范只保证 maxPushConstantsSize >= 128 字节。这是全 Vulkan 里最吝啬的下限之一，
// 而且它是「所有阶段加起来」的总量，不是每个阶段各 128。
// 一个 mat4 就 64 字节，所以现实中 push constant 只能放：
// 一个矩阵、几个索引、一两个标量参数。再多就得回 UBO。
std::vector<VkPushConstantRange> ResourceApp::pushConstantRanges() const {
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;   // 只有顶点着色器读 model 矩阵
    range.offset     = 0;
    range.size       = sizeof(ObjectPush);           // 64 字节，稳在下限之内
    return {range};
}

// 录制期直接把数据塞进命令缓冲。
//
// 注意这里没有任何同步：数据是「命令流的一部分」，随命令缓冲一起走，
// 天然不存在「GPU 还在读上一帧的值」这种问题 —— 而 UBO 必须为此每帧准备一份。
// 这就是 t02 和 t03 的分水岭。
void ResourceApp::pushObjectData(VkCommandBuffer cmd, const ObjectPush& data) const {
    vkCmdPushConstants(cmd, m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,   // 必须是 range 里声明过的阶段的子集
                       0, sizeof(ObjectPush), &data);
}

} // namespace p02
