// t07 —— indirect dispatch
// 任务书: projects/p03-compute/docs/t07-indirect.md
// 判分:   python rwb.py test p03-t07

#include "../ComputeApp.h"
#include "rwb/rhi/Readback.h"

namespace p03 {
namespace {
struct PreparePush { std::uint32_t n = 0; };
struct ScalePush { std::uint32_t n = 0; float factor = 1.0f; };
} // namespace

std::vector<float> ComputeApp::runIndirectScale(const std::vector<float>& data, float factor) {
    if (data.empty()) { m_lastIndirect = {}; return {}; }
    const auto n = static_cast<std::uint32_t>(data.size());
    const VkDeviceSize bytes = sizeof(float) * static_cast<VkDeviceSize>(n);
    const VkBufferUsageFlags dataUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ScopedBuffer input(*this, createBuffer(bytes, dataUsage, false, false));
    ScopedBuffer output(*this, createBuffer(bytes, dataUsage, false, false));
    ScopedBuffer command(*this, createBuffer(sizeof(VkDispatchIndirectCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, false));
    uploadToBuffer(data.data(), bytes, input.get());

    ScopedPipeline prepare(*this, createComputePipeline(
        "indirect_prepare.comp.spv", 1, sizeof(PreparePush)));
    ScopedPipeline scale(*this, createComputePipeline(
        "indirect_scale.comp.spv", 2, sizeof(ScalePush)));
    const VkDescriptorSet prepareSet = allocateStorageSet(prepare.get(), {&command.get()});
    const VkDescriptorSet scaleSet = allocateStorageSet(scale.get(), {&input.get(), &output.get()});

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prepare.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prepare.get().layout,
                                0, 1, &prepareSet, 0, nullptr);
        const PreparePush preparePush{n};
        vkCmdPushConstants(cmd, prepare.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(preparePush), &preparePush);
        vkCmdDispatch(cmd, 1, 1, 1);

        VkMemoryBarrier indirectBarrier{};
        indirectBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        indirectBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        indirectBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1, &indirectBarrier,
                             0, nullptr, 0, nullptr);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scale.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scale.get().layout,
                                0, 1, &scaleSet, 0, nullptr);
        const ScalePush scalePush{n, factor};
        vkCmdPushConstants(cmd, scale.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(scalePush), &scalePush);
        vkCmdDispatchIndirect(cmd, command.handle(), 0);

        VkMemoryBarrier readback{};
        readback.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        readback.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        readback.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &readback,
                             0, nullptr, 0, nullptr);
    });

    m_lastIndirect = rwb::rhi::readbackBufferAs<VkDispatchIndirectCommand>(
        *m_ctx, command.handle(), 1).front();
    return rwb::rhi::readbackBufferAs<float>(*m_ctx, output.handle(), n);
}

} // namespace p03
