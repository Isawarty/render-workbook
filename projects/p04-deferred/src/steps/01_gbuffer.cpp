// t01 —— G-buffer 资源与 render-pass 契约
#include "../DeferredApp.h"

#include "rwb/rhi/VmaUsage.h"

#include <array>

namespace p04 {

Image DeferredApp::createImage(VkFormat format, VkImageUsageFlags usage,
                               VkImageAspectFlags aspect) {
    const VkExtent2D extent = m_swapchain->extent();
    return createImage(extent.width, extent.height, format, usage, aspect);
}

Image DeferredApp::createImage(std::uint32_t width, std::uint32_t height,
                               VkFormat format, VkImageUsageFlags usage,
                               VkImageAspectFlags aspect) {
    Image image;
    image.format = format;
    image.width = width;
    image.height = height;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    VK_CHECK(vmaCreateImage(m_ctx->allocator(), &imageInfo, &allocationInfo,
                            &image.handle, &image.allocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.handle;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    try {
        VK_CHECK(vkCreateImageView(m_ctx->device(), &viewInfo, nullptr, &image.view));
    } catch (...) {
        vmaDestroyImage(m_ctx->allocator(), image.handle, image.allocation);
        throw;
    }
    return image;
}

void DeferredApp::createGBufferResources() {
    constexpr VkImageUsageFlags colorUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    m_contract = {
        {GBufferSlot::AlbedoMetallic, VK_FORMAT_R8G8B8A8_UNORM, colorUsage,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4},
        {GBufferSlot::NormalRoughness, VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 8},
        {GBufferSlot::EmissiveAo, VK_FORMAT_R8G8B8A8_UNORM, colorUsage,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4},
    };

    m_gbuffer.reserve(m_contract.size());
    for (const AttachmentContract& contract : m_contract) {
        m_gbuffer.push_back(createImage(contract.format, contract.usage,
                                        VK_IMAGE_ASPECT_COLOR_BIT));
    }
    m_depth = createImage(findDepthFormat(),
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_DEPTH_BIT);
    if (m_config.offscreenCapture) {
        // Tonemap is the HDR -> LDR boundary. The capture target should match
        // that LDR contract; keeping it half-float lets backend-specific
        // undefined conversion values survive while the swapchain clamps them.
        m_finalColor = createImage(VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void DeferredApp::createRenderPass() {
    const bool useHdr = stageIndex(m_stage) >= stageIndex(Stage::Bloom);
    std::array<VkAttachmentDescription, 5> attachments{};
    for (std::size_t i = 0; i < m_contract.size(); ++i) {
        attachments[i].format = m_contract[i].format;
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout = m_contract[i].finalLayout;
    }

    attachments[3].format = m_depth.format;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    attachments[4].format = useHdr ? m_hdr.format
                                    : (m_config.offscreenCapture ? m_finalColor.format
                                                                 : m_swapchain->format());
    attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[4].finalLayout = useHdr || m_config.offscreenCapture
                                     ? VK_IMAGE_LAYOUT_GENERAL
                                     : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    const std::array<VkAttachmentReference, 3> geometryColors{{
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    }};
    const VkAttachmentReference depthRef{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference presentRef{4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const std::array<VkAttachmentReference, 4> lightingInputs{{
        {0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
    }};

    std::array<VkSubpassDescription, 2> subpasses{};
    subpasses[kGeometrySubpass].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[kGeometrySubpass].colorAttachmentCount =
        static_cast<std::uint32_t>(geometryColors.size());
    subpasses[kGeometrySubpass].pColorAttachments = geometryColors.data();
    subpasses[kGeometrySubpass].pDepthStencilAttachment = &depthRef;
    subpasses[kLightingSubpass].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[kLightingSubpass].inputAttachmentCount =
        static_cast<std::uint32_t>(lightingInputs.size());
    subpasses[kLightingSubpass].pInputAttachments = lightingInputs.data();
    subpasses[kLightingSubpass].colorAttachmentCount = 1;
    subpasses[kLightingSubpass].pColorAttachments = &presentRef;

    std::array<VkSubpassDependency, 3> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = kGeometrySubpass;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = dependencies[0].srcStageMask;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = kGeometrySubpass;
    dependencies[1].dstSubpass = kLightingSubpass;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    m_gbufferBarrier = {dependencies[1].srcStageMask, dependencies[1].dstStageMask,
                        dependencies[1].srcAccessMask, dependencies[1].dstAccessMask};

    dependencies[2].srcSubpass = kLightingSubpass;
    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstStageMask = useHdr ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                          : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[2].dstAccessMask = useHdr ? VK_ACCESS_SHADER_READ_BIT : 0;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = static_cast<std::uint32_t>(subpasses.size());
    info.pSubpasses = subpasses.data();
    info.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    info.pDependencies = dependencies.data();
    VK_CHECK(vkCreateRenderPass(m_ctx->device(), &info, nullptr, &m_renderPass));
}

void DeferredApp::createFramebuffers() {
    m_framebuffers.resize(m_swapchain->imageCount());
    for (std::size_t i = 0; i < m_framebuffers.size(); ++i) {
        const std::array<VkImageView, 5> views{
            m_gbuffer[0].view, m_gbuffer[1].view, m_gbuffer[2].view,
            m_depth.view,
            stageIndex(m_stage) >= stageIndex(Stage::Bloom)
                ? m_hdr.view
                : (m_config.offscreenCapture ? m_finalColor.view
                                             : m_swapchain->imageViews()[i]),
        };
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = static_cast<std::uint32_t>(views.size());
        info.pAttachments = views.data();
        info.width = m_swapchain->extent().width;
        info.height = m_swapchain->extent().height;
        info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(m_ctx->device(), &info, nullptr, &m_framebuffers[i]));
    }
}

} // namespace p04
