// t08 — swapchain 重建
//
// 任务书: projects/p01-triangle/docs/t08-resize.md
// 判分:   ctest --preset win-msvc -R p01-t08
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::cleanupSwapchain() {
    // TODO(p01-t08):
    //   按「与创建相反」的顺序销毁 framebuffers -> imageViews -> swapchain。
    //   注意 m_swapchainImages 里的 VkImage 归 swapchain 所有，不要自己销毁它们。
    //
    //   这个函数也会被析构路径调用，所以必须能在「什么都没建过」的状态下安全执行。
    RWB_TODO("p01-t08 TriangleApp::cleanupSwapchain");
}

void TriangleApp::recreateSwapchain() {
    // TODO(p01-t08):
    //   1. 窗口最小化时 framebuffer 是 0x0，建不出 swapchain —— 先轮询等它恢复
    //   2. vkDeviceWaitIdle（想清楚为什么这里必须整设备等待，而不是等某个 fence）
    //   3. cleanupSwapchain()
    //   4. createSwapchain / createImageViews / createFramebuffers
    //   5. renderFinished semaphore 是按 swapchain 图像数分配的，
    //      而图像数可能变了 —— 一并重建
    //
    //   不需要重建的: render pass、pipeline、command pool。
    //   想清楚为什么 —— 这正是 t05 把 viewport/scissor 设成动态状态换来的。
    RWB_TODO("p01-t08 TriangleApp::recreateSwapchain");
}

} // namespace p01
