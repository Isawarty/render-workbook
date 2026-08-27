// t09 — 多帧并行 (frames in flight)
//
// t07 结束时程序是「串行」的: CPU 提交一帧 -> 等 fence -> 再提交下一帧。
// GPU 在 CPU 录制期间是闲着的, CPU 在 GPU 渲染期间也是闲着的。
//
// frames in flight 的做法是准备 N 套 per-frame 资源
// (command buffer / imageAvailable semaphore / inFlight fence),
// 于是 CPU 可以在录制第 n+1 帧的同时, GPU 还在画第 n 帧。
//
// 为什么 N 通常取 2 而不是更大:
//   N 越大吞吐越好, 但输入延迟也越大（你的鼠标操作要等 N 帧才上屏）,
//   而且每多一帧就要多一整套资源。2 是绝大多数引擎的默认值。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::setFramesInFlight(std::uint32_t n) {
    if (n == 0) throw std::runtime_error("framesInFlight 不能是 0");
    if (n == m_framesInFlight) return;

    // 改的是「正在被 GPU 使用的资源的数量」, 必须先确保没有在飞的工作。
    vkDeviceWaitIdle(m_device);

    // --- 拆掉旧的 per-frame 资源 ---------------------------------------
    for (VkSemaphore s : m_imageAvailableSemaphores) vkDestroySemaphore(m_device, s, nullptr);
    for (VkFence f : m_inFlightFences)               vkDestroyFence(m_device, f, nullptr);
    m_imageAvailableSemaphores.clear();
    m_inFlightFences.clear();

    if (!m_commandBuffers.empty()) {
        vkFreeCommandBuffers(m_device, m_commandPool,
                             static_cast<std::uint32_t>(m_commandBuffers.size()),
                             m_commandBuffers.data());
        m_commandBuffers.clear();
    }

    // --- 按新的帧数重建 --------------------------------------------------
    m_framesInFlight = n;
    m_currentFrame   = 0;

    createCommandBuffers();   // 内部按 m_framesInFlight 分配

    m_imageAvailableSemaphores.resize(m_framesInFlight);
    m_inFlightFences.resize(m_framesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t i = 0; i < m_framesInFlight; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("重建 per-frame 同步对象失败");
        }
    }

    // 注意 renderFinished semaphore 不在这里动: 它是 per-image 的,
    // 数量由 swapchain 决定, 与 framesInFlight 无关。
    // 想清楚这两者为什么必须分开, 是这一题真正的考点。
    rwb::logInfo(rwb::format("frames in flight -> %u", m_framesInFlight));
}

} // namespace p01
