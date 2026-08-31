#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace p07 {

enum class Stage : uint32_t {
    Core = 1,
    Pipeline = 2,
    Synchronization = 3,
    TexturedCube = 4,
    Compute = 5,
    Slang = 6,
};

struct StateSummary {
    bool usingWarp = false;
    bool adapterIsSoftware = false;
    bool debugLayerEnabled = false;
    bool infoQueueClean = false;
    bool hasQueue = false;
    bool hasCommandList = false;
    bool commandListClosed = false;
    bool hasSwapchain = false;
    bool hasRtvHeap = false;
    bool hasRootSignature = false;
    bool hasGraphicsPso = false;
    bool hasSrvHeap = false;
    bool srvHeapShaderVisible = false;
    bool hasFence = false;
    bool hasCubeResources = false;
    bool hasComputePso = false;
    uint32_t frameCount = 0;
    uint32_t allocatorCount = 0;
    uint32_t width = 128;
    uint32_t height = 128;
    uint32_t rootParameterCount = 0;
    uint32_t indexCount = 0;
    uint32_t textureWidth = 0;
    uint32_t textureHeight = 0;
    D3D12_COMMAND_LIST_TYPE queueType = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
    uint64_t completedFence = 0;
    std::string infoQueueErrors;
};

struct BarrierSnapshot {
    D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_COMMON;
};

class D3D12App {
public:
    D3D12App() = default;
    ~D3D12App();
    D3D12App(const D3D12App&) = delete;
    D3D12App& operator=(const D3D12App&) = delete;

    void initialize(Stage stage);
    void runFrames(uint32_t frameCount);
    std::vector<uint8_t> readbackFrameRgba8();
    std::vector<float> runSaxpy(const std::vector<float>& x,
                                const std::vector<float>& y,
                                float a);

    const StateSummary& summary() const { return m_summary; }
    const BarrierSnapshot& beginBarrier() const { return m_beginBarrier; }
    const BarrierSnapshot& endBarrier() const { return m_endBarrier; }
    ID3D12Device* device() const { return m_device.Get(); }

private:
    void createCore();
    void createPipeline();
    void createSynchronization();
    void createTexturedCube();
    void createComputePipeline();
    void waitForGpu();
    void updateInfoQueueStatus();

    StateSummary m_summary;
    BarrierSnapshot m_beginBarrier;
    BarrierSnapshot m_endBarrier;
    HWND m_window = nullptr;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_nextFenceValue = 1;

    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, 2> m_allocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapchain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_graphicsPso;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePso;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_computeOutput;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> m_infoQueue;
};

std::string shaderDirectory();

} // namespace p07
