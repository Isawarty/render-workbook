// t07 — semaphore / fence / 提交 / 呈现
//
// P1 最难的一题。任务书: projects/p01-triangle/docs/t07-sync.md
// 判分:   ctest --preset win-msvc -R p01-t07
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::createSyncObjects() {
    // TODO(p01-t07):
    //   建三组同步对象:
    //     m_imageAvailableSemaphores  数量 = m_framesInFlight
    //     m_inFlightFences            数量 = m_framesInFlight，初始必须是 SIGNALED
    //                                 （否则第一帧的 vkWaitForFences 永久阻塞）
    //     m_renderFinishedSemaphores  数量 = swapchain 图像数，「不是」帧数
    //
    //   最后一条是本题的核心考点。先自己想清楚为什么，再看任务书里的解释。
    RWB_TODO("p01-t07 TriangleApp::createSyncObjects");
}

void TriangleApp::drawFrame() {
    // TODO(p01-t07):
    //   一帧的完整流程:
    //     1. vkWaitForFences 等本帧槽位上一轮的 GPU 工作结束
    //     2. vkAcquireNextImageKHR 取图像索引，signal imageAvailable[m_currentFrame]
    //        - 返回 VK_ERROR_OUT_OF_DATE_KHR 时要重建 swapchain 并「直接 return」
    //     3. vkResetFences —— 位置很关键，想清楚放在 acquire 之前还是之后。
    //        放错了会在 resize 时死锁，而且很难查。
    //     4. 重置并重录 command buffer
    //     5. vkQueueSubmit:
    //          wait   = imageAvailable[m_currentFrame]
    //          waitDstStageMask = COLOR_ATTACHMENT_OUTPUT
    //                             （为什么不是 TOP_OF_PIPE? 想清楚）
    //          signal = renderFinished[imageIndex]
    //          fence  = inFlight[m_currentFrame]
    //     6. vkQueuePresentKHR，等 renderFinished[imageIndex]
    //        - OUT_OF_DATE / SUBOPTIMAL / m_framebufferResized 时重建 swapchain
    //     7. m_currentFrame = (m_currentFrame + 1) % m_framesInFlight
    RWB_TODO("p01-t07 TriangleApp::drawFrame");
}

} // namespace p01
