#include "rwb/rhi/Readback.h"
#include "rwb/rhi/VmaUsage.h"

#include <algorithm>
#include <cstring>

namespace rwb::rhi {
namespace {

bool formatIsBgra(VkFormat f) {
    return f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_B8G8R8A8_UNORM ||
           f == VK_FORMAT_B8G8R8A8_SNORM;
}

// staging buffer 的 RAII 包装。只在本文件内用，所以不进公共头。
struct StagingBuffer {
    const Context* ctx        = nullptr;
    VkBuffer       buffer     = VK_NULL_HANDLE;
    VmaAllocation  allocation = VK_NULL_HANDLE;
    void*          mapped     = nullptr;

    StagingBuffer(const Context& c, VkDeviceSize size, VkBufferUsageFlags usage) : ctx(&c) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        // HOST_ACCESS_RANDOM + AUTO 让 VMA 自己挑一块 CPU 能映射的内存，
        // 我们不必手写 findMemoryType。MAPPED_BIT 表示建好就保持映射。
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(c.allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &info));
        mapped = info.pMappedData;
    }

    ~StagingBuffer() {
        if (buffer != VK_NULL_HANDLE) vmaDestroyBuffer(ctx->allocator(), buffer, allocation);
    }

    StagingBuffer(const StagingBuffer&)            = delete;
    StagingBuffer& operator=(const StagingBuffer&) = delete;
};

void imageBarrier(VkCommandBuffer cmd, VkImage image,
                  VkImageLayout oldLayout, VkImageLayout newLayout,
                  VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                  VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

CapturedImage readbackImage(const Context& ctx, VkImage image, VkFormat format,
                            std::uint32_t width, std::uint32_t height,
                            VkImageLayout currentLayout) {
    const VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4;
    StagingBuffer staging(ctx, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    ctx.immediateSubmit([&](VkCommandBuffer cmd) {
        imageBarrier(cmd, image, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;   // 0 = 紧密排列
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging.buffer, 1, &region);

        // 转回调用者交给我们时的 layout，让回读对外不可见。
        imageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currentLayout,
                     VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    });

    CapturedImage out;
    out.width  = width;
    out.height = height;
    out.pixels.resize(static_cast<std::size_t>(size));
    std::memcpy(out.pixels.data(), staging.mapped, static_cast<std::size_t>(size));

    if (formatIsBgra(format)) {
        for (std::size_t i = 0; i + 3 < out.pixels.size(); i += 4) {
            std::swap(out.pixels[i], out.pixels[i + 2]);
        }
    }
    // swapchain 的 alpha 通道不可靠（compositeAlpha 是 OPAQUE），强制不透明，
    // 否则基准图比对会在 alpha 上出现跨平台差异。
    for (std::size_t i = 3; i < out.pixels.size(); i += 4) out.pixels[i] = 255;

    return out;
}

std::vector<std::uint8_t> readbackBuffer(const Context& ctx, VkBuffer buffer, VkDeviceSize size) {
    if (size == 0) return {};
    StagingBuffer staging(ctx, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    ctx.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, buffer, staging.buffer, 1, &region);

        // 保证 copy 的写入对 host 可见。immediateSubmit 里的 fence 只保证
        // 「GPU 做完了」，不保证「写入已经对 CPU 可见」—— 这是两件事。
        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
    });

    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    std::memcpy(out.data(), staging.mapped, out.size());
    return out;
}

} // namespace rwb::rhi
