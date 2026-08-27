// t06 — command pool / command buffer / 录制
//
// OpenGL 的 draw call 是「立即」的（至少看起来是）。
// Vulkan 里你先把一串命令录进 command buffer，之后再整体提交给队列。
// 好处：录制可以多线程并行，提交是单线程的廉价操作；
//       而且录制期间驱动不需要做任何状态校验（都在建 pipeline 时做完了）。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::createCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // RESET_COMMAND_BUFFER: 允许单独重置某一个 command buffer 再重录。
    // 不加这个 flag 的话只能整池 vkResetCommandPool —— 每帧重录时就不够用了。
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // command pool 绑定到某个队列族：从它分配的 buffer 只能提交给那个族的队列。
    info.queueFamilyIndex = m_queueFamilies.graphics.value();

    if (vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool 失败");
    }
}

void TriangleApp::createCommandBuffers() {
    // 每个「在飞的帧」一个 command buffer。
    // t07 之前 m_framesInFlight == 1（串行）；t09 会把它调到 2。
    m_commandBuffers.resize(m_framesInFlight);

    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool        = m_commandPool;
    // PRIMARY   : 可以直接提交给队列
    // SECONDARY : 只能被 primary 通过 vkCmdExecuteCommands 调用（多线程录制时用）
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = static_cast<std::uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &info, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateCommandBuffers 失败");
    }
}

void TriangleApp::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;   // ONE_TIME_SUBMIT 也可以, 但我们每帧都重录, 差别不大

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("vkBeginCommandBuffer 失败");
    }

    // clear 值的顺序必须和 render pass 的 pAttachments 一一对应
    VkClearValue clearColor{};
    clearColor.color = {{0.02f, 0.03f, 0.06f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = m_renderPass;
    renderPassInfo.framebuffer       = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount   = 1;
    renderPassInfo.pClearValues      = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    // 这两个在建 pipeline 时声明成了动态状态, 所以必须在这里显式设置。
    // 漏掉的话 validation layer 会直接报错。
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchainExtent.width);
    viewport.height   = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // 3 个顶点, 1 个实例, 无 vertex buffer —— 顶点数据在 shader 里硬编码
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("vkEndCommandBuffer 失败");
    }
}

} // namespace p01
