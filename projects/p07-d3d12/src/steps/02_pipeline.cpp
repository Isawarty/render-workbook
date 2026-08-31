#include "D3D12App.h"

#include <d3dcompiler.h>

#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace {

using Microsoft::WRL::ComPtr;

[[noreturn]] void throwHr(const char* operation, HRESULT result,
                          ID3DBlob* diagnostic = nullptr) {
    std::ostringstream message;
    message << operation << " failed (HRESULT=0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ')';
    if (diagnostic && diagnostic->GetBufferPointer()) {
        message << ": "
                << static_cast<const char*>(diagnostic->GetBufferPointer());
    }
    throw std::runtime_error(message.str());
}

void checkHr(HRESULT result, const char* operation) {
    if (FAILED(result)) throwHr(operation, result);
}

ComPtr<ID3DBlob> compileShader(const wchar_t* path, const char* entry, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(
        path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target,
        flags, 0, &bytecode, &errors);
    if (FAILED(result)) throwHr("D3DCompileFromFile", result, errors.Get());
    return bytecode;
}

D3D12_BLEND_DESC defaultBlendState() {
    D3D12_BLEND_DESC desc{};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC target{
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL};
    for (auto& renderTarget : desc.RenderTarget) renderTarget = target;
    return desc;
}

D3D12_RASTERIZER_DESC defaultRasterizerState() {
    D3D12_RASTERIZER_DESC desc{};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.CullMode = D3D12_CULL_MODE_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc.DepthClipEnable = TRUE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    desc.ForcedSampleCount = 0;
    desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return desc;
}

} // namespace

namespace p07 {

void D3D12App::createPipeline() {
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 2;
    rootDesc.pParameters = rootParameters;
    rootDesc.NumStaticSamplers = 1;
    rootDesc.pStaticSamplers = &sampler;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRoot;
    ComPtr<ID3DBlob> rootErrors;
    const HRESULT serializeResult = D3D12SerializeRootSignature(
        &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRoot, &rootErrors);
    if (FAILED(serializeResult)) {
        throwHr("D3D12SerializeRootSignature", serializeResult, rootErrors.Get());
    }
    checkHr(m_device->CreateRootSignature(
                0, serializedRoot->GetBufferPointer(), serializedRoot->GetBufferSize(),
                IID_PPV_ARGS(&m_rootSignature)),
            "ID3D12Device::CreateRootSignature");
    m_rootSignature->SetName(L"P07 Graphics Root Signature");
    m_summary.hasRootSignature = true;
    m_summary.rootParameterCount = 2;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    checkHr(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)),
            "ID3D12Device::CreateDescriptorHeap(CBV_SRV_UAV)");
    m_srvHeap->SetName(L"P07 Shader-visible SRV Heap");
    m_summary.hasSrvHeap = true;
    m_summary.srvHeapShaderVisible = true;

#ifdef RWB_P07_SOURCE_SHADER_DIR
    const auto shaderPath =
        std::filesystem::path(RWB_P07_SOURCE_SHADER_DIR) / "triangle.hlsl";
#else
    throw std::runtime_error("RWB_P07_SOURCE_SHADER_DIR is not defined");
#endif
    const auto vertexShader = compileShader(shaderPath.c_str(), "vsMain", "vs_5_1");
    const auto pixelShader = compileShader(shaderPath.c_str(), "psMain", "ps_5_1");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.BlendState = defaultBlendState();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = defaultRasterizerState();
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = {nullptr, 0};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    checkHr(m_device->CreateGraphicsPipelineState(&psoDesc,
                                                   IID_PPV_ARGS(&m_graphicsPso)),
            "ID3D12Device::CreateGraphicsPipelineState");
    m_graphicsPso->SetName(L"P07 Triangle Graphics PSO");
    m_summary.hasGraphicsPso = true;
    updateInfoQueueStatus();
}

} // namespace p07
