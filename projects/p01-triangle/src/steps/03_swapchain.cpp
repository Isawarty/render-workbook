// t03 — swapchain / image views
//
// OpenGL 的双缓冲是驱动替你管的，你只会调 SwapBuffers。
// Vulkan 把这件事完全摊开：你自己决定几张图、什么格式、什么呈现模式、
// 图像归谁用、什么时候可以往里画。代价是啰嗦，回报是可控。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace p01 {
namespace {

// 优先 8-bit sRGB。选 sRGB 意味着硬件在写入时替你做 gamma 编码，
// 你的 shader 就可以一直在线性空间里算 —— 这是正确做 PBR 的前提（P4 会依赖）。
VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    for (const VkSurfaceFormatKHR& f : available) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return available.front();   // 规范保证 formats 非空
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& available) {
    // FIFO 是唯一被规范保证一定支持的模式（等价于 VSync on），
    // 而且它不会空转 GPU —— 教学用它最省心，也最容易得到稳定的截图。
    (void)available;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps,
                        std::uint32_t desiredWidth, std::uint32_t desiredHeight) {
    // currentExtent 是 UINT32_MAX 时，表示窗口系统允许我们自选尺寸；
    // 否则必须照它给的用。Retina 屏上这两者差 2 倍，所以传进来的应当是
    // framebuffer 尺寸而不是窗口尺寸。
    if (caps.currentExtent.width != (std::numeric_limits<std::uint32_t>::max)()) {
        return caps.currentExtent;
    }
    VkExtent2D extent{desiredWidth, desiredHeight};
    extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

} // namespace

void TriangleApp::createSwapchain() {
    const SwapchainSupport support = querySwapchainSupport(m_physicalDevice);

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR   presentMode   = choosePresentMode(support.presentModes);

    std::uint32_t fbWidth = m_config.width, fbHeight = m_config.height;
    if (m_window) m_window->framebufferSize(fbWidth, fbHeight);
    const VkExtent2D extent = chooseExtent(support.capabilities, fbWidth, fbHeight);

    // 只要 minImageCount 张会导致：驱动可能正拿着全部图像，
    // 我们 acquire 时只能干等。多要一张来换取真正的并行余量。
    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = m_surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;      // 非 VR 场景恒为 1
    // COLOR_ATTACHMENT: 要往里画
    // TRANSFER_SRC:     要把它拷出来存成 PNG（L3 golden 测试用）
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    const std::uint32_t families[] = {m_queueFamilies.graphics.value(),
                                      m_queueFamilies.present.value()};
    if (families[0] != families[1]) {
        // 两个队列族都要碰这些图像。CONCURRENT 省事但有性能损失；
        // 正式项目通常用 EXCLUSIVE + 显式 ownership transfer。
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = families;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;   // 被遮挡的像素允许不渲染
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    const VkResult res = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(rwb::format("vkCreateSwapchainKHR 失败, VkResult = %d", res));
    }

    // 注意: 实际拿到的图像数可能多于 minImageCount，必须重新查询。
    std::uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, nullptr);
    m_swapchainImages.resize(actualCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, m_swapchainImages.data());

    m_swapchainFormat = surfaceFormat.format;
    m_swapchainExtent = extent;
    ++m_swapchainGeneration;
}

void TriangleApp::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (std::size_t i = 0; i < m_swapchainImages.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image    = m_swapchainImages[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format   = m_swapchainFormat;
        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(m_device, &info, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImageView 失败");
        }
    }
}

} // namespace p01
