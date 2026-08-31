#include "D3D12App.h"

#include <d3dcompiler.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace {
using Microsoft::WRL::ComPtr;

struct Vertex { float position[3]; float uv[2]; };

[[noreturn]] void throwHr(const char* operation, HRESULT result,
                          ID3DBlob* diagnostic = nullptr) {
    std::ostringstream message;
    message << operation << " failed (HRESULT=0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ')';
    if (diagnostic && diagnostic->GetBufferPointer())
        message << ": " << static_cast<const char*>(diagnostic->GetBufferPointer());
    throw std::runtime_error(message.str());
}

void checkHr(HRESULT result, const char* operation) {
    if (FAILED(result)) throwHr(operation, result);
}

D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = type;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_RESOURCE_DESC bufferDesc(uint64_t byteCount) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = byteCount;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

ComPtr<ID3D12Resource> createBuffer(ID3D12Device* device, uint64_t byteCount,
                                    D3D12_HEAP_TYPE heapType,
                                    D3D12_RESOURCE_STATES initialState,
                                    const wchar_t* name) {
    const auto heap = heapProperties(heapType);
    const auto desc = bufferDesc(byteCount);
    ComPtr<ID3D12Resource> resource;
    checkHr(device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr,
                IID_PPV_ARGS(&resource)),
            "CreateCommittedResource(buffer)");
    resource->SetName(name);
    return resource;
}

ComPtr<ID3DBlob> compileShader(const std::filesystem::path& path,
                               const char* entry, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> bytecode, errors;
    const HRESULT result = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, &bytecode, &errors);
    if (FAILED(result)) throwHr("D3DCompileFromFile(cube.hlsl)", result, errors.Get());
    return bytecode;
}

D3D12_BLEND_DESC defaultBlendState() {
    D3D12_BLEND_DESC desc{};
    const D3D12_RENDER_TARGET_BLEND_DESC target{
        FALSE, FALSE, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL};
    for (auto& renderTarget : desc.RenderTarget) renderTarget = target;
    return desc;
}

D3D12_RASTERIZER_DESC defaultRasterizerState() {
    D3D12_RASTERIZER_DESC desc{};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    // t04 尚未引入 depth buffer；关闭剔除让所有 winding 都能稳定留下可观察输出。
    desc.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc.DepthClipEnable = TRUE;
    desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return desc;
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
                                  D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}
} // namespace

