#include "rwb/rhi/Swapchain.h"

#include <algorithm>
#include <limits>

namespace rwb::rhi {
namespace {

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    // 优先 8-bit sRGB：硬件在写入时替你做 gamma 编码，shader 就能一直在线性空间算。
    // 这是 P4 正确做 PBR 的前提。
    for (const VkSurfaceFormatKHR& f : available) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return available.front();   // 规范保证 formats 非空
}

VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps,
                        std::uint32_t desiredWidth, std::uint32_t desiredHeight) {
    if (caps.currentExtent.width != (std::numeric_limits<std::uint32_t>::max)()) {
        return caps.currentExtent;
    }
    VkExtent2D extent{desiredWidth, desiredHeight};
    extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

} // namespace

Swapchain::Swapchain(Context& ctx, VkPresentModeKHR preferredPresentMode)
    : m_ctx(ctx), m_preferredPresentMode(preferredPresentMode) {
    if (ctx.surface() == VK_NULL_HANDLE) {
        throw std::runtime_error("headless Context 没有 surface，建不了 swapchain");
    }
    create();
}

Swapchain::~Swapchain() { destroy(); }

void Swapchain::create() {
    VkPhysicalDevice phys    = m_ctx.physicalDevice();
    VkSurfaceKHR     surface = m_ctx.surface();

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps));

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());

    std::uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(modes.begin(), modes.end(), m_preferredPresentMode) != modes.end()) {
        presentMode = m_preferredPresentMode;
    }

    std::uint32_t fbWidth = m_ctx.config().width, fbHeight = m_ctx.config().height;
    if (m_ctx.window()) m_ctx.window()->framebufferSize(fbWidth, fbHeight);
    const VkExtent2D extent = chooseExtent(caps, fbWidth, fbHeight);

    std::uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = surfaceFormat.format;
    info.imageColorSpace  = surfaceFormat.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    // TRANSFER_SRC 是给 L3 截图用的；TRANSFER_DST 让后处理链能直接 blit 进来。
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT     |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const QueueFamilies& qf = m_ctx.queueFamilies();
    const std::uint32_t families[] = {qf.graphics.value(), qf.present.value()};
    if (families[0] != families[1]) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = presentMode;
    info.clipped        = VK_TRUE;
    // 把旧的交给驱动，让它复用底层显示资源。旧句柄在 vkCreateSwapchainKHR 返回后
    // 就进入 retired 状态，但「必须」由我们自己销毁 —— 见下面 create() 的收尾。
    info.oldSwapchain   = m_swapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(m_ctx.device(), &info, nullptr, &newSwapchain));

    // 旧的 image view 和 swapchain 到这里才能销毁。
    for (VkImageView v : m_imageViews) vkDestroyImageView(m_ctx.device(), v, nullptr);
    m_imageViews.clear();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_ctx.device(), m_swapchain, nullptr);
    }

    m_swapchain = newSwapchain;
    m_format    = surfaceFormat.format;
    m_extent    = extent;

    // 实际拿到的图像数可能多于请求数，必须重新查询。
    std::uint32_t actual = 0;
    vkGetSwapchainImagesKHR(m_ctx.device(), m_swapchain, &actual, nullptr);
    m_images.resize(actual);
    vkGetSwapchainImagesKHR(m_ctx.device(), m_swapchain, &actual, m_images.data());

    m_imageViews.resize(m_images.size());
    for (std::size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image    = m_images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format   = m_format;
        vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(m_ctx.device(), &vi, nullptr, &m_imageViews[i]));
    }

    ++m_generation;
}

void Swapchain::recreate() {
    // 最小化时 framebuffer 是 0x0，此时建 swapchain 非法。等窗口回来。
    if (m_ctx.window()) {
        while (m_ctx.window()->isMinimized() && !m_ctx.window()->shouldClose()) {
            m_ctx.window()->pollEvents();
        }
    }
    m_ctx.waitIdle();
    create();
}

void Swapchain::destroy() noexcept {
    VkDevice device = m_ctx.device();
    if (device == VK_NULL_HANDLE) return;

    for (VkImageView v : m_imageViews) vkDestroyImageView(device, v, nullptr);
    m_imageViews.clear();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_images.clear();
}

} // namespace rwb::rhi
