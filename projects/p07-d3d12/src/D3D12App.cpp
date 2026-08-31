#include "D3D12App.h"

#include <sstream>
#include <stdexcept>
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

        // t01/t02 只建立对象；t03 创建 fence 后才开启安全的提交与 present。
        if (!m_fence) continue;

        const uint32_t frameIndex = m_swapchain->GetCurrentBackBufferIndex();
        waitForFence(m_frameFenceValues[frameIndex]);

        const HRESULT allocatorResult = m_allocators[frameIndex]->Reset();
        if (FAILED(allocatorResult)) {
            throw std::runtime_error("ID3D12CommandAllocator::Reset failed");
        }
        const HRESULT listResult =
            m_commandList->Reset(m_allocators[frameIndex].Get(), m_graphicsPso.Get());
        if (FAILED(listResult)) {
            throw std::runtime_error("ID3D12GraphicsCommandList::Reset failed");
        }

        D3D12_RESOURCE_BARRIER begin{};
        begin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        begin.Transition.pResource = m_backBuffers[frameIndex].Get();
        begin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        begin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        begin.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_commandList->ResourceBarrier(1, &begin);
        m_beginBarrier = {begin.Transition.StateBefore, begin.Transition.StateAfter};

        const uint32_t descriptorSize =
            m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(frameIndex) * descriptorSize;
        constexpr float clearColor[] = {0.04f, 0.06f, 0.10f, 1.0f};
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        D3D12_RESOURCE_BARRIER end = begin;
        end.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        end.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_commandList->ResourceBarrier(1, &end);
        m_endBarrier = {end.Transition.StateBefore, end.Transition.StateAfter};

        if (FAILED(m_commandList->Close())) {
            throw std::runtime_error("ID3D12GraphicsCommandList::Close failed");
        }
        ID3D12CommandList* lists[] = {m_commandList.Get()};
        m_queue->ExecuteCommandLists(1, lists);
        if (FAILED(m_swapchain->Present(0, 0))) {
            throw std::runtime_error("IDXGISwapChain::Present failed");
        }

        const uint64_t fenceValue = m_nextFenceValue++;
        if (FAILED(m_queue->Signal(m_fence.Get(), fenceValue))) {
            throw std::runtime_error("ID3D12CommandQueue::Signal failed");
        }
        m_frameFenceValues[frameIndex] = fenceValue;
    }
    if (m_fence) waitForGpu();
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