namespace p07 {

void D3D12App::createTexturedCube() {
    constexpr std::array<Vertex, 24> vertices{{
        {{-1,-1,-1},{0,1}}, {{-1, 1,-1},{0,0}}, {{ 1, 1,-1},{1,0}}, {{ 1,-1,-1},{1,1}},
        {{ 1,-1, 1},{0,1}}, {{ 1, 1, 1},{0,0}}, {{-1, 1, 1},{1,0}}, {{-1,-1, 1},{1,1}},
        {{-1,-1, 1},{0,1}}, {{-1, 1, 1},{0,0}}, {{-1, 1,-1},{1,0}}, {{-1,-1,-1},{1,1}},
        {{ 1,-1,-1},{0,1}}, {{ 1, 1,-1},{0,0}}, {{ 1, 1, 1},{1,0}}, {{ 1,-1, 1},{1,1}},
        {{-1, 1,-1},{0,1}}, {{-1, 1, 1},{0,0}}, {{ 1, 1, 1},{1,0}}, {{ 1, 1,-1},{1,1}},
        {{-1,-1, 1},{0,1}}, {{-1,-1,-1},{0,0}}, {{ 1,-1,-1},{1,0}}, {{ 1,-1, 1},{1,1}},
    }};
    constexpr std::array<uint16_t, 36> indices{{
         0, 1, 2,  0, 2, 3,  4, 5, 6,  4, 6, 7,
         8, 9,10,  8,10,11, 12,13,14, 12,14,15,
        16,17,18, 16,18,19, 20,21,22, 20,22,23,
    }};
    const uint64_t vertexBytes = sizeof(vertices), indexBytes = sizeof(indices);
    m_vertexBuffer = createBuffer(m_device.Get(), vertexBytes, D3D12_HEAP_TYPE_DEFAULT,
                                  D3D12_RESOURCE_STATE_COPY_DEST, L"P07 Cube Vertex Buffer");
    m_indexBuffer = createBuffer(m_device.Get(), indexBytes, D3D12_HEAP_TYPE_DEFAULT,
                                 D3D12_RESOURCE_STATE_COPY_DEST, L"P07 Cube Index Buffer");
    auto vertexUpload = createBuffer(m_device.Get(), vertexBytes, D3D12_HEAP_TYPE_UPLOAD,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, L"P07 Vertex Upload");
    auto indexUpload = createBuffer(m_device.Get(), indexBytes, D3D12_HEAP_TYPE_UPLOAD,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, L"P07 Index Upload");
    void* mapped = nullptr;
    checkHr(vertexUpload->Map(0, nullptr, &mapped), "Map(vertex upload)");
    std::memcpy(mapped, vertices.data(), vertexBytes);
    vertexUpload->Unmap(0, nullptr);
    checkHr(indexUpload->Map(0, nullptr, &mapped), "Map(index upload)");
    std::memcpy(mapped, indices.data(), indexBytes);
    indexUpload->Unmap(0, nullptr);

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = 8; textureDesc.Height = 8;
    textureDesc.DepthOrArraySize = 1; textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    const auto defaultHeap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    checkHr(m_device->CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture)),
            "CreateCommittedResource(checker texture)");
    m_texture->SetName(L"P07 8x8 Checker Texture");

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t textureUploadBytes = 0;
    m_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint,
                                    nullptr, nullptr, &textureUploadBytes);
    auto textureUpload = createBuffer(m_device.Get(), textureUploadBytes,
                                      D3D12_HEAP_TYPE_UPLOAD,
                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                      L"P07 Texture Upload");
    checkHr(textureUpload->Map(0, nullptr, &mapped), "Map(texture upload)");
    std::memset(mapped, 0, static_cast<size_t>(textureUploadBytes));
    for (uint32_t y = 0; y < 8; ++y) {
        auto* row = static_cast<uint8_t*>(mapped) + footprint.Offset +
                    static_cast<size_t>(y) * footprint.Footprint.RowPitch;
        for (uint32_t x = 0; x < 8; ++x) {
            const bool light = ((x / 2) + (y / 2)) % 2 == 0;
            row[x*4+0] = light ? 245 : 35; row[x*4+1] = light ? 210 : 70;
            row[x*4+2] = light ? 90 : 180; row[x*4+3] = 255;
        }
    }
    textureUpload->Unmap(0, nullptr);

    waitForFence(m_frameFenceValues[0]);
    checkHr(m_allocators[0]->Reset(), "Reset allocator for cube upload");
    checkHr(m_commandList->Reset(m_allocators[0].Get(), nullptr), "Reset list for cube upload");
    m_commandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, vertexUpload.Get(), 0, vertexBytes);
    m_commandList->CopyBufferRegion(m_indexBuffer.Get(), 0, indexUpload.Get(), 0, indexBytes);
    D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = m_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = textureUpload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = footprint;
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    std::array<D3D12_RESOURCE_BARRIER, 3> barriers{{
        transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
        transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_INDEX_BUFFER),
        transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)}};
    m_commandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
    checkHr(m_commandList->Close(), "Close cube upload list");
    ID3D12CommandList* uploadLists[] = {m_commandList.Get()};
    m_queue->ExecuteCommandLists(1, uploadLists);
    waitForGpu();

    m_constantBuffer = createBuffer(m_device.Get(), 256, D3D12_HEAP_TYPE_UPLOAD,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, L"P07 Cube Transform CB");
    const D3D12_RANGE noRead{0, 0};
    checkHr(m_constantBuffer->Map(0, &noRead, &m_constantMapped), "Map cube constants");
    std::memset(m_constantMapped, 0, 256);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = textureDesc.Format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_texture.Get(), &srv,
                                       m_srvHeap->GetCPUDescriptorHandleForHeapStart());

