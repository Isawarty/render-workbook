// t06 — command pool / command buffer / 录制
//
// 任务书: projects/p01-triangle/docs/t06-commands.md
// 判分:   ctest --preset win-msvc -R p01-t06
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::createCommandPool() {
    // TODO(p01-t06):
    //   建 m_commandPool，绑到 graphics 队列族。
    //   flags 要带 RESET_COMMAND_BUFFER_BIT —— 不然只能整池重置，每帧重录就没法做。
    RWB_TODO("p01-t06 TriangleApp::createCommandPool");
}

void TriangleApp::createCommandBuffers() {
    // TODO(p01-t06):
    //   按 m_framesInFlight 的数量分配 PRIMARY 级 command buffer -> m_commandBuffers。
    //   （现在是 1；t09 会把它调到 2，那时这个函数要能直接复用）
    RWB_TODO("p01-t06 TriangleApp::createCommandBuffers");
}

void TriangleApp::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    // TODO(p01-t06):
    //   录一帧:
    //     vkBeginCommandBuffer
    //     vkCmdBeginRenderPass   framebuffer 取 m_framebuffers[imageIndex]，
    //                            renderArea 用 m_swapchainExtent，给一个 clear color
    //     vkCmdBindPipeline
    //     vkCmdSetViewport / vkCmdSetScissor
    //                            ——  t05 里把它们声明成了动态状态，这里不设 validation 会报错
    //     vkCmdDraw(cb, 3, 1, 0, 0)
    //     vkCmdEndRenderPass
    //     vkEndCommandBuffer
    RWB_TODO("p01-t06 TriangleApp::recordCommandBuffer");
}

} // namespace p01
