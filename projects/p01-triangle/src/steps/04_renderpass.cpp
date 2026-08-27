// t04 — render pass / framebuffer
//
// render pass 是 Vulkan 里最没有 OpenGL 对应物的概念之一。
// 它是一份「这一趟渲染会碰哪些附件、怎么读写、彼此依赖关系如何」的声明，
// 驱动据此提前决定 tile 划分、内存布局转换的时机、以及能不能省掉一次写回。
// 移动端 tile-based GPU 从中获益最大，桌面端则主要换来显式的 layout 管理。
#include "../TriangleApp.h"

#include <array>
#include <stdexcept>

namespace p01 {

void TriangleApp::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format  = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    // loadOp = CLEAR: 进入 pass 时清屏。
    // 选 CLEAR 而不是 LOAD 不只是省一次清屏调用 —— 它告诉驱动
    // 「旧内容我不要了」，驱动因此可以完全跳过从显存读回 tile 的步骤。
    colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // initialLayout = UNDEFINED 配合 loadOp = CLEAR：
    // 「我不关心之前是什么布局，反正要全部覆盖」。
    // finalLayout = PRESENT_SRC：pass 结束时硬件自动转成可呈现的布局，
    // 省掉一次手写的 pipeline barrier。
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;   // 索引进 pAttachments
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;
    // 这里 pColorAttachments 的下标就是 shader 里 layout(location = 0) out 的 location。

    // 这条 dependency 是 P1 里最容易被忽略、也最容易被 sync validation 抓住的地方。
    //
    // 问题：vkAcquireNextImageKHR 用 semaphore 通知「图像可用了」，
    //       但 semaphore 只保证「等到了」，不保证 layout 转换发生在正确的时刻。
    //       render pass 隐式的 UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL 转换
    //       可能早于图像真正可用。
    // 解法：声明一条从 SUBPASS_EXTERNAL（pass 之前的一切）到 subpass 0 的依赖，
    //       把转换钉在 COLOR_ATTACHMENT_OUTPUT 阶段之后。
    //       这个阶段正是我们在提交时让 imageAvailable semaphore 等待的阶段（见 t07）。
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments    = &colorAttachment;
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dependency;

    if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateRenderPass 失败");
    }
}

void TriangleApp::createFramebuffers() {
    // render pass 说的是「附件的格式和用法」，framebuffer 说的是「具体是哪几张图」。
    // 所以每张 swapchain 图像都要一个 framebuffer，但它们共享同一个 render pass。
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (std::size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        const std::array<VkImageView, 1> attachments = {m_swapchainImageViews[i]};

        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = m_renderPass;
        info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        info.pAttachments    = attachments.data();
        info.width           = m_swapchainExtent.width;
        info.height          = m_swapchainExtent.height;
        info.layers          = 1;

        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer 失败");
        }
    }
}

} // namespace p01
