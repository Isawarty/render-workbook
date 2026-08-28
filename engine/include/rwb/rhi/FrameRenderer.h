#pragma once

#include "rwb/rhi/Context.h"
#include "rwb/rhi/Readback.h"
#include "rwb/rhi/Swapchain.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace rwb::rhi {

// ---------------------------------------------------------------------------
// FrameRenderer —— P1 t06/t07/t08/t09 的成品：
// 每帧的命令缓冲、同步对象、acquire/submit/present、swapchain 重建、多帧并行。
//
// 你只需要提供一个「往 command buffer 里录什么」的回调。
//
// ## 一处相对 P1 的重要修正
//
// P1 里 renderFinished 信号量是「每个在飞帧一个」。这在多数驱动上能跑，
// 但严格来说是错的：present 什么时候真正完成不受我们的 fence 控制，
// 于是可能出现「上一次 present 还在等这个信号量，我们又拿它去 signal」。
// 开了 synchronization validation 之后这会被抓出来。
//
// 正确做法是 renderFinished「每张 swapchain 图像一个」——
// 因为它的生命周期跟的是图像，不是帧槽位。这里就是这么写的。
// ---------------------------------------------------------------------------
class FrameRenderer {
public:
    // imageIndex: 这一帧画到第几张 swapchain 图像（选 framebuffer 用）
    // frameIndex: 第几个在飞帧槽位（选 per-frame 的 UBO / descriptor set 用）
    using RecordFn = std::function<void(VkCommandBuffer cmd,
                                        std::uint32_t   imageIndex,
                                        std::uint32_t   frameIndex)>;

    // swapchain 重建之后调用，通知你重建依赖 extent 的资源
    // （framebuffer、深度图、G-Buffer……）。
    using ResizeFn = std::function<void()>;

    FrameRenderer(Context& ctx, Swapchain& swapchain, std::uint32_t framesInFlight = 2);
    ~FrameRenderer();

    FrameRenderer(const FrameRenderer&)            = delete;
    FrameRenderer& operator=(const FrameRenderer&) = delete;

    void setResizeCallback(ResizeFn fn) { m_onResize = std::move(fn); }

    // 画一帧。内部处理 acquire 失败 / OUT_OF_DATE / 窗口 resize。
    void drawFrame(const RecordFn& record);

    // 循环画。frameCount < 0 表示一直画到窗口关闭。
    void run(int frameCount, const RecordFn& record);

    // 画一帧并把结果读回 CPU（RGBA8）。L3 判分用。
    CapturedImage renderAndCapture(const RecordFn& record);

    // 改变在飞帧数并重建 per-frame 资源。
    void setFramesInFlight(std::uint32_t n);
    std::uint32_t framesInFlight() const { return m_framesInFlight; }

    const std::vector<VkCommandBuffer>& commandBuffers() const { return m_commandBuffers; }

    // 外部（比如窗口回调）主动要求下一帧重建 swapchain。
    void requestResize() { m_resizeRequested = true; }

private:
    void createFrameResources();
    void destroyFrameResources() noexcept;
    void createImageResources();
    void destroyImageResources() noexcept;
    void handleResize();

    Context&   m_ctx;
    Swapchain& m_swapchain;

    std::uint32_t m_framesInFlight  = 2;
    std::uint32_t m_currentFrame    = 0;
    bool          m_resizeRequested = false;

    // 每个在飞帧一份
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore>     m_imageAvailable;
    std::vector<VkFence>         m_inFlightFences;

    // 每张 swapchain 图像一份
    std::vector<VkSemaphore> m_renderFinished;
    // 「这张图像上一次是被哪个帧槽位用的」。不拥有这些 fence，只是引用。
    std::vector<VkFence>     m_imagesInFlight;

    ResizeFn m_onResize;
};

} // namespace rwb::rhi
