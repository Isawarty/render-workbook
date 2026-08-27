// t07 — semaphore / fence / 提交 / 呈现
//
// 这是 P1 里最难、也最值钱的一段。Vulkan 不替你做任何隐式同步：
//   semaphore : GPU 内部、队列之间的同步。CPU 看不见它的状态。
//   fence     : GPU 通知 CPU「我干完了」。可以 vkWaitForFences 阻塞等。
//
// 一帧的因果链是：
//   acquire 拿到图像索引 --(imageAvailable semaphore)--> GPU 开始画
//   GPU 画完 --(renderFinished semaphore)--> present 引擎开始显示
//   GPU 画完 --(inFlight fence)--> CPU 知道这一帧的资源可以复用了
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::createSyncObjects() {
    m_imageAvailableSemaphores.resize(m_framesInFlight);
    m_inFlightFences.resize(m_framesInFlight);

    // renderFinished 按 swapchain 图像数分配, 而不是按帧数。
    //
    // 原因：vkAcquireNextImageKHR 返回的 imageIndex 顺序是驱动决定的, 不保证轮转。
    // 如果按帧数分配, 有可能出现「present 还在等图像 A 上的 semaphore,
    // 而新一帧又把同一个 semaphore 拿去 signal 图像 B」的别名冲突。
    // 这个 bug 在很多教程里都存在, 开了 sync validation 就会被抓出来。
    m_renderFinishedSemaphores.resize(m_swapchainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // 初始就置为 signaled: 否则第一帧的 vkWaitForFences 会永久阻塞
    // （在等一个从没被提交过的 fence）。
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t i = 0; i < m_framesInFlight; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("创建 per-frame 同步对象失败");
        }
    }
    for (std::size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("创建 per-image 同步对象失败");
        }
    }
}

void TriangleApp::drawFrame() {
    // 1. 等这一帧槽位上一次的 GPU 工作干完, 否则会覆写还在用的 command buffer
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // 2. 取一张可画的图像。注意它是「立即返回」的：
    //    返回时图像未必真的可用, 只是承诺 imageAvailable semaphore 会在可用时被 signal。
    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        // swapchain 已经和窗口对不上了（t08 处理）。这一帧直接放弃, 不能继续用。
        recreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error(rwb::format("vkAcquireNextImageKHR 失败, VkResult = %d", acquireResult));
    }

    // 3. 只有确认要提交工作了才重置 fence。
    //    如果在 acquire 之前就重置, 上面那个 early return 会留下一个永远不被 signal 的
    //    fence, 下一帧直接死锁。这是个非常经典的坑。
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    // 4. 提交。waitStage 说的是「GPU 可以先跑到哪一步再等 semaphore」：
    //    顶点处理不碰颜色附件, 所以可以先跑; 到写颜色输出这一步才必须等图像就绪。
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_imageAvailableSemaphores[m_currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &m_commandBuffers[m_currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_renderFinishedSemaphores[imageIndex];

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit 失败");
    }

    // 5. 呈现。等的是 per-image 的 renderFinished, 不是 per-frame 的。
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores[imageIndex];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error(rwb::format("vkQueuePresentKHR 失败, VkResult = %d", presentResult));
    }

    // 6. 轮转到下一个帧槽位。m_framesInFlight == 1 时这行等价于什么都没做。
    m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;
}

} // namespace p01
