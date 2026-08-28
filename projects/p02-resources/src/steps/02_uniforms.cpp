// t02 —— UBO / descriptor set layout / pool / set
//
// 任务书: projects/p02-resources/docs/t02-uniforms.md
// 判分:   ctest --preset win-msvc -R p02-t02
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"
#include "rwb/rhi/VmaUsage.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace p02 {

void ResourceApp::createDescriptorSetLayout() {
    // TODO(p02-t02):
    //   binding 0: UNIFORM_BUFFER, count 1, 只给 VERTEX 阶段看（相机矩阵在那里用）
    //
    //   还要处理一个跨阶段的细节：t05 之后多一个 binding 1
    //   （COMBINED_IMAGE_SAMPLER, 给 FRAGMENT 阶段）。
    //   用 stageIndex(m_reached) >= stageIndex(Stage::Sampler) 判断。
    //   布局必须和当前 pipeline 用的那对 shader 严格一致 ——
    //   framework 会按阶段挑 scene_ubo / scene_push / scene_tex。
    RWB_TODO("p02-t02 ResourceApp::createDescriptorSetLayout");
}

void ResourceApp::createUniformBuffers() {
    // TODO(p02-t02):
    //   建 kFramesInFlight 个 UBO，每个装得下一个 CameraUniform。
    //   usage = UNIFORM_BUFFER，hostVisible + 常驻映射（每帧都要写）。
    //
    //   想清楚：为什么不能只建一份？
    RWB_TODO("p02-t02 ResourceApp::createUniformBuffers");
}

void ResourceApp::createDescriptorPool() {
    // TODO(p02-t02):
    //   声明「每种类型要发多少个」+ maxSets。
    //   注意这是两笔独立的账：poolSizes 数的是 descriptor 个数，
    //   maxSets 数的是 set 个数。发超了返回 VK_ERROR_OUT_OF_POOL_MEMORY，不会自动扩容。
    //   binding 1 存在时（t05 之后）别忘了给 COMBINED_IMAGE_SAMPLER 也留额度。
    RWB_TODO("p02-t02 ResourceApp::createDescriptorPool");
}

void ResourceApp::createDescriptorSets() {
    // TODO(p02-t02):
    //   1. vkAllocateDescriptorSets 分出 kFramesInFlight 个 set
    //      （pSetLayouts 要传 kFramesInFlight 份「同一个」layout）
    //   2. 对每个 set 调 vkUpdateDescriptorSets，把第 i 个 UBO 写进 binding 0
    //   3. m_texture 和 m_sampler 都就绪时（t05 之后），再写一条 binding 1，
    //      imageLayout 用 SHADER_READ_ONLY_OPTIMAL
    //
    //   坑：VkDescriptorBufferInfo / VkDescriptorImageInfo 是栈上的，
    //   vkUpdateDescriptorSets 必须在它们还活着的时候调用。
    RWB_TODO("p02-t02 ResourceApp::createDescriptorSets");
}

void ResourceApp::updateUniformBuffer(std::uint32_t frameIndex) {
    // TODO(p02-t02):
    //   算好 view / proj 写进第 frameIndex 个 UBO 的映射地址。
    //   相机: 眼睛 (0, 1.2, 3.2)，看向原点，上方向 +Y
    //   投影: glm::perspective(radians(45), 宽高比, 0.1, 30.0)
    //
    //   必须做一件 glm 不会替你做的事 —— 看顶层 CMakeLists.txt 里
    //   GLM_FORCE_DEPTH_ZERO_TO_ONE 那段注释的最后一句。
    RWB_TODO("p02-t02 ResourceApp::updateUniformBuffer");
}

} // namespace p02
