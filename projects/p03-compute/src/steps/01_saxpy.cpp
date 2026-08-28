// t01 —— compute pipeline / storage buffer / dispatch / 回读
//
// 任务书: projects/p03-compute/docs/t01-saxpy.md
// 判分:   python rwb.py test p03-t01

#include "../ComputeApp.h"

#include "rwb/core/Todo.h"
#include "rwb/rhi/Readback.h"
#include "rwb/rhi/Shader.h"

#include <stdexcept>
#include <vector>

namespace p03 {
namespace {

// push constant 块。布局必须和 saxpy.comp 里的 `Push` 一致。
struct SaxpyPush {
    float         a = 0.0f;
    std::uint32_t n = 0;
};

} // namespace

// ---------------------------------------------------------------------------

VkDescriptorPool ComputeApp::descriptorPool() {
    if (m_descriptorPool != VK_NULL_HANDLE) return m_descriptorPool;

    // P3 只用一种 descriptor：storage buffer。
    // 池子开得比实际需要大一截 —— 池满了会得到 VK_ERROR_OUT_OF_POOL_MEMORY，
    // 那是个「配置错误」，不该在做题的时候浪费你时间。
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 128;

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets       = 64;
    info.poolSizeCount = 1;
    info.pPoolSizes    = &poolSize;

    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &info, nullptr, &m_descriptorPool));
    return m_descriptorPool;
}

ComputePipeline ComputeApp::createComputePipeline(const std::string& spvName,
                                                  std::uint32_t      storageBindingCount,
                                                  std::uint32_t      pushConstantSize) {
    VkDevice device = m_ctx->device();

    ComputePipeline pipe;
    pipe.bindingCount = storageBindingCount;

    try {
        // 1. descriptor set layout —— 「这个 shader 期待几个 storage buffer」。
        //    binding 号必须和 shader 里的 layout(binding = N) 对上。
        std::vector<VkDescriptorSetLayoutBinding> bindings(storageBindingCount);
        for (std::uint32_t i = 0; i < storageBindingCount; ++i) {
            bindings[i].binding         = i;
            bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
        setLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        setLayoutInfo.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &pipe.setLayout));

        // 2. pipeline layout = set layout + push constant range。
        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset     = 0;
        range.size       = pushConstantSize;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &pipe.setLayout;
        layoutInfo.pushConstantRangeCount = (pushConstantSize > 0) ? 1u : 0u;
        layoutInfo.pPushConstantRanges    = (pushConstantSize > 0) ? &range : nullptr;
        VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipe.layout));

        // 3. compute pipeline。比 graphics pipeline 简单得多：
        //    没有顶点输入、没有光栅化状态、没有 render pass —— 只有一个 stage。
        rwb::rhi::ShaderModule shader =
            rwb::rhi::ShaderModule::fromFile(device, shaderPath(spvName));

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage  = shader.stageInfo(VK_SHADER_STAGE_COMPUTE_BIT);
        pipelineInfo.layout = pipe.layout;
        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                          &pipe.pipeline));
    } catch (...) {
        // 中途失败的话，前面建好的那一半必须收掉，否则 vkDestroyDevice 会报
        // 「还有对象活着」，把 L1 判成红 —— 而错误行号指向设备销毁，看不出是谁漏的。
        destroyComputePipeline(pipe);
        throw;
    }

    return pipe;
}

VkDescriptorSet ComputeApp::allocateStorageSet(const ComputePipeline&            pipe,
                                               const std::vector<const Buffer*>& buffers) {
    if (buffers.size() != pipe.bindingCount) {
        throw std::runtime_error(rwb::format(
            "descriptor set 要 %u 个 buffer，给了 %zu 个", pipe.bindingCount, buffers.size()));
    }

    VkDevice device = m_ctx->device();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &pipe.setLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set));

    // set 分出来时是空的，得把 buffer 写进去才能用。
    // bufferInfos 必须活到 vkUpdateDescriptorSets 调用完 —— writes 里存的是指针。
    std::vector<VkDescriptorBufferInfo> bufferInfos(buffers.size());
    std::vector<VkWriteDescriptorSet>   writes(buffers.size());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        bufferInfos[i].buffer = buffers[i]->handle;
        bufferInfos[i].offset = 0;
        bufferInfos[i].range  = VK_WHOLE_SIZE;

        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = set;
        writes[i].dstBinding      = static_cast<std::uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo     = &bufferInfos[i];
    }
    vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
    return set;
}

std::vector<float> ComputeApp::runSaxpy(float a, const std::vector<float>& x,
                                        const std::vector<float>& y) {
    if (x.size() != y.size()) throw std::runtime_error("saxpy: x 和 y 必须等长");
    if (x.empty()) return {};

    const auto         n     = static_cast<std::uint32_t>(x.size());
    const VkDeviceSize bytes = sizeof(float) * static_cast<VkDeviceSize>(n);

    // STORAGE_BUFFER 是 compute 读写它的凭证；TRANSFER_DST 是上传要的；
    // y 还要被回读，所以多一个 TRANSFER_SRC。
    ScopedBuffer bufX(*this, createBuffer(bytes,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          false, false));
    ScopedBuffer bufY(*this, createBuffer(bytes,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          false, false));
    uploadToBuffer(x.data(), bytes, bufX.get());
    uploadToBuffer(y.data(), bytes, bufY.get());

    ScopedPipeline pipe(*this,
                        createComputePipeline("saxpy.comp.spv", 2, sizeof(SaxpyPush)));
    const VkDescriptorSet set =
        allocateStorageSet(pipe.get(), {&bufX.get(), &bufY.get()});

    // 向上取整。n 不是 256 的整数倍时少一组，最后那几十个元素就没人算 ——
    // 而前面所有元素都是对的，所以肉眼看结果完全正常。
    const std::uint32_t groups = (n + kWorkgroupSize - 1) / kWorkgroupSize;

    SaxpyPush push;
    push.a = a;
    push.n = n;

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().layout, 0, 1,
                                &set, 0, nullptr);
        vkCmdPushConstants(cmd, pipe.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(push), &push);
        vkCmdDispatch(cmd, groups, 1, 1);

        // dispatch 写的是 shader write，接下来 readback 要用 transfer read 去读它。
        // 没有这道 barrier，两者之间没有任何顺序保证 —— 在这块卡上多半照样对，
        // 在别的卡上就不一定。sync validation 专门抓这个。
        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0,
                             nullptr);
    });

    return rwb::rhi::readbackBufferAs<float>(*m_ctx, bufY.handle(), n);
}

} // namespace p03
