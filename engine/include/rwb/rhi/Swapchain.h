#pragma once

#include "rwb/rhi/Context.h"

#include <cstdint>
#include <vector>

namespace rwb::rhi {

// ---------------------------------------------------------------------------
// Swapchain —— P1-t03 + t08 的成品。
//
// 相对你在 P1 写的那份，多做了一件事：recreate() 会把旧 swapchain 作为
// oldSwapchain 传进去。P1 里为了少一个概念直接传了 VK_NULL_HANDLE，
// 代价是重建时必须先 vkDeviceWaitIdle、且驱动没法复用旧的显示资源。
// ---------------------------------------------------------------------------
class Swapchain {
public:
    // preferredPresentMode 找不到时自动回退到 FIFO（唯一保证支持的模式）。
    explicit Swapchain(Context& ctx,
                       VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR);
    ~Swapchain();

    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // 窗口尺寸变了、或 acquire/present 返回 OUT_OF_DATE 时调用。
    // 窗口最小化（framebuffer 为 0x0）时会阻塞等到窗口恢复。
    void recreate();

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkFormat       format() const { return m_format; }
    VkExtent2D     extent() const { return m_extent; }

    const std::vector<VkImage>&     images()     const { return m_images; }
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }
    std::uint32_t imageCount() const { return static_cast<std::uint32_t>(m_images.size()); }

    // 每重建一次 +1。测试用它验证「确实走过了重建路径」。
    std::uint32_t generation() const { return m_generation; }

private:
    void create();
    void destroy() noexcept;

    Context&         m_ctx;
    VkPresentModeKHR m_preferredPresentMode;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat       m_format    = VK_FORMAT_UNDEFINED;
    VkExtent2D     m_extent{};

    std::vector<VkImage>     m_images;
    std::vector<VkImageView> m_imageViews;
    std::uint32_t            m_generation = 0;
};

} // namespace rwb::rhi
