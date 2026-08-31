#include "D3D12App.h"

#include <sstream>
#include <vector>

namespace p07 {

D3D12App::~D3D12App() {
    if (m_queue && m_fence) {
        try { waitForGpu(); } catch (...) {}
    }
    if (m_fenceEvent != nullptr) CloseHandle(m_fenceEvent);
    for (auto& buffer : m_backBuffers) buffer.Reset();
    m_swapchain.Reset();
    m_rtvHeap.Reset();
    if (m_window != nullptr) DestroyWindow(m_window);
}

void D3D12App::initialize(Stage stage) {
    createCore();
    if (stage >= Stage::Pipeline) createPipeline();
    if (stage >= Stage::Synchronization) createSynchronization();
    if (stage >= Stage::TexturedCube) createTexturedCube();
    if (stage >= Stage::Compute) createComputePipeline();
}

void D3D12App::runFrames(uint32_t frameCount) {
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        // t01 只建立窗口与 swapchain；没有 fence 前不提交/Present 后立刻销毁资源。
        // t03 会在补齐 barrier 与 fence timeline 后扩展这里的真正帧循环。
    }
    updateInfoQueueStatus();
}

void D3D12App::updateInfoQueueStatus() {
    m_summary.infoQueueClean = true;
    m_summary.infoQueueErrors.clear();
    if (!m_infoQueue) {
        m_summary.infoQueueClean = false;
        m_summary.infoQueueErrors = "ID3D12InfoQueue is unavailable";
        return;
    }

    const uint64_t messageCount = m_infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (uint64_t index = 0; index < messageCount; ++index) {
        SIZE_T byteCount = 0;
        if (FAILED(m_infoQueue->GetMessage(index, nullptr, &byteCount))) {
            m_summary.infoQueueClean = false;
            m_summary.infoQueueErrors = "ID3D12InfoQueue::GetMessage(size) failed";
            return;
        }
        std::vector<uint8_t> bytes(byteCount);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        if (FAILED(m_infoQueue->GetMessage(index, message, &byteCount))) {
            m_summary.infoQueueClean = false;
            m_summary.infoQueueErrors = "ID3D12InfoQueue::GetMessage(data) failed";
            return;
        }
        if (message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
            message->Severity == D3D12_MESSAGE_SEVERITY_ERROR) {
            m_summary.infoQueueClean = false;
            std::ostringstream diagnostic;
            diagnostic << "D3D12 message " << message->ID << ": "
                       << (message->pDescription ? message->pDescription : "<no description>");
            m_summary.infoQueueErrors = diagnostic.str();
            return;
        }
    }
}

std::string shaderDirectory() {
#ifdef RWB_P07_SHADER_DIR
    return RWB_P07_SHADER_DIR;
#else
    return {};
#endif
}

} // namespace p07
