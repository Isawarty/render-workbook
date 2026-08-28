// t04 —— image 创建 / staging 上传 / layout transition
//
// 任务书: projects/p02-resources/docs/t04-texture.md
// 判分:   ctest --preset win-msvc -R p02-t04
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"
#include "rwb/rhi/VmaUsage.h"

#include <cstring>
#include <stdexcept>

namespace p02 {

Image ResourceApp::createImage(std::uint32_t width, std::uint32_t height,
                               std::uint32_t mipLevels, VkSampleCountFlagBits samples,
                               VkFormat format, VkImageTiling tiling,
                               VkImageUsageFlags usage) {
    // TODO(p02-t04):
    //   vmaCreateImage。VkImageCreateInfo 要填：
    //     imageType = 2D, extent = {w,h,1}, mipLevels, arrayLayers = 1,
    //     format, tiling, usage, samples, sharingMode = EXCLUSIVE
    //     initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    //       （规范只允许 UNDEFINED 或 PREINITIALIZED。从 UNDEFINED 转出去
    //         允许驱动丢弃旧内容，所以它是最快的起点）
    //   VmaAllocationCreateInfo 用 VMA_MEMORY_USAGE_AUTO 即可（图像不需要 CPU 访问）。
    //   记得把 format / width / height / mipLevels 填回返回的 Image。
    RWB_TODO("p02-t04 ResourceApp::createImage");
}

// 这个「不」是挖空题，直接给你 —— 理由同 destroyBuffer：它是 noexcept，
// 而 destroySizeDependent() 在任何阶段都会调它。挖空的话你会收到 abort 而不是报错。
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

void ResourceApp::transitionImageLayout(VkCommandBuffer cmd, const Image& image,
                                        VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkImageAspectFlags aspect) {
    // TODO(p02-t04):
    //   一条 VkImageMemoryBarrier + vkCmdPipelineBarrier。
    //
    //   barrier 同时表达三件事，三件都要写对：
    //     * 排布转换      oldLayout -> newLayout
    //     * 执行依赖      srcStage / dstStage
    //     * 内存可见性    srcAccessMask / dstAccessMask
    //   只写对第一件、access mask 填 0，多数时候画面看着是对的，
    //   直到换一块 GPU 才崩。开着 synchronization validation 就是为了当场抓住它。
    //
    //   subresourceRange: aspect 用传进来的，levelCount 用 image.mipLevels，layerCount = 1。
    //   srcQueueFamilyIndex / dstQueueFamilyIndex 都填 VK_QUEUE_FAMILY_IGNORED
    //   —— 填成真实索引反而会触发一次队列族所有权转移。
    //
    //   本课需要支持的三种转换（按 oldLayout/newLayout 分支）：
    //     UNDEFINED        -> TRANSFER_DST_OPTIMAL          （准备接收 staging 拷贝）
    //     TRANSFER_DST     -> SHADER_READ_ONLY_OPTIMAL      （拷完了，交给采样器）
    //     UNDEFINED        -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL （t06 的深度图会用）
    //   剩下的组合直接抛异常 —— 每加一种用法都显式补一条，正是 Vulkan 让你想清楚的地方。
    RWB_TODO("p02-t04 ResourceApp::transitionImageLayout");
}

void ResourceApp::copyBufferToImage(VkCommandBuffer cmd, const Buffer& src, const Image& dst) {
    // TODO(p02-t04):
    //   vkCmdCopyBufferToImage，目标 layout 传 TRANSFER_DST_OPTIMAL。
    //   VkBufferImageCopy: bufferRowLength / bufferImageHeight 填 0 表示紧密排列，
    //   imageSubresource.mipLevel = 0（只上传最高清那级，其余靠 t05 生成）。
    RWB_TODO("p02-t04 ResourceApp::copyBufferToImage");
}

void ResourceApp::createTextureImage() {
    // TODO(p02-t04):
    //   1. checkerboardPixels(kTextureSize) 拿到 RGBA8 像素（框架提供，不用你生成）
    //   2. 建一个 TRANSFER_SRC 的 staging buffer，memcpy 进去
    //   3. 算 mip 级数：floor(log2(max(w,h))) + 1。256 -> 9 级
    //   4. createImage(...) 建纹理。usage 要想全 —— 现在要 TRANSFER_DST 和 SAMPLED，
    //      而 t05 生成 mip 链时这张图还要当 blit 的源，所以 TRANSFER_SRC 也得现在就带上。
    //      format 用 VK_FORMAT_R8G8B8A8_SRGB（sRGB 才能让 shader 一直在线性空间算）
    //   5. immediateSubmit 里: transition(UNDEFINED -> TRANSFER_DST)
    //                          copyBufferToImage
    //                          然后分两种情况:
    //        stageIndex(m_reached) >= stageIndex(Stage::Sampler) 时调 generateMipmaps(cmd, m_texture)
    //          （它会自己把每一级转到 SHADER_READ_ONLY，所以这里不要再整体转一次）
    //        否则 transition(TRANSFER_DST -> SHADER_READ_ONLY)
    //   6. 销毁 staging buffer —— 而且要保证异常路径上也销毁。
    //      immediateSubmit 的 lambda 里一旦抛异常（这门课里最容易触发的就是
    //      「generateMipmaps 还没实现」），后面那句销毁会被跳过，staging 泄漏，
    //      程序退出时 VMA 会打一条「销毁内存块时还有分配没释放」的 error。
    //      换成任何一个真会失败的 Vulkan 调用，道理完全一样。
    RWB_TODO("p02-t04 ResourceApp::createTextureImage");
}

} // namespace p02
