// t08 —— MSAA（多重采样抗锯齿）
//
// 锯齿的根因是「一个像素只在中心采一次样」：三角形边缘要么全覆盖要么不覆盖，
// 没有中间值。MSAA 的做法是每像素放 N 个采样点做覆盖测试，
// 但片元着色器仍然只跑一次 —— 这就是它比超采样便宜得多的原因。
//
// 代价在显存和带宽：一个 4x MSAA 的颜色附件占 4 倍空间。
#include "../ResourceApp.h"

namespace p02 {

// 采样数不能随便挑：必须是「颜色附件」和「深度附件」都支持的那些位的交集。
//
// 只查 framebufferColorSampleCounts 是个经典错误 —— 某些硬件上颜色支持 8x
// 但深度只到 4x，建 framebuffer 时才会失败，而错误信息不会告诉你是深度的锅。
VkSampleCountFlagBits ResourceApp::maxUsableSampleCount() const {
    const VkPhysicalDeviceLimits& limits = m_ctx->properties().limits;

    const VkSampleCountFlags counts = limits.framebufferColorSampleCounts &
                                      limits.framebufferDepthSampleCounts;

    // 从高到低取第一个可用的。这门课封顶到 4x：
    // 8x 以上的画质收益迅速递减，而显存占用是线性涨的。
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;   // 软件渲染器上很可能就是这个
}

// 多重采样的颜色附件。
//
// swapchain 的图像永远是单采样的（显示引擎不认识多重采样），
// 所以必须另开一张多采样的离屏图来画，画完在 render pass 里 resolve 过去。
// resolve 这一步由 subpass 的 pResolveAttachments 声明，驱动自己完成。
void ResourceApp::createColorResources() {
    if (m_sampleCount == VK_SAMPLE_COUNT_1_BIT) return;   // 设备不支持，退回单采样

    m_colorTarget = createImage(m_swapchain->extent().width, m_swapchain->extent().height,
                                /*mipLevels=*/1, m_sampleCount, m_swapchain->format(),
                                VK_IMAGE_TILING_OPTIMAL,
                                // TRANSIENT: 告诉驱动「这块内容不会离开这趟 render pass」。
                                // tile-based GPU（Apple Silicon / 移动端）据此可以把它
                                // 完全留在片上内存，一个字节都不写回显存。
                                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    m_colorTarget.view = createImageView(m_colorTarget.handle, m_colorTarget.format,
                                         VK_IMAGE_ASPECT_COLOR_BIT, /*mipLevels=*/1);
}

} // namespace p02
