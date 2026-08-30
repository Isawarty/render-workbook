// t06 —— HDR lighting -> compute bloom -> tonemap present
#include "../DeferredApp.h"
#include "rwb/rhi/Shader.h"

#include <array>
#include <string>

namespace p04 {

void DeferredApp::createBloomResources() {
    const VkExtent2D extent = m_swapchain->extent();
    constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_hdr = createImage(format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_STORAGE_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT);
    m_bloom = createImage(format, VK_IMAGE_USAGE_STORAGE_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT);

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_bloom.handle;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);
    });

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    VK_CHECK(vkCreateSampler(m_ctx->device(), &samplerInfo, nullptr, &m_postSampler));

    std::array<VkDescriptorSetLayoutBinding, 2> computeBindings{};
    for (std::uint32_t i = 0; i < computeBindings.size(); ++i) {
        computeBindings[i].binding = i;
        computeBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        computeBindings[i].descriptorCount = 1;
        computeBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo computeSetInfo{};
    computeSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeSetInfo.bindingCount = static_cast<std::uint32_t>(computeBindings.size());
    computeSetInfo.pBindings = computeBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &computeSetInfo, nullptr,
                                         &m_bloomSetLayout));
    VkPipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &m_bloomSetLayout;
    VkPushConstantRange bloomPush{};
    bloomPush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bloomPush.size = sizeof(std::uint32_t) * 2;
    computeLayoutInfo.pushConstantRangeCount = 1;
    computeLayoutInfo.pPushConstantRanges = &bloomPush;
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &computeLayoutInfo, nullptr,
                                    &m_bloomLayout));
    const std::string shaderDir = m_useSlang ? std::string(RWB_SLANG_SHADER_DIR)
                                             : std::string(RWB_SHADER_DIR);
    const std::string computeName = m_useSlang ? "/bloom.comp.slang.spv"
                                                : "/bloom.comp.spv";
    auto computeShader = rwb::rhi::ShaderModule::fromFile(
        m_ctx->device(), shaderDir + computeName);
    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.stage = computeShader.stageInfo(VK_SHADER_STAGE_COMPUTE_BIT);
    computePipelineInfo.layout = m_bloomLayout;
    VK_CHECK(vkCreateComputePipelines(m_ctx->device(), VK_NULL_HANDLE, 1,
                                      &computePipelineInfo, nullptr, &m_bloomPipeline));
    VkDescriptorPoolSize computePoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2};
    VkDescriptorPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    computePoolInfo.maxSets = 1;
    computePoolInfo.poolSizeCount = 1;
    computePoolInfo.pPoolSizes = &computePoolSize;
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &computePoolInfo, nullptr, &m_bloomPool));
    VkDescriptorSetAllocateInfo computeAlloc{};
    computeAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeAlloc.descriptorPool = m_bloomPool;
    computeAlloc.descriptorSetCount = 1;
    computeAlloc.pSetLayouts = &m_bloomSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &computeAlloc, &m_bloomSet));
    const std::array<VkDescriptorImageInfo, 2> storageImages{{
        {VK_NULL_HANDLE, m_hdr.view, VK_IMAGE_LAYOUT_GENERAL},
        {VK_NULL_HANDLE, m_bloom.view, VK_IMAGE_LAYOUT_GENERAL}}};
    std::array<VkWriteDescriptorSet, 2> storageWrites{};
    for (std::uint32_t i = 0; i < storageWrites.size(); ++i) {
        storageWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        storageWrites[i].dstSet = m_bloomSet;
        storageWrites[i].dstBinding = i;
        storageWrites[i].descriptorCount = 1;
        storageWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storageWrites[i].pImageInfo = &storageImages[i];
    }
    vkUpdateDescriptorSets(m_ctx->device(), 2, storageWrites.data(), 0, nullptr);

    VkAttachmentDescription color{};
    color.format = m_config.offscreenCapture ? m_finalColor.format
                                              : m_swapchain->format();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = m_config.offscreenCapture ? VK_IMAGE_LAYOUT_GENERAL
                                                   : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    VkRenderPassCreateInfo postPassInfo{};
    postPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    postPassInfo.attachmentCount = 1;
    postPassInfo.pAttachments = &color;
    postPassInfo.subpassCount = 1;
    postPassInfo.pSubpasses = &subpass;
    VK_CHECK(vkCreateRenderPass(m_ctx->device(), &postPassInfo, nullptr, &m_postRenderPass));
    m_postFramebuffers.resize(m_swapchain->imageCount());
    for (std::size_t i = 0; i < m_postFramebuffers.size(); ++i) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_postRenderPass;
        fbInfo.attachmentCount = 1;
        const VkImageView targetView = m_config.offscreenCapture
                                           ? m_finalColor.view
                                           : m_swapchain->imageViews()[i];
        fbInfo.pAttachments = &targetView;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        VK_CHECK(vkCreateFramebuffer(m_ctx->device(), &fbInfo, nullptr,
                                     &m_postFramebuffers[i]));
    }

    std::array<VkDescriptorSetLayoutBinding, 2> postBindings{};
    for (std::uint32_t i = 0; i < postBindings.size(); ++i) {
        postBindings[i].binding = i;
        postBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        postBindings[i].descriptorCount = 1;
        postBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo postSetInfo{};
    postSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postSetInfo.bindingCount = 2;
    postSetInfo.pBindings = postBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &postSetInfo, nullptr,
                                         &m_postSetLayout));
    VkDescriptorPoolSize postPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2};
    VkDescriptorPoolCreateInfo postPoolInfo{};
    postPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    postPoolInfo.maxSets = 1;
    postPoolInfo.poolSizeCount = 1;
    postPoolInfo.pPoolSizes = &postPoolSize;
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &postPoolInfo, nullptr, &m_postPool));
    VkDescriptorSetAllocateInfo postAlloc{};
    postAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    postAlloc.descriptorPool = m_postPool;
    postAlloc.descriptorSetCount = 1;
    postAlloc.pSetLayouts = &m_postSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &postAlloc, &m_postSet));
    const std::array<VkDescriptorImageInfo, 2> sampledImages{{
        {m_postSampler, m_hdr.view, VK_IMAGE_LAYOUT_GENERAL},
        {m_postSampler, m_bloom.view, VK_IMAGE_LAYOUT_GENERAL}}};
    std::array<VkWriteDescriptorSet, 2> sampledWrites{};
    for (std::uint32_t i = 0; i < sampledWrites.size(); ++i) {
        sampledWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampledWrites[i].dstSet = m_postSet;
        sampledWrites[i].dstBinding = i;
        sampledWrites[i].descriptorCount = 1;
        sampledWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampledWrites[i].pImageInfo = &sampledImages[i];
    }
    vkUpdateDescriptorSets(m_ctx->device(), 2, sampledWrites.data(), 0, nullptr);

    VkPipelineLayoutCreateInfo postLayoutInfo{};
    postLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    postLayoutInfo.setLayoutCount = 1;
    postLayoutInfo.pSetLayouts = &m_postSetLayout;
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &postLayoutInfo, nullptr, &m_postLayout));
    const std::string dir = m_useSlang ? std::string(RWB_SLANG_SHADER_DIR) + "/"
                                       : std::string(RWB_SHADER_DIR) + "/";
    const std::string suffix = m_useSlang ? ".slang.spv" : ".spv";
    auto vert = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + "tonemap.vert" + suffix);
    auto frag = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + "tonemap.frag" + suffix);
    const VkPipelineShaderStageCreateInfo stages[] = {
        vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT), frag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0, 0, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;
    VkGraphicsPipelineCreateInfo postPipelineInfo{};
    postPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    postPipelineInfo.stageCount = 2;
    postPipelineInfo.pStages = stages;
    postPipelineInfo.pVertexInputState = &vertexInput;
    postPipelineInfo.pInputAssemblyState = &assembly;
    postPipelineInfo.pViewportState = &viewportState;
    postPipelineInfo.pRasterizationState = &raster;
    postPipelineInfo.pMultisampleState = &msaa;
    postPipelineInfo.pColorBlendState = &blend;
    postPipelineInfo.layout = m_postLayout;
    postPipelineInfo.renderPass = m_postRenderPass;
    VK_CHECK(vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &postPipelineInfo,
                                       nullptr, &m_postPipeline));
}

