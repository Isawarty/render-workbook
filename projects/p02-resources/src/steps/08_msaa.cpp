// t08 —— MSAA（多重采样抗锯齿）
//
// 任务书: projects/p02-resources/docs/t08-msaa.md
// 判分:   ctest --preset win-msvc -R p02-t08
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"

namespace p02 {

VkSampleCountFlagBits ResourceApp::maxUsableSampleCount() const {
    // TODO(p02-t08):
    //   从 m_ctx->properties().limits 里取
    //     framebufferColorSampleCounts & framebufferDepthSampleCounts
    //   的交集，从高到低返回第一个可用的，本课封顶 4x（8x 以上收益递减而显存线性涨）。
    //   一个都没有就返回 VK_SAMPLE_COUNT_1_BIT（软件渲染器上很可能就是这样）。
    //
    //   只查颜色附件的采样能力是经典错误：某些硬件颜色支持 8x 而深度只到 4x，
    //   要到建 framebuffer 时才失败，而错误信息不会告诉你是深度的锅。
    RWB_TODO("p02-t08 ResourceApp::maxUsableSampleCount");
}

void ResourceApp::createColorResources() {
    // TODO(p02-t08):
    //   m_sampleCount == VK_SAMPLE_COUNT_1_BIT 时直接 return（设备不支持，退回单采样）。
    //
    //   否则建一张多采样的离屏颜色附件 —— swapchain 的图像永远是单采样的
    //   （显示引擎不认识多重采样），所以必须另开一张来画，画完由 render pass
    //   的 pResolveAttachments 解析过去。
    //     尺寸同 swapchain，mipLevels = 1，samples = m_sampleCount，
    //     format 用 m_swapchain->format()
    //     usage = TRANSIENT_ATTACHMENT | COLOR_ATTACHMENT
    //       TRANSIENT 告诉驱动「这块内容不会离开这趟 render pass」，
    //       tile-based GPU（Apple Silicon / 移动端）据此可以把它完全留在片上内存。
    //   再用 createImageView 建视图（aspect = COLOR, mipLevels = 1）。
    RWB_TODO("p02-t08 ResourceApp::createColorResources");
}

} // namespace p02
