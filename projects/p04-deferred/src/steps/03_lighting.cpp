// t03 —— lighting pass：读取 input attachments，执行 Cook-Torrance
#include "../DeferredApp.h"
#include "rwb/rhi/Shader.h"

#include <array>
#include <string>

namespace p04 {

void DeferredApp::createLightingResources() {
    const bool useIbl = stageIndex(m_stage) >= stageIndex(Stage::Ibl);
    const bool useShadows = stageIndex(m_stage) >= stageIndex(Stage::Shadows);
    if (!m_lightingUniform.valid()) {
        m_lightingUniform = createMappedBuffer(sizeof(LightingUniform),
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(7);
    for (std::uint32_t i = 0; i < 4; ++i) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }
    if (useIbl) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 4;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }
    if (useShadows) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 5;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        // portability subset 可能不支持 mutable comparison samplers（MoltenVK）。
        // 固化进 layout 后，descriptor 只更新 image view 即可。
        binding.pImmutableSamplers = &m_shadowSampler;
        bindings.push_back(binding);
    }
    VkDescriptorSetLayoutBinding frameBinding{};
    frameBinding.binding = 6;
    frameBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameBinding.descriptorCount = 1;
    frameBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(frameBinding);
    VkDescriptorSetLayoutCreateInfo setInfo{};
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setInfo.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &setInfo, nullptr,
                                         &m_lightingSetLayout));

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
    if (useIbl) poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});
    if (useShadows) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1});
    }
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &poolInfo, nullptr, &m_descriptorPool));

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_descriptorPool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &m_lightingSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &alloc, &m_lightingSet));

    const std::array<VkDescriptorImageInfo, 4> images{{
        {VK_NULL_HANDLE, m_gbuffer[0].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, m_gbuffer[1].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, m_gbuffer[2].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, m_depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}}};
    std::array<VkWriteDescriptorSet, 4> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_lightingSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[i].pImageInfo = &images[i];
    }
    vkUpdateDescriptorSets(m_ctx->device(), static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    if (useIbl) {
        VkDescriptorBufferInfo bufferInfo{m_iblBuffer.handle, 0, m_iblBuffer.size};
        VkWriteDescriptorSet iblWrite{};
        iblWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        iblWrite.dstSet = m_lightingSet;
        iblWrite.dstBinding = 4;
        iblWrite.descriptorCount = 1;
        iblWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        iblWrite.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(m_ctx->device(), 1, &iblWrite, 0, nullptr);
    }
    if (useShadows) {
        VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, m_shadowDepth.view,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet shadowWrite{};
        shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowWrite.dstSet = m_lightingSet;
        shadowWrite.dstBinding = 5;
        shadowWrite.descriptorCount = 1;
        shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_ctx->device(), 1, &shadowWrite, 0, nullptr);
    }
    VkDescriptorBufferInfo frameInfo{m_lightingUniform.handle, 0,
                                     sizeof(LightingUniform)};
    VkWriteDescriptorSet frameWrite{};
    frameWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    frameWrite.dstSet = m_lightingSet;
    frameWrite.dstBinding = 6;
    frameWrite.descriptorCount = 1;
    frameWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameWrite.pBufferInfo = &frameInfo;
    vkUpdateDescriptorSets(m_ctx->device(), 1, &frameWrite, 0, nullptr);
}

void DeferredApp::createLightingPipeline() {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_lightingSetLayout;
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_lightingLayout));

    const std::string dir = m_useSlang ? std::string(RWB_SLANG_SHADER_DIR) + "/"
                                       : std::string(RWB_SHADER_DIR) + "/";
    auto vert = rwb::rhi::ShaderModule::fromFile(
        m_ctx->device(), dir + (m_useSlang ? "lighting.vert.slang.spv"
                                           : "lighting.vert.spv"));
    const char* fragment = m_useSlang ? "lighting_hdr.frag.slang.spv"
                                      : "lighting.frag.spv";
    if (!m_useSlang) {
        if (stageIndex(m_stage) >= stageIndex(Stage::Bloom)) {
            fragment = "lighting_hdr.frag.spv";
        } else if (stageIndex(m_stage) >= stageIndex(Stage::Shadows)) {
            fragment = "lighting_shadow.frag.spv";
        } else if (stageIndex(m_stage) >= stageIndex(Stage::Ibl)) {
            fragment = "lighting_ibl.frag.spv";
        }
    }
    auto frag = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + fragment);
    const VkPipelineShaderStageCreateInfo stages[] = {
        vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT), frag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkExtent2D extent = m_swapchain->extent();
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
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &msaa;
    info.pColorBlendState = &blend;
    info.layout = m_lightingLayout;
    info.renderPass = m_renderPass;
    info.subpass = kLightingSubpass;
    VK_CHECK(vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &info, nullptr,
                                       &m_lightingPipeline));
}

} // namespace p04
