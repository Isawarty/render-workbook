// t09 — 多帧并行 (frames in flight)
//
// 任务书: projects/p01-triangle/docs/t09-frames-in-flight.md
// 判分:   ctest --preset win-msvc -R p01-t09
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <stdexcept>

namespace p01 {

void TriangleApp::setFramesInFlight(std::uint32_t n) {
    // TODO(p01-t09):
    //   把同时在飞的帧数改成 n，并重建对应的 per-frame 资源:
    //     1. vkDeviceWaitIdle —— 你要动的正是 GPU 可能正在用的东西
    //     2. 销毁旧的 imageAvailable semaphore / inFlight fence，
    //        并 vkFreeCommandBuffers 释放旧的 command buffer
    //     3. m_framesInFlight = n; m_currentFrame = 0;
    //     4. createCommandBuffers()（它读 m_framesInFlight，t06 写好就能直接复用）
    //     5. 按新数量重建 semaphore 和 fence，fence 仍然要 SIGNALED
    //
    //   关键判断: m_renderFinishedSemaphores 要不要跟着改数量?
    //   想清楚再动手 —— 这是本题唯一真正的考点。
    RWB_TODO("p01-t09 TriangleApp::setFramesInFlight");
}

} // namespace p01
