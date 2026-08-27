// t04 — render pass / framebuffer
//
// 任务书: projects/p01-triangle/docs/t04-renderpass.md
// 判分:   ctest --preset win-msvc -R p01-t04
#include "../TriangleApp.h"

#include "rwb/core/Todo.h"

#include <array>
#include <stdexcept>

namespace p01 {

void TriangleApp::createRenderPass() {
    // TODO(p01-t04):
    //   1. 一个 color attachment:
    //        format      = m_swapchainFormat
    //        loadOp      = CLEAR      (想清楚为什么不用 LOAD)
    //        storeOp     = STORE
    //        initialLayout = UNDEFINED
    //        finalLayout   = PRESENT_SRC_KHR
    //   2. 一个 subpass，绑定点 GRAPHICS，引用上面那个 attachment，
    //      layout 用 COLOR_ATTACHMENT_OPTIMAL
    //   3. 一条 VK_SUBPASS_EXTERNAL -> subpass 0 的依赖。
    //      这条最容易漏。漏了之后画面通常还是对的，但 sync validation 会报错 ——
    //      而 L1 判分把 warning 也算失败。
    //      src/dst stage 都用 COLOR_ATTACHMENT_OUTPUT，
    //      dstAccessMask 用 COLOR_ATTACHMENT_WRITE。
    //      想清楚: 这个 stage 和 t07 里 submit 时的 waitDstStageMask 是什么关系。
    RWB_TODO("p01-t04 TriangleApp::createRenderPass");
}

void TriangleApp::createFramebuffers() {
    // TODO(p01-t04):
    //   每个 swapchain image view 一个 framebuffer，尺寸取 m_swapchainExtent，layers = 1。
    //   它们共享同一个 m_renderPass。
    RWB_TODO("p01-t04 TriangleApp::createFramebuffers");
}

} // namespace p01
