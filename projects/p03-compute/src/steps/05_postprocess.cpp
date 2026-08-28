// t05 —— Gaussian blur / tonemap，compute → graphics barrier
//
// 任务书: projects/p03-compute/docs/t05-postprocess.md
// 判分:   python rwb.py test p03-t05

#include "../ComputeApp.h"

#include "rwb/rhi/Readback.h"
#include "rwb/rhi/Shader.h"

#include <stdexcept>

namespace p03 {
namespace {

struct ImagePush { std::uint32_t width = 0, height = 0; };

struct GraphicsConsumer {
    VkRenderPass          renderPass = VK_NULL_HANDLE;
    VkFramebuffer         framebuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout      layout = VK_NULL_HANDLE;
    VkPipeline            pipeline = VK_NULL_HANDLE;
    VkDescriptorSet       set = VK_NULL_HANDLE;
};

void destroyGraphicsConsumer(VkDevice device, GraphicsConsumer& g) noexcept {
    if (g.pipeline) vkDestroyPipeline(device, g.pipeline, nullptr);
    if (g.layout) vkDestroyPipelineLayout(device, g.layout, nullptr);
    if (g.setLayout) vkDestroyDescriptorSetLayout(device, g.setLayout, nullptr);
    if (g.framebuffer) vkDestroyFramebuffer(device, g.framebuffer, nullptr);
    if (g.renderPass) vkDestroyRenderPass(device, g.renderPass, nullptr);
    g = {};
}

} // namespace

std::vector<float> ComputeApp::runPostprocess(const std::vector<float>& rgba,
                                              std::uint32_t width,
                                              std::uint32_t height) {
    if (width == 0 || height == 0) {
        if (rgba.empty()) return {};
        throw std::runtime_error("postprocess: 空尺寸必须配空输入");
    }
    const std::size_t floatCount = static_cast<std::size_t>(width) * height * 4;
    if (rgba.size() != floatCount) throw std::runtime_error("postprocess: RGBA 元素数量不匹配");

    const VkDeviceSize bytes = sizeof(float) * static_cast<VkDeviceSize>(floatCount);
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ScopedBuffer input(*this, createBuffer(bytes, usage, false, false));
    ScopedBuffer filtered(*this, createBuffer(bytes, usage, false, false));
    ScopedBuffer consumed(*this, createBuffer(bytes, usage, false, false));
    uploadToBuffer(rgba.data(), bytes, input.get());

    ScopedPipeline compute(
        *this, createComputePipeline("postprocess.comp.spv", 2, sizeof(ImagePush)));
    const VkDescriptorSet computeSet =
        allocateStorageSet(compute.get(), {&input.get(), &filtered.get()});

    VkDevice device = m_ctx->device();
    GraphicsConsumer g;
    try {
        VkDescriptorSetLayoutBinding bindings[2]{};
        for (std::uint32_t i = 0; i < 2; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo setInfo{};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setInfo.bindingCount = 2;
        setInfo.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &g.setLayout));

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.size = sizeof(ImagePush);
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &g.setLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &g.layout));

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        VK_CHECK(vkCreateRenderPass(device, &rpInfo, nullptr, &g.renderPass));

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = g.renderPass;
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &g.framebuffer));

        auto vert = rwb::rhi::ShaderModule::fromFile(device, shaderPath("postprocess.vert.spv"));
        auto frag = rwb::rhi::ShaderModule::fromFile(device, shaderPath("postprocess.frag.spv"));
        VkPipelineShaderStageCreateInfo stages[] = {
            vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        VkRect2D scissor{{0, 0}, {width, height}};
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
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &msaa;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.layout = g.layout;
        pipelineInfo.renderPass = g.renderPass;
        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                           &g.pipeline));

        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = descriptorPool();
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &g.setLayout;
        VK_CHECK(vkAllocateDescriptorSets(device, &alloc, &g.set));
        VkDescriptorBufferInfo infos[] = {{filtered.handle(), 0, VK_WHOLE_SIZE},
                                          {consumed.handle(), 0, VK_WHOLE_SIZE}};
        VkWriteDescriptorSet writes[2]{};
        for (std::uint32_t i = 0; i < 2; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = g.set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &infos[i];
        }
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

        const ImagePush push{width, height};
        m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute.get().pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute.get().layout,
                                    0, 1, &computeSet, 0, nullptr);
            vkCmdPushConstants(cmd, compute.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(push), &push);
            vkCmdDispatch(cmd, (width + 7) / 8, (height + 7) / 8, 1);

            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            m_lastBarrier = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             barrier.srcAccessMask, barrier.dstAccessMask};
            vkCmdPipelineBarrier(cmd, m_lastBarrier.srcStage, m_lastBarrier.dstStage, 0, 1,
                                 &barrier, 0, nullptr, 0, nullptr);

            VkRenderPassBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            begin.renderPass = g.renderPass;
            begin.framebuffer = g.framebuffer;
            begin.renderArea.extent = {width, height};
            vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.layout, 0, 1,
                                    &g.set, 0, nullptr);
            vkCmdPushConstants(cmd, g.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                               &push);
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);

            VkMemoryBarrier readback{};
            readback.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            readback.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            readback.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &readback, 0, nullptr,
                                 0, nullptr);
        });
    } catch (...) {
        destroyGraphicsConsumer(device, g);
        throw;
    }

    auto result = rwb::rhi::readbackBufferAs<float>(*m_ctx, consumed.handle(), floatCount);
    destroyGraphicsConsumer(device, g);
    return result;
}

} // namespace p03
