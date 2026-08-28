// t06 —— 深度缓冲 / 深度测试
//
// 任务书: projects/p02-resources/docs/t06-depth.md
// 判分:   ctest --preset win-msvc -R p02-t06
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"

namespace p02 {

VkFormat ResourceApp::findDepthFormat() const {
    // TODO(p02-t06):
    //   用 m_ctx->findSupportedFormat 从候选里挑第一个可用的，
    //   按精度从高到低排: D32_SFLOAT / D32_SFLOAT_S8_UINT / D24_UNORM_S8_UINT。
    //   tiling = OPTIMAL, features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT。
    //
    //   写死 D32_SFLOAT 在桌面卡上一直没事，直到某天在 MoltenVK 或安卓上
    //   拿到 VK_ERROR_FORMAT_NOT_SUPPORTED —— 而那时你已经忘了这里。
    RWB_TODO("p02-t06 ResourceApp::findDepthFormat");
}

void ResourceApp::createDepthResources() {
    // TODO(p02-t06):
    //   1. findDepthFormat()
    //   2. createImage: 尺寸和 swapchain 一致，mipLevels = 1,
    //      samples 必须用 m_sampleCount（t08 之前恒为 1，之后要跟颜色附件一致，
    //      否则 framebuffer 建不出来），usage = DEPTH_STENCIL_ATTACHMENT
    //   3. createImageView: aspect 是 VK_IMAGE_ASPECT_DEPTH_BIT，不是 COLOR
    //   4. immediateSubmit 里 transition(UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    //      （render pass 其实会替你转，这里显式做一次是为了养成习惯 ——
    //        到 P4 的 G-Buffer 就没有 render pass 兜底了）
    //
    //   框架已经替你处理好的部分：render pass 里的深度附件、pipeline 的
    //   depthTestEnable / depthWriteEnable / compareOp = LESS、clear 值 1.0、
    //   以及窗口 resize 时重建。可以去 ResourceApp.cpp 里读一遍它们怎么写的。
    RWB_TODO("p02-t06 ResourceApp::createDepthResources");
}

} // namespace p02
