// t04 —— compute 生成 irradiance / prefiltered environment / BRDF LUT
#include "../DeferredApp.h"
#include "rwb/rhi/Shader.h"

#include <string>

namespace p04 {

void DeferredApp::createIblResources() {
    constexpr std::uint32_t kVec4Count = 22; // 1 irradiance + 5 prefilter + 16 LUT
    m_iblBuffer = createBuffer(sizeof(float) * 4 * kVec4Count,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo setInfo{};
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setInfo.bindingCount = 1;
    setInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &setInfo, nullptr, &m_iblSetLayout));

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_iblSetLayout;
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_iblLayout));

    const std::string dir = m_useSlang ? std::string(RWB_SLANG_SHADER_DIR)
                                       : std::string(RWB_SHADER_DIR);
    const std::string name = m_useSlang ? "/ibl.comp.slang.spv" : "/ibl.comp.spv";
    auto shader = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + name);
    const auto stage = shader.stageInfo(VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stage;
    pipelineInfo.layout = m_iblLayout;
    VK_CHECK(vkCreateComputePipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                      nullptr, &m_iblPipeline));

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &poolInfo, nullptr, &m_iblPool));
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_iblPool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &m_iblSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &alloc, &m_iblSet));
    VkDescriptorBufferInfo bufferInfo{m_iblBuffer.handle, 0, m_iblBuffer.size};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_iblSet;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_ctx->device(), 1, &write, 0, nullptr);

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_iblPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_iblLayout,
                                0, 1, &m_iblSet, 0, nullptr);
        vkCmdDispatch(cmd, 1, 1, 1);
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        m_iblBarrier = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        barrier.srcAccessMask, barrier.dstAccessMask};
        vkCmdPipelineBarrier(cmd, m_iblBarrier.srcStage, m_iblBarrier.dstStage,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
    });
}

} // namespace p04
