#include "D3D12App.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

[[noreturn]] void throwHr(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed (HRESULT=0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ')';
    throw std::runtime_error(message.str());
}

void checkHr(HRESULT result, const char* operation) {
    if (FAILED(result)) throwHr(operation, result);
}

} // namespace

namespace p07 {

void D3D12App::createSynchronization() {
    checkHr(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&m_fence)),
            "ID3D12Device::CreateFence");
    m_fence->SetName(L"P07 Frame Fence");
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr) {
        throw std::runtime_error("CreateEventW for P07 frame fence failed");
    }
    m_summary.hasFence = true;
    m_summary.completedFence = m_fence->GetCompletedValue();
    updateInfoQueueStatus();
}

void D3D12App::waitForGpu() {
    const uint64_t value = m_nextFenceValue++;
    checkHr(m_queue->Signal(m_fence.Get(), value),
            "ID3D12CommandQueue::Signal(waitForGpu)");
    waitForFence(value);
}

void D3D12App::waitForFence(uint64_t value) {
    if (value == 0 || m_fence->GetCompletedValue() >= value) {
        m_summary.completedFence = m_fence->GetCompletedValue();
        return;
    }
    checkHr(m_fence->SetEventOnCompletion(value, m_fenceEvent),
            "ID3D12Fence::SetEventOnCompletion");
    const DWORD waitResult = WaitForSingleObject(m_fenceEvent, 30'000);
    if (waitResult != WAIT_OBJECT_0) {
        throw std::runtime_error("P07 fence wait timed out or failed");
    }
    m_summary.completedFence = m_fence->GetCompletedValue();
}

} // namespace p07
