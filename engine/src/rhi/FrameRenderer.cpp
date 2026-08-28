#include "rwb/rhi/FrameRenderer.h"

#include <utility>

namespace rwb::rhi {

FrameRenderer::FrameRenderer(Context& ctx, Swapchain& swapchain, std::uint32_t framesInFlight)
    : m_ctx(ctx), m_swapchain(swapchain), m_framesInFlight(framesInFlight) {
    if (m_framesInFlight == 0) m_framesInFlight = 1;
    createFrameResources();
    createImageResources();
}

FrameRenderer::~FrameRenderer() {
    m_ctx.waitIdle();
    destroyImageResources();
    destroyFrameResources();
}

void FrameRenderer::createFrameResources() {
    m_commandBuffers.resize(m_framesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_ctx.commandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = m_framesInFlight;
    VK_CHECK(vkAllocateCommandBuffers(m_ctx.device(), &allocInfo, m_commandBuffers.data()));

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // SIGNALED: 第一帧就 vkWaitForFences 而不用特判。这是最省事的初始化方式。
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    m_imageAvailable.resize(m_framesInFlight);
    m_inFlightFences.resize(m_framesInFlight);
    for (std::uint32_t i = 0; i < m_framesInFlight; ++i) {
        VK_CHECK(vkCreateSemaphore(m_ctx.device(), &semInfo, nullptr, &m_imageAvailable[i]));
        VK_CHECK(vkCreateFence(m_ctx.device(), &fenceInfo, nullptr, &m_inFlightFences[i]));
    }
    m_currentFrame = 0;
}

void FrameRenderer::destroyFrameResources() noexcept {
    VkDevice device = m_ctx.device();
    if (device == VK_NULL_HANDLE) return;

    if (!m_commandBuffers.empty()) {
        vkFreeCommandBuffers(device, m_ctx.commandPool(),
                             static_cast<std::uint32_t>(m_commandBuffers.size()),
                             m_commandBuffers.data());
        m_commandBuffers.clear();
    }
    for (VkSemaphore s : m_imageAvailable) vkDestroySemaphore(device, s, nullptr);
    for (VkFence f : m_inFlightFences)     vkDestroyFence(device, f, nullptr);
    m_imageAvailable.clear();
    m_inFlightFences.clear();
}

void FrameRenderer::createImageResources() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_renderFinished.resize(m_swapchain.imageCount());
    for (VkSemaphore& s : m_renderFinished) {
        VK_CHECK(vkCreateSemaphore(m_ctx.device(), &semInfo, nullptr, &s));
    }
    // VK_NULL_HANDLE = 这张图像还没被任何帧用过
    m_imagesInFlight.assign(m_swapchain.imageCount(), VK_NULL_HANDLE);
}

void FrameRenderer::destroyImageResources() noexcept {
    VkDevice device = m_ctx.device();
    if (device == VK_NULL_HANDLE) return;
    for (VkSemaphore s : m_renderFinished) vkDestroySemaphore(device, s, nullptr);
    m_renderFinished.clear();
    m_imagesInFlight.clear();
}

void FrameRenderer::setFramesInFlight(std::uint32_t n) {
    if (n == 0) n = 1;
    if (n == m_framesInFlight) return;
    m_ctx.waitIdle();
    destroyFrameResources();
    m_framesInFlight = n;
    createFrameResources();
}

void FrameRenderer::handleResize() {
    m_swapchain.recreate();
    m_ctx.waitIdle();
    destroyImageResources();
    createImageResources();
    if (m_onResize) m_onResize();
    m_resizeRequested = false;
}

void FrameRenderer::drawFrame(const RecordFn& record) {
    VkDevice device = m_ctx.device();

    // 等这个帧槽位上一次的工作做完，之后才能安全地重录它的 command buffer。
    VK_CHECK(vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));

    std::uint32_t imageIndex = 0;
    const VkResult acquireRes =
        vkAcquireNextImageKHR(device, m_swapchain.handle(), UINT64_MAX,
                              m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
        // 关键：这里「还没有」reset fence。
        // 如果在 acquire 之前就 vkResetFences，这条提前返回的路径会留下一个
        // 永远不会被 signal 的 fence，下一帧的 vkWaitForFences 直接死锁。
        // 这是 P1-t07 最容易踩的坑，也是那道题的测试专门在验的东西。
        handleResize();
        return;
    }
    if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(acquireRes);
    }

    // 这张图像可能还被「另一个」在飞帧占着（图像数和在飞帧数不一定相等）。
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        VK_CHECK(vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
    }
    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    VK_CHECK(vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]));

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    record(cmd, imageIndex, m_currentFrame);
    VK_CHECK(vkEndCommandBuffer(cmd));

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &m_imageAvailable[m_currentFrame];
    // 只在「往颜色附件写」这一步之前等。顶点处理可以先跑起来 ——
    // 这就是 Vulkan 让你显式写 stage mask 的收益。
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &m_renderFinished[imageIndex];
    VK_CHECK(vkQueueSubmit(m_ctx.graphicsQueue(), 1, &submit, m_inFlightFences[m_currentFrame]));

    VkSwapchainKHR swapchains[] = {m_swapchain.handle()};
    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &m_renderFinished[imageIndex];
    present.swapchainCount     = 1;
    present.pSwapchains        = swapchains;
    present.pImageIndices      = &imageIndex;

    const VkResult presentRes = vkQueuePresentKHR(m_ctx.presentQueue(), &present);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR ||
        m_resizeRequested) {
        handleResize();
    } else if (presentRes != VK_SUCCESS) {
        VK_CHECK(presentRes);
    }

    m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;
}

void FrameRenderer::run(int frameCount, const RecordFn& record) {
    int drawn = 0;
    Window* window = m_ctx.window();
    while (true) {
        if (window) {
            if (window->shouldClose()) break;
            window->pollEvents();
            if (window->consumeResizeFlag()) m_resizeRequested = true;
            if (window->isMinimized()) continue;
        }
        drawFrame(record);
        ++drawn;
        if (frameCount >= 0 && drawn >= frameCount) break;
    }
    m_ctx.waitIdle();
}

CapturedImage FrameRenderer::renderAndCapture(const RecordFn& record) {
    VkDevice device = m_ctx.device();

    // 不走 drawFrame：那条路径会 present，图像的归属就交回给显示引擎了。
    // 这里 acquire 之后「不 present」，图像一直归我们，回读的 layout 推理最简单。
    VK_CHECK(vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence acquireFence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &acquireFence));

    std::uint32_t imageIndex = 0;
    const VkResult acquireRes = vkAcquireNextImageKHR(device, m_swapchain.handle(), UINT64_MAX,
                                                      VK_NULL_HANDLE, acquireFence, &imageIndex);
    if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        vkDestroyFence(device, acquireFence, nullptr);
        VK_CHECK(acquireRes);
    }
    VK_CHECK(vkWaitForFences(device, 1, &acquireFence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(device, acquireFence, nullptr);

    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        VK_CHECK(vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
    }

    // 用 immediateSubmit 录用户的绘制命令并同步等完成。
    m_ctx.immediateSubmit([&](VkCommandBuffer cmd) {
        record(cmd, imageIndex, m_currentFrame);
    });

    // render pass 的 finalLayout 是 PRESENT_SRC_KHR，回读按这个 layout 进出。
    return readbackImage(m_ctx, m_swapchain.images()[imageIndex], m_swapchain.format(),
                         m_swapchain.extent().width, m_swapchain.extent().height,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

} // namespace rwb::rhi
