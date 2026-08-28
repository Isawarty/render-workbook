// t05 —— image view / sampler / mipmap 生成
//
// 任务书: projects/p02-resources/docs/t05-sampler.md
// 判分:   ctest --preset win-msvc -R p02-t05
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"

#include <stdexcept>

namespace p02 {

VkImageView ResourceApp::createImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspect, std::uint32_t mipLevels) {
    // TODO(p02-t05):
    //   vkCreateImageView，viewType = 2D，layerCount = 1。
    //   subresourceRange.levelCount 必须是传进来的 mipLevels 而不是写死 1 ——
    //   写 1 的话采样器永远只看得到最清晰那级，mip 链等于白生成，
    //   而画面上只表现为「有点闪」，很难联想到这里。
    //
    //   这个函数 t06 的深度图和 t08 的多采样附件也会复用，所以 aspect 是参数。
    RWB_TODO("p02-t05 ResourceApp::createImageView");
}

void ResourceApp::generateMipmaps(VkCommandBuffer cmd, const Image& image) {
    // TODO(p02-t05):
    //   用 vkCmdBlitImage 从第 i-1 级缩一半到第 i 级，逐级递推。
    //
    //   0. 先查 vkGetPhysicalDeviceFormatProperties: optimalTilingFeatures 里
    //      必须有 VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT。
    //      没有就抛异常（真实项目此时应回退到「离线生成好再上传」）。
    //
    //   1. for i in 1..mipLevels-1:
    //        barrier: 第 i-1 级 TRANSFER_DST -> TRANSFER_SRC
    //                 （srcAccess = TRANSFER_WRITE, dstAccess = TRANSFER_READ，
    //                   两边 stage 都是 TRANSFER）
    //        vkCmdBlitImage: src = 第 i-1 级、layout TRANSFER_SRC
    //                        dst = 第 i 级、  layout TRANSFER_DST
    //                        filter = VK_FILTER_LINEAR
    //                        尺寸每级减半，但不能减到 0（非正方形纹理会先有一边到 1）
    //        barrier: 第 i-1 级 TRANSFER_SRC -> SHADER_READ_ONLY
    //
    //   2. 循环结束后还有一件事没做完 —— 最后一级从来没当过 blit 的源。
    //      漏掉它是这道题最常见的错误，validation 会报最后一级 layout 不对。
    //
    //   注意 barrier 的 subresourceRange.levelCount 要是 1（每次只动一级），
    //   baseMipLevel 每轮都要更新。
    RWB_TODO("p02-t05 ResourceApp::generateMipmaps");
}

void ResourceApp::createTextureImageView() {
    // TODO(p02-t05):
    //   用 createImageView 给 m_texture 建视图，存回 m_texture.view。
    //   aspect 是 COLOR，mipLevels 用 m_texture.mipLevels。
    RWB_TODO("p02-t05 ResourceApp::createTextureImageView");
}

void ResourceApp::createTextureSampler() {
    // TODO(p02-t05):
    //   vkCreateSampler。要点：
    //     magFilter / minFilter = LINEAR
    //     addressMode U/V/W    = REPEAT（地面那块四边形的 uv 到 8.0，靠它平铺）
    //     mipmapMode           = LINEAR（用 NEAREST 会在地面上留下能看见的 mip 分界线）
    //     minLod = 0, maxLod = m_texture.mipLevels
    //     unnormalizedCoordinates = VK_FALSE
    //     compareEnable = VK_FALSE（阴影贴图的 PCF 才用得上，P4-t05）
    //
    //   各向异性过滤是「可选特性」：
    //     必须先查 m_ctx->enabledFeatures().samplerAnisotropy，开了才能置 anisotropyEnable。
    //     没开就用是未定义行为 —— validation 会抓住你，而那已经是 L1 判失败了。
    //     maxAnisotropy 用 m_ctx->properties().limits.maxSamplerAnisotropy。
    RWB_TODO("p02-t05 ResourceApp::createTextureSampler");
}

} // namespace p02
