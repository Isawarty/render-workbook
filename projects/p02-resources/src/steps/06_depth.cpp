// t06 —— 深度缓冲 / 深度测试
//
// P1 到 t05 为止，画面全靠「后画的盖住先画的」。一旦有了立方体，
// 这条规则立刻失效：立方体的背面在索引缓冲里排在正面之后，于是背面把正面盖住了。
//
// 深度缓冲把「谁在前面」从绘制顺序里解耦出来 —— 这是 3D 渲染的地基。
#include "../ResourceApp.h"

namespace p02 {

// 深度格式不是所有设备都一样。按「精度从高到低」排队挑第一个可用的。
//
// D32_SFLOAT       32 位浮点，桌面卡的标配
// D32_SFLOAT_S8_UI 带 8 位模板，模板测试（描边、遮罩）要用
// D24_UNORM_S8_UI  24 位定点 + 模板，移动端和一些老卡上更常见
//
// 写死 D32_SFLOAT 在多数机器上没事，直到某天在 MoltenVK 或安卓上拿到
// VK_ERROR_FORMAT_NOT_SUPPORTED —— 而那时你已经忘了这里。
VkFormat ResourceApp::findDepthFormat() const {
    return m_ctx->findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void ResourceApp::createDepthResources() {
    const VkFormat format = findDepthFormat();

    // 深度图必须和颜色附件同尺寸、同采样数，否则 framebuffer 建不出来。
    // m_sampleCount 在 t08 之前恒为 1，t08 之后跟着颜色附件走。
    m_depth = createImage(m_swapchain->extent().width, m_swapchain->extent().height,
                          /*mipLevels=*/1, m_sampleCount, format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    m_depth.view = createImageView(m_depth.handle, format,
                                   VK_IMAGE_ASPECT_DEPTH_BIT, /*mipLevels=*/1);

    // 严格来说这一步可以省：render pass 里深度附件的 initialLayout 是 UNDEFINED，
    // loadOp 是 CLEAR，driver 会自己完成转换。
    // 这里显式做一次，是为了让「资源建好之后先转到它该在的 layout」
    // 这个习惯成型 —— 到 P4 的 G-Buffer 就没有 render pass 替你兜底了。
    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        transitionImageLayout(cmd, m_depth, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_ASPECT_DEPTH_BIT);
    });
}

} // namespace p02
