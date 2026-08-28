// t05 —— image view / sampler / mipmap 生成
//
// image 是显存里的一堆像素，view 是「怎么解读它」，sampler 是「怎么取样它」。
// 三者在 Vulkan 里是彻底分开的，因为它们的复用粒度不同：
// 一张图可以有多个 view（不同 mip 范围、不同 swizzle），
// 一个 sampler 可以给几百张图共用。
#include "../ResourceApp.h"

#include <stdexcept>

namespace p02 {

VkImageView ResourceApp::createImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspect, std::uint32_t mipLevels) {
    VkImageViewCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image    = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format   = format;
    info.subresourceRange.aspectMask     = aspect;
    info.subresourceRange.baseMipLevel   = 0;
    // 覆盖全部 mip 级。只写 1 的话采样器永远只看得到最高清那一级，
    // mip 链等于白生成 —— 而且画面看起来「只是有点闪」，很难联想到这里。
    info.subresourceRange.levelCount     = mipLevels;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(m_ctx->device(), &info, nullptr, &view));
    return view;
}

// 用 vkCmdBlitImage 从上一级缩一半到下一级，逐级递推。
//
// 为什么不在 CPU 上生成好再一次性上传？可以，而且离线管线通常就那么做。
// GPU 生成的价值在运行时动态图（渲染目标、程序化纹理）——
// 而且它逼你把「同一张图的不同 mip 级处于不同 layout」这件事想清楚。
void ResourceApp::generateMipmaps(VkCommandBuffer cmd, const Image& image) {
    // 不是所有格式都支持线性过滤的 blit。不查就用，在某些移动 GPU 上直接是错的。
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(m_ctx->physicalDevice(), image.format, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error(
            "这个格式不支持线性过滤的 blit，没法用 GPU 生成 mip 链。\n"
            "  真实项目里此时应当回退到「离线生成好再上传」。");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image               = image.handle;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;   // 每次只动一级

    auto mipWidth  = static_cast<std::int32_t>(image.width);
    auto mipHeight = static_cast<std::int32_t>(image.height);

    for (std::uint32_t i = 1; i < image.mipLevels; ++i) {
        // 第 i-1 级刚被写完（i==1 时是 staging 拷进来的，之后是上一轮 blit 写的），
        // 现在要拿它当 blit 的源，所以转成 TRANSFER_SRC。
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        // 每级减半，但不能减到 0 —— 非正方形纹理会先有一边到 1
        blit.dstOffsets[1] = {mipWidth  > 1 ? mipWidth  / 2 : 1,
                              mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        // 同一张 image 既当源又当目标 —— 合法，因为源和目标是不同的 mip 级，
        // 而且上面那条 barrier 已经把它们的 layout 和访问分开了。
        vkCmdBlitImage(cmd,
                       image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        // 第 i-1 级已经用完了，转成可采样。
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth  > 1) mipWidth  /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // 最后一级从来没当过 blit 的源，所以还停在 TRANSFER_DST，单独收尾。
    // 漏掉这一段是这道题最常见的错误：validation 会报最后一级 layout 不对。
    barrier.subresourceRange.baseMipLevel = image.mipLevels - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ResourceApp::createTextureImageView() {
    m_texture.view = createImageView(m_texture.handle, m_texture.format,
                                     VK_IMAGE_ASPECT_COLOR_BIT, m_texture.mipLevels);
}

void ResourceApp::createTextureSampler() {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // magFilter: 贴近了看（放大）。minFilter: 拉远了看（缩小）。
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;

    // uv 超出 [0,1] 时怎么办。地面那块四边形的 uv 到 8.0，靠 REPEAT 平铺。
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // 各向异性过滤：斜着看地面时，沿视线方向的采样比垂直方向密得多，
    // 各向同性的 mip 选择只能取两者里更粗的那一级，于是远处糊成一片。
    // 它是「可选特性」，必须查开没开 —— 没开就用是未定义行为。
    if (m_ctx->enabledFeatures().samplerAnisotropy) {
        info.anisotropyEnable = VK_TRUE;
        info.maxAnisotropy    = m_ctx->properties().limits.maxSamplerAnisotropy;
    } else {
        info.anisotropyEnable = VK_FALSE;
        info.maxAnisotropy    = 1.0f;
    }

    info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    // VK_FALSE = uv 归一化到 [0,1]。VK_TRUE 时用像素坐标，
    // 那要求 mip 只有一级、且不能 REPEAT，实际很少用。
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable           = VK_FALSE;   // 阴影贴图的 PCF 会用到，P4-t05
    info.compareOp               = VK_COMPARE_OP_ALWAYS;

    // mip 级之间也做线性插值（三线性过滤）。用 NEAREST 的话
    // 地面上会出现一条条能看见的「mip 分界线」。
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.minLod     = 0.0f;
    info.maxLod     = static_cast<float>(m_texture.mipLevels);

    VK_CHECK(vkCreateSampler(m_ctx->device(), &info, nullptr, &m_sampler));
}

} // namespace p02
