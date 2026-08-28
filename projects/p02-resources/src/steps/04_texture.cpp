// t04 —— image 创建 / staging 上传 / layout transition
//
// buffer 只有「一段字节」，image 多了两件事：
//   1. 平铺方式（tiling）—— 驱动为了缓存局部性会把像素重排，具体怎么排是私有的
//   2. layout —— 同一块显存在「被采样」「被写入」「被拷贝」时的最优排布不同
//
// layout transition 是 Vulkan 里最容易写错、也最容易被 validation 抓住的一环。
#include "../ResourceApp.h"
#include "rwb/rhi/VmaUsage.h"

#include <cstring>
#include <stdexcept>

namespace p02 {

Image ResourceApp::createImage(std::uint32_t width, std::uint32_t height,
                               std::uint32_t mipLevels, VkSampleCountFlagBits samples,
                               VkFormat format, VkImageTiling tiling,
                               VkImageUsageFlags usage) {
    Image image;
    image.format    = format;
    image.width     = width;
    image.height    = height;
    image.mipLevels = mipLevels;

    VkImageCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.extent        = {width, height, 1};
    info.mipLevels     = mipLevels;
    info.arrayLayers   = 1;
    info.format        = format;
    // OPTIMAL: 让驱动按自己的方式重排像素，采样最快，但 CPU 不能直接读写。
    // LINEAR:  行优先、CPU 可读，但采样慢，而且很多格式压根不支持。
    // 所以标准路径永远是 OPTIMAL + staging 上传。
    info.tiling        = tiling;
    // UNDEFINED: 「我不关心里面原来是什么」。第一次 transition 一定从它出发 ——
    // 从 UNDEFINED 转出去允许驱动丢弃旧内容，这是最快的路径。
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage         = usage;
    info.samples       = samples;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(vmaCreateImage(m_ctx->allocator(), &info, &alloc,
                            &image.handle, &image.allocation, nullptr));
    return image;
}

void ResourceApp::destroyImage(Image& image) noexcept {
    if (!m_ctx) { image = Image{}; return; }
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_ctx->device(), image.view, nullptr);
    }
    if (image.handle != VK_NULL_HANDLE) {
        vmaDestroyImage(m_ctx->allocator(), image.handle, image.allocation);
    }
    image = Image{};
}

// layout transition 就是一条 image memory barrier。
//
// 它同时表达三件事：
//   * 排布转换（oldLayout -> newLayout）
//   * 执行依赖（srcStage 之前的活儿，要排在 dstStage 之后的活儿之前）
//   * 内存可见性（srcAccess 的写入，要对 dstAccess 的读取可见）
//
// 只写对第一件事、access mask 填 0，多数时候画面看起来是对的 ——
// 直到某天换一块 GPU 才崩。开 synchronization validation 就是为了当场抓住它。
void ResourceApp::transitionImageLayout(VkCommandBuffer cmd, const Image& image,
                                        VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkImageAspectFlags aspect) {
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    // 不做队列族所有权转移时，两边都必须是 IGNORED。
    // 填成实际的队列族索引反而会触发一次真正的 ownership transfer。
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image.handle;
    barrier.subresourceRange.aspectMask     = aspect;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = image.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // 还没人写过它，不需要等任何东西；只要在传输开始前完成转换。
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // 传输的写入必须对片元着色器的采样可见。
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
               newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        // 深度测试在 EARLY_FRAGMENT_TESTS 就开始了，不是在片元着色器里
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    } else {
        throw std::runtime_error(
            rwb::format("没处理过的 layout 转换: %d -> %d。"
                        "每加一种用法都要显式补一条 —— 这正是 Vulkan 让你想清楚的地方。",
                        static_cast<int>(oldLayout), static_cast<int>(newLayout)));
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ResourceApp::copyBufferToImage(VkCommandBuffer cmd, const Buffer& src, const Image& dst) {
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;   // 0 = 按 imageExtent 紧密排列
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;   // 只上传 mip 0，其余靠 t05 生成
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {dst.width, dst.height, 1};

    vkCmdCopyBufferToImage(cmd, src.handle, dst.handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void ResourceApp::createTextureImage() {
    const std::vector<std::uint8_t> pixels = checkerboardPixels(kTextureSize);
    const VkDeviceSize size = pixels.size();

    Buffer staging = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  /*hostVisible=*/true, /*persistentlyMapped=*/true);
    std::memcpy(staging.mapped, pixels.data(), static_cast<std::size_t>(size));

    // mip 级数 = floor(log2(max(w,h))) + 1。256 -> 9 级。
    // t05 会把 1..8 级填上；这一题只管 mip 0。
    std::uint32_t mipLevels = 1;
    for (std::uint32_t s = kTextureSize; s > 1; s /= 2) ++mipLevels;

    // TRANSFER_DST: staging 拷进来
    // TRANSFER_SRC: t05 生成 mip 链时要从上一级 blit 到下一级，源也是它自己
    // SAMPLED:      shader 要采样它
    m_texture = createImage(kTextureSize, kTextureSize, mipLevels, VK_SAMPLE_COUNT_1_BIT,
                            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

    const bool willGenerateMips = stageIndex(m_reached) >= stageIndex(Stage::Sampler);

    // try/catch 不是洁癖：immediateSubmit 的 lambda 里一旦抛异常，
    // 后面那句 destroyBuffer 就被跳过了，staging 泄漏，
    // 程序退出时 VMA 会报「销毁内存块时还有分配没释放」。
    // 这门课里最容易触发它的是「generateMipmaps 还没实现」，
    // 但换成任何一个真会失败的 Vulkan 调用，道理完全一样。
    try {
        m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
            transitionImageLayout(cmd, m_texture, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_ASPECT_COLOR_BIT);
            copyBufferToImage(cmd, staging, m_texture);

            if (willGenerateMips) {
                // t05 的 generateMipmaps 会自己把每一级转到 SHADER_READ_ONLY，
                // 所以这里不做整体转换，直接把图交给它。
                generateMipmaps(cmd, m_texture);
            } else {
                // t04 阶段还没有 mip 链，整张图直接转成可采样状态。
                transitionImageLayout(cmd, m_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_IMAGE_ASPECT_COLOR_BIT);
            }
        });
    } catch (...) {
        destroyBuffer(staging);
        throw;
    }
    destroyBuffer(staging);
}

} // namespace p02
