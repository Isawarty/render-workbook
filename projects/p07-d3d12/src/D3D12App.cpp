#include "D3D12App.h"

#include <rwb/core/Todo.h>

namespace p07 {

D3D12App::~D3D12App() {
    if (m_queue && m_fence) {
        try { waitForGpu(); } catch (...) {}
    }
    if (m_fenceEvent != nullptr) CloseHandle(m_fenceEvent);
    if (m_window != nullptr) DestroyWindow(m_window);
}

void D3D12App::initialize(Stage stage) {
    createCore();
    if (stage >= Stage::Pipeline) createPipeline();
    if (stage >= Stage::Synchronization) createSynchronization();
    if (stage >= Stage::TexturedCube) createTexturedCube();
    if (stage >= Stage::Compute) createComputePipeline();
}

void D3D12App::runFrames(uint32_t) {
    RWB_TODO("p07-t01 D3D12App::runFrames");
}

std::string shaderDirectory() {
#ifdef RWB_P07_SHADER_DIR
    return RWB_P07_SHADER_DIR;
#else
    return {};
#endif
}

} // namespace p07
