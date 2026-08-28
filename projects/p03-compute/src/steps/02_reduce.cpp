// t02 —— 并行归约 / subgroup 降级路径
//
// 任务书: projects/p03-compute/docs/t02-reduce.md
// 判分:   python rwb.py test p03-t02

#include "../ComputeApp.h"

#include "rwb/rhi/Readback.h"

#include <stdexcept>

namespace p03 {
namespace {

struct ReducePush {
    std::uint32_t n = 0;
};

} // namespace

ReducePath ComputeApp::choosePath(VkSubgroupFeatureFlags supportedOperations,
                                  VkShaderStageFlags     supportedStages,
                                  std::uint32_t          subgroupSize) {
    const bool compute = (supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
    const bool arithmetic =
        (supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0;
    // subgroupSums[64] 要容纳 256 / subgroupSize 个小组。
    return compute && arithmetic && subgroupSize >= 4 ? ReducePath::Subgroup
                                                       : ReducePath::Shared;
}

ReducePath ComputeApp::chooseReducePath() const {
    const auto& sub = m_ctx->subgroupProperties();
    return choosePath(sub.supportedOperations, sub.supportedStages, sub.subgroupSize);
}

float ComputeApp::runReduce(const std::vector<float>& data, ReducePath path) {
    if (data.empty()) return 0.0f;
    if (path == ReducePath::Subgroup && chooseReducePath() != ReducePath::Subgroup) {
        throw std::runtime_error("当前设备不支持 compute subgroup arithmetic");
    }

    const VkDeviceSize inputBytes = sizeof(float) * static_cast<VkDeviceSize>(data.size());
    const std::uint32_t maxGroups =
        (static_cast<std::uint32_t>(data.size()) + kWorkgroupSize - 1) / kWorkgroupSize;

    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ScopedBuffer ping(*this, createBuffer(inputBytes, usage, false, false));
    ScopedBuffer pong(*this,
                      createBuffer(sizeof(float) * static_cast<VkDeviceSize>(maxGroups), usage,
                                   false, false));
    uploadToBuffer(data.data(), inputBytes, ping.get());

    const char* shader =
        path == ReducePath::Subgroup ? "reduce_subgroup.comp.spv" : "reduce_shared.comp.spv";
    ScopedPipeline pipe(*this, createComputePipeline(shader, 2, sizeof(ReducePush)));

    Buffer*       input  = &ping.get();
    Buffer*       output = &pong.get();
    std::uint32_t count  = static_cast<std::uint32_t>(data.size());

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().pipeline);

        while (count > 1) {
            const std::uint32_t groups = (count + kWorkgroupSize - 1) / kWorkgroupSize;
            const VkDescriptorSet set = allocateStorageSet(pipe.get(), {input, output});
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().layout, 0,
                                    1, &set, 0, nullptr);

            const ReducePush push{count};
            vkCmdPushConstants(cmd, pipe.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(push), &push);
            vkCmdDispatch(cmd, groups, 1, 1);

            VkMemoryBarrier barrier{};
            barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = groups > 1 ? VK_ACCESS_SHADER_READ_BIT
                                                : VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 groups > 1 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                            : VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 1, &barrier, 0, nullptr, 0, nullptr);

            count = groups;
            std::swap(input, output);
        }
    });

    return rwb::rhi::readbackBufferAs<float>(*m_ctx, input->handle, 1).front();
}

} // namespace p03
