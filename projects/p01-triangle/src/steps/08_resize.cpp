// t08 — swapchain 重建
//
// 窗口一变大小, swapchain 里的图像尺寸就不再匹配, 驱动会开始返回
// VK_ERROR_OUT_OF_DATE_KHR。OpenGL 里这件事完全由驱动兜住,
// Vulkan 里你必须自己把整条依赖 swapchain 的资源链重建一遍。
//
// 依赖链: swapchain -> imageViews -> framebuffers
// 不依赖 swapchain 的: render pass（只依赖格式）、pipeline（视口是动态状态）、
//                      command pool。所以这些不用重建 —— 这也是 t05 里把
//                      viewport/scissor 设成动态状态的回报。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::cleanupSwapchain() {
    for (VkFramebuffer fb : m_framebuffers) {
        vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_framebuffers.clear();

    for (VkImageView view : m_swapchainImageViews) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    // m_swapchainImages 里的 VkImage 归 swapchain 所有, 不要自己销毁
    m_swapchainImages.clear();
}

void TriangleApp::recreateSwapchain() {
    // 最小化时 framebuffer 是 0x0, 这种尺寸建不出 swapchain。
    // 一直轮询到窗口恢复为止 —— 这是少数几个「阻塞等待用户」是正确做法的地方。
    if (m_window) {
        while (m_window->isMinimized()) {
            if (m_window->shouldClose()) return;
            m_window->pollEvents();
        }
    }

    // 必须等 GPU 完全空闲。任何还在飞的命令都可能正在引用即将被销毁的
    // framebuffer / image view, 提前销毁就是 use-after-free。
    vkDeviceWaitIdle(m_device);

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createFramebuffers();

    // swapchain 图像数量有可能变化（比如换了显示器刷新率或 present 模式）,
    // 而 renderFinished semaphore 是按图像数分配的, 所以要跟着重建。
    for (VkSemaphore s : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_device, s, nullptr);
    }
    m_renderFinishedSemaphores.assign(m_swapchainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (VkSemaphore& s : m_renderFinishedSemaphores) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &s) != VK_SUCCESS) {
            throw std::runtime_error("重建 renderFinished semaphore 失败");
        }
    }

    rwb::logInfo(rwb::format("swapchain 已重建 -> %ux%u (第 %u 代)",
                                   m_swapchainExtent.width, m_swapchainExtent.height,
                                   m_swapchainGeneration));
}

} // namespace p01
