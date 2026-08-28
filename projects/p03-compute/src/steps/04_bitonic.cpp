// t04 —— bitonic sort
//
// 任务书: projects/p03-compute/docs/t04-bitonic.md
// 判分:   python rwb.py test p03-t04

#include "../ComputeApp.h"

#include "rwb/rhi/Readback.h"

#include <algorithm>
#include <limits>

namespace p03 {
namespace {

struct BitonicPush {
    std::uint32_t n = 0;
    std::uint32_t k = 0;
    std::uint32_t j = 0;
};

std::uint32_t nextPowerOfTwo(std::uint32_t n) {
    std::uint32_t result = 1;
    while (result < n) result <<= 1u;
    return result;
}

} // namespace

std::vector<std::uint32_t> ComputeApp::runBitonicSort(
    const std::vector<std::uint32_t>& data) {
    if (data.empty()) return {};

    const std::uint32_t originalCount = static_cast<std::uint32_t>(data.size());
    const std::uint32_t paddedCount   = nextPowerOfTwo(originalCount);
    std::vector<std::uint32_t> padded(paddedCount, std::numeric_limits<std::uint32_t>::max());
    std::copy(data.begin(), data.end(), padded.begin());

    const VkDeviceSize bytes =
        sizeof(std::uint32_t) * static_cast<VkDeviceSize>(paddedCount);
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ScopedBuffer buffer(*this, createBuffer(bytes, usage, false, false));
    uploadToBuffer(padded.data(), bytes, buffer.get());

    ScopedPipeline pipe(*this,
                        createComputePipeline("bitonic.comp.spv", 1, sizeof(BitonicPush)));
    const VkDescriptorSet set = allocateStorageSet(pipe.get(), {&buffer.get()});
    const std::uint32_t groups = (paddedCount + kWorkgroupSize - 1) / kWorkgroupSize;

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.get().layout, 0, 1,
                                &set, 0, nullptr);

        for (std::uint32_t k = 2; k <= paddedCount; k <<= 1u) {
            for (std::uint32_t j = k >> 1u; j > 0; j >>= 1u) {
                const BitonicPush push{paddedCount, k, j};
                vkCmdPushConstants(cmd, pipe.get().layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(push), &push);
                vkCmdDispatch(cmd, groups, 1, 1);

                VkMemoryBarrier barrier{};
                barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = (k == paddedCount && j == 1)
                                            ? VK_ACCESS_TRANSFER_READ_BIT
                                            : VK_ACCESS_SHADER_READ_BIT |
                                                  VK_ACCESS_SHADER_WRITE_BIT;
                vkCmdPipelineBarrier(
                    cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    (k == paddedCount && j == 1) ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                                 : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
            }
        }
    });

    auto sorted = rwb::rhi::readbackBufferAs<std::uint32_t>(*m_ctx, buffer.handle(), paddedCount);
    sorted.resize(originalCount);
    return sorted;
}

} // namespace p03