void DeferredApp::destroyBloomResources() noexcept {
    if (!m_ctx) return;
    if (m_postPipeline) vkDestroyPipeline(m_ctx->device(), m_postPipeline, nullptr);
    if (m_postLayout) vkDestroyPipelineLayout(m_ctx->device(), m_postLayout, nullptr);
    if (m_postSetLayout) vkDestroyDescriptorSetLayout(m_ctx->device(), m_postSetLayout, nullptr);
    if (m_postPool) vkDestroyDescriptorPool(m_ctx->device(), m_postPool, nullptr);
    for (VkFramebuffer framebuffer : m_postFramebuffers) {
        vkDestroyFramebuffer(m_ctx->device(), framebuffer, nullptr);
    }
    m_postFramebuffers.clear();
    if (m_postRenderPass) vkDestroyRenderPass(m_ctx->device(), m_postRenderPass, nullptr);
    if (m_bloomPipeline) vkDestroyPipeline(m_ctx->device(), m_bloomPipeline, nullptr);
    if (m_bloomLayout) vkDestroyPipelineLayout(m_ctx->device(), m_bloomLayout, nullptr);
    if (m_bloomSetLayout) vkDestroyDescriptorSetLayout(m_ctx->device(), m_bloomSetLayout, nullptr);
    if (m_bloomPool) vkDestroyDescriptorPool(m_ctx->device(), m_bloomPool, nullptr);
    if (m_postSampler) vkDestroySampler(m_ctx->device(), m_postSampler, nullptr);
    destroyImage(m_bloom);
    destroyImage(m_hdr);
    m_postPipeline = VK_NULL_HANDLE;
    m_postLayout = VK_NULL_HANDLE;
    m_postSetLayout = VK_NULL_HANDLE;
    m_postPool = VK_NULL_HANDLE;
    m_postSet = VK_NULL_HANDLE;
    m_postRenderPass = VK_NULL_HANDLE;
    m_bloomPipeline = VK_NULL_HANDLE;
    m_bloomLayout = VK_NULL_HANDLE;
    m_bloomSetLayout = VK_NULL_HANDLE;
    m_bloomPool = VK_NULL_HANDLE;
    m_bloomSet = VK_NULL_HANDLE;
    m_postSampler = VK_NULL_HANDLE;
}

} // namespace p04
