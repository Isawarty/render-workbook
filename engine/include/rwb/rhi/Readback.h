#pragma once

#include "rwb/rhi/Context.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace rwb::rhi {

// 回读 —— 判分基建，不是教学点。
//
// L2 靠 readbackBuffer 把 compute 的结果拿回 CPU 与参考实现比对；
// L3 靠 readbackImage 把渲染结果拿回来与基准图比对。
//
// 两者都是「同步、慢、每次都新建 staging」的实现。教学期这是优点：
// 一次调用等于一次完整的 GPU 往返，没有任何隐藏的异步状态需要你推理。

struct CapturedImage {
    std::uint32_t             width  = 0;
    std::uint32_t             height = 0;
    std::vector<std::uint8_t> pixels;   // 恒为 RGBA8，每像素 4 字节
};

// 把一张 2D 图像的 mip 0 / layer 0 读回 CPU，并统一转成 RGBA8。
//
// currentLayout 是调用时该图像所处的 layout；函数结束后会把它转回去，
// 所以调用前后 layout 不变 —— 你可以在渲染循环中间插一次回读而不打乱状态机。
CapturedImage readbackImage(const Context& ctx,
                            VkImage        image,
                            VkFormat       format,
                            std::uint32_t  width,
                            std::uint32_t  height,
                            VkImageLayout  currentLayout);

// 把一个 device-local buffer 的前 size 字节读回 CPU。
std::vector<std::uint8_t> readbackBuffer(const Context& ctx, VkBuffer buffer, VkDeviceSize size);

// 便利封装：按元素类型取回。P3 的每一题都用它。
template <typename T>
std::vector<T> readbackBufferAs(const Context& ctx, VkBuffer buffer, std::size_t elementCount) {
    const std::vector<std::uint8_t> bytes =
        readbackBuffer(ctx, buffer, static_cast<VkDeviceSize>(sizeof(T) * elementCount));
    std::vector<T> out(elementCount);
    if (!bytes.empty()) std::memcpy(out.data(), bytes.data(), sizeof(T) * elementCount);
    return out;
}

} // namespace rwb::rhi
