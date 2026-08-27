// t03 — swapchain / image views
//
// 任务书: projects/p01-triangle/docs/t03-swapchain.md
// 判分:   ctest --preset win-msvc -R p01-t03
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace p01 {

void TriangleApp::createSwapchain() {
    // TODO(p01-t03):
    //   1. 选格式: 优先 B8G8R8A8_SRGB + SRGB_NONLINEAR
    //      （选 sRGB 才能让 shader 一直在线性空间算，P4 做 PBR 时会依赖这一点）
    //   2. 选呈现模式: FIFO 是唯一保证支持的，教学期用它
    //   3. 选尺寸: capabilities.currentExtent 为 UINT32_MAX 时才能自选，
    //      自选时要用 framebuffer 尺寸（Retina 上和窗口尺寸差 2 倍）并 clamp 到 min/max
    //   4. 图像数量: minImageCount + 1，并别超过 maxImageCount（0 表示无上限）
    //   5. imageUsage 要包含 COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT
    //      （后者是给 L3 截图用的，缺了 golden 测试跑不了）
    //   6. graphics != present 时用 CONCURRENT 共享模式，否则 EXCLUSIVE
    //   7. vkCreateSwapchainKHR，然后「重新查询」实际图像数（可能多于你要的）
    //   8. 记好 m_swapchainFormat / m_swapchainExtent，并让 m_swapchainGeneration 自增
    //      （t08 的测试靠这个计数判断你有没有真的重建过）
    RWB_TODO("p01-t03 TriangleApp::createSwapchain");
}

void TriangleApp::createImageViews() {
    // TODO(p01-t03):
    //   给每张 swapchain 图像建一个 2D、单 mip、单层的 color image view。
    RWB_TODO("p01-t03 TriangleApp::createImageViews");
}

} // namespace p01
