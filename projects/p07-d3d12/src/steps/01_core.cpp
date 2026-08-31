#include "D3D12App.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

constexpr wchar_t kWindowClassName[] = L"RenderWorkbookP07Window";

[[noreturn]] void throwHr(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed (HRESULT=0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ')';
    throw std::runtime_error(message.str());
}

void checkHr(HRESULT result, const char* operation) {
    if (FAILED(result)) throwHr(operation, result);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

HWND createHiddenWindow(uint32_t width, uint32_t height) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = windowProc;
    windowClass.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("RegisterClassExW failed");
    }

    HWND window = CreateWindowExW(
        0, kWindowClassName, L"render-workbook P07", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(width), static_cast<int>(height),
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr) throw std::runtime_error("CreateWindowExW failed");
    return window;
}

} // namespace

namespace p07 {

void D3D12App::createCore() {
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    checkHr(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
            "D3D12GetDebugInterface (install Windows Graphics Tools)");
    debugController->EnableDebugLayer();
    m_summary.debugLayerEnabled = true;

    checkHr(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_factory)),
            "CreateDXGIFactory2");
    checkHr(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter)),
            "IDXGIFactory::EnumWarpAdapter");
    DXGI_ADAPTER_DESC1 adapterDesc{};
    checkHr(m_adapter->GetDesc1(&adapterDesc), "IDXGIAdapter::GetDesc1");
    m_summary.usingWarp = true;
    m_summary.adapterIsSoftware =
        (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

    checkHr(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                              IID_PPV_ARGS(&m_device)),
            "D3D12CreateDevice(WARP, feature level 12_0)");
    m_device->SetName(L"P07 WARP Device");
    checkHr(m_device.As(&m_infoQueue), "QueryInterface(ID3D12InfoQueue)");

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    checkHr(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue)),
            "ID3D12Device::CreateCommandQueue");
    m_queue->SetName(L"P07 Direct Queue");
    m_summary.hasQueue = true;
    m_summary.queueType = queueDesc.Type;

    for (uint32_t frame = 0; frame < m_allocators.size(); ++frame) {
        checkHr(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  IID_PPV_ARGS(&m_allocators[frame])),
                "ID3D12Device::CreateCommandAllocator");
        const std::wstring name = L"P07 Frame Allocator " + std::to_wstring(frame);
        m_allocators[frame]->SetName(name.c_str());
    }
    m_summary.allocatorCount = static_cast<uint32_t>(m_allocators.size());

    checkHr(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         m_allocators[0].Get(), nullptr,
                                         IID_PPV_ARGS(&m_commandList)),
            "ID3D12Device::CreateCommandList");
    m_commandList->SetName(L"P07 Direct Command List");
    checkHr(m_commandList->Close(), "ID3D12GraphicsCommandList::Close");
    m_summary.hasCommandList = true;
    m_summary.commandListClosed = true;

    m_window = createHiddenWindow(m_summary.width, m_summary.height);

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = m_summary.width;
    swapchainDesc.Height = m_summary.height;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = static_cast<uint32_t>(m_backBuffers.size());
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain;
    checkHr(m_factory->CreateSwapChainForHwnd(
                m_queue.Get(), m_window, &swapchainDesc, nullptr, nullptr, &swapchain),
            "IDXGIFactory::CreateSwapChainForHwnd");
    checkHr(m_factory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER),
            "IDXGIFactory::MakeWindowAssociation");
    checkHr(swapchain.As(&m_swapchain), "QueryInterface(IDXGISwapChain3)");

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = static_cast<uint32_t>(m_backBuffers.size());
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    checkHr(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
            "ID3D12Device::CreateDescriptorHeap(RTV)");
    m_rtvHeap->SetName(L"P07 Swapchain RTV Heap");
    m_summary.hasRtvHeap = true;

    const uint32_t descriptorSize =
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t frame = 0; frame < m_backBuffers.size(); ++frame) {
        checkHr(m_swapchain->GetBuffer(frame, IID_PPV_ARGS(&m_backBuffers[frame])),
                "IDXGISwapChain::GetBuffer");
        const std::wstring name = L"P07 Back Buffer " + std::to_wstring(frame);
        m_backBuffers[frame]->SetName(name.c_str());
        m_device->CreateRenderTargetView(m_backBuffers[frame].Get(), nullptr, rtv);
        rtv.ptr += descriptorSize;
    }

    m_summary.hasSwapchain = true;
    m_summary.frameCount = static_cast<uint32_t>(m_backBuffers.size());
    updateInfoQueueStatus();
}

} // namespace p07
