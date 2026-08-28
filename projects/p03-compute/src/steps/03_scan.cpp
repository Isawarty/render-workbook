// t03 —— 分块 exclusive prefix sum
//
// 任务书: projects/p03-compute/docs/t03-scan.md
// 判分:   python rwb.py test p03-t03

#include "../ComputeApp.h"

#include "rwb/rhi/Readback.h"

#include <stdexcept>

namespace p03 {
namespace {

struct ScanPush {
    std::uint32_t n = 0;
};

void shaderBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);
}

} // namespace

std::vector<std::uint32_t> ComputeApp::runScan(const std::vector<std::uint32_t>& data) {
    if (data.empty()) return {};
    if (data.size() > kScanMaxElements) {
        throw std::runtime_error("scan: 元素数超过两级实现的上限 65536");
    }

    const std::uint32_t n      = static_cast<std::uint32_t>(data.size());
    const std::uint32_t blocks = (n + kScanBlockSize - 1) / kScanBlockSize;
    const VkDeviceSize  bytes  = sizeof(std::uint32_t) * static_cast<VkDeviceSize>(n);
    const VkDeviceSize blockBytes =
        sizeof(std::uint32_t) * static_cast<VkDeviceSize>(blocks);
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    ScopedBuffer input(*this, createBuffer(bytes, usage, false, false));
    ScopedBuffer output(*this, createBuffer(bytes, usage, false, false));
    ScopedBuffer blockSums(*this, createBuffer(blockBytes, usage, false, false));
    ScopedBuffer blockOffsets(*this, createBuffer(blockBytes, usage, false, false));
    // 第二趟 scan 仍会写一个总和；结果不参与第三趟，但 descriptor 必须指向合法 buffer。
    ScopedBuffer grandTotal(*this, createBuffer(sizeof(std::uint32_t), usage, false, false));
    uploadToBuffer(data.data(), bytes, input.get());

    ScopedPipeline scan(*this,
                        createComputePipeline("scan_block.comp.spv", 3, sizeof(ScanPush)));
    ScopedPipeline add(*this, createComputePipeline("scan_add_offsets.comp.spv", 2,
                                                    sizeof(ScanPush)));
    const VkDescriptorSet first =
        allocateStorageSet(scan.get(), {&input.get(), &output.get(), &blockSums.get()});
    const VkDescriptorSet second = allocateStorageSet(
        scan.get(), {&blockSums.get(), &blockOffsets.get(), &grandTotal.get()});
    const VkDescriptorSet third =
        allocateStorageSet(add.get(), {&output.get(), &blockOffsets.get()});

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scan.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scan.get().layout, 0, 1,
                                &first, 0, nullptr);
        const ScanPush firstPush{n};
        vkCmdPushConstants(cmd, scan.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(firstPush), &firstPush);
        vkCmdDispatch(cmd, blocks, 1, 1);
        shaderBarrier(cmd);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scan.get().layout, 0, 1,
                                &second, 0, nullptr);
        const ScanPush secondPush{blocks};
        vkCmdPushConstants(cmd, scan.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(secondPush), &secondPush);
        vkCmdDispatch(cmd, 1, 1, 1);
        shaderBarrier(cmd);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, add.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, add.get().layout, 0, 1,
                                &third, 0, nullptr);
        vkCmdPushConstants(cmd, add.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(firstPush), &firstPush);
        vkCmdDispatch(cmd, blocks, 1, 1);

        VkMemoryBarrier readback{};
        readback.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        readback.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        readback.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &readback, 0, nullptr, 0,
                             nullptr);
    });

    return rwb::rhi::readbackBufferAs<std::uint32_t>(*m_ctx, output.handle(), n);
}

} // namespace p03