#ifdef RWB_P07_SOURCE_SHADER_DIR
    const auto shaderPath = std::filesystem::path(RWB_P07_SOURCE_SHADER_DIR) / "cube.hlsl";
#else
    throw std::runtime_error("RWB_P07_SOURCE_SHADER_DIR is not defined");
#endif
    const auto vs = compileShader(shaderPath, "vsMain", "vs_5_1");
    const auto ps = compileShader(shaderPath, "psMain", "ps_5_1");
    const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.BlendState = defaultBlendState(); pso.SampleMask = UINT_MAX;
    pso.RasterizerState = defaultRasterizerState();
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pso.InputLayout = {inputLayout, 2};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1; pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> cubePso;
    checkHr(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&cubePso)),
            "CreateGraphicsPipelineState(cube)");
    cubePso->SetName(L"P07 Textured Cube PSO"); m_graphicsPso = cubePso;

    m_vertexView = {m_vertexBuffer->GetGPUVirtualAddress(),
                    static_cast<uint32_t>(vertexBytes), static_cast<uint32_t>(sizeof(Vertex))};
    m_indexView = {m_indexBuffer->GetGPUVirtualAddress(),
                   static_cast<uint32_t>(indexBytes), DXGI_FORMAT_R16_UINT};

    const auto backBufferDesc = m_backBuffers[0]->GetDesc();
    uint64_t frameReadbackBytes = 0;
    m_device->GetCopyableFootprints(
        &backBufferDesc, 0, 1, 0, &m_frameReadbackFootprint,
        nullptr, nullptr, &frameReadbackBytes);
    for (uint32_t frame = 0; frame < m_frameReadbacks.size(); ++frame) {
        const std::wstring name = L"P07 Frame Readback " + std::to_wstring(frame);
        m_frameReadbacks[frame] = createBuffer(
            m_device.Get(), frameReadbackBytes, D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_STATE_COPY_DEST, name.c_str());
    }
    m_summary.hasCubeResources = true;
    m_summary.indexCount = static_cast<uint32_t>(indices.size());
    m_summary.textureWidth = 8; m_summary.textureHeight = 8;
    m_summary.textureFormat = textureDesc.Format;
    updateInfoQueueStatus();
}

std::vector<uint8_t> D3D12App::readbackFrameRgba8() {
    if (!m_summary.hasCubeResources)
        throw std::runtime_error("readbackFrameRgba8 requires P07-t04 resources");
    waitForGpu();
    const uint32_t frameIndex = m_lastRenderedFrameIndex;
    const SIZE_T mappedBytes = static_cast<SIZE_T>(
        m_frameReadbackFootprint.Offset +
        static_cast<uint64_t>(m_frameReadbackFootprint.Footprint.RowPitch) *
            m_summary.height);
    const D3D12_RANGE readRange{0, mappedBytes};
    void* mapped = nullptr;
    checkHr(m_frameReadbacks[frameIndex]->Map(0, &readRange, &mapped),
            "Map frame readback");
    std::vector<uint8_t> rgba(static_cast<size_t>(m_summary.width) * m_summary.height * 4);
    for (uint32_t y = 0; y < m_summary.height; ++y) {
        const auto* source = static_cast<const uint8_t*>(mapped) +
                             m_frameReadbackFootprint.Offset +
                             static_cast<size_t>(y) *
                                 m_frameReadbackFootprint.Footprint.RowPitch;
        std::memcpy(rgba.data() + static_cast<size_t>(y) * m_summary.width * 4,
                    source, static_cast<size_t>(m_summary.width) * 4);
    }
    const D3D12_RANGE noWrite{0, 0};
    m_frameReadbacks[frameIndex]->Unmap(0, &noWrite);
    updateInfoQueueStatus();
    return rgba;
}
} // namespace p07
