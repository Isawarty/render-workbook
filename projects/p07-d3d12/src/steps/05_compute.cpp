#include "D3D12App.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
using Microsoft::WRL::ComPtr;

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

D3D12_RESOURCE_DESC bufferDesc(uint64_t byteCount, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = byteCount;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    return desc;
}

ComPtr<ID3D12Resource> createBuffer(ID3D12Device* device, uint64_t byteCount,
                                    D3D12_HEAP_TYPE heapType,
                                    D3D12_RESOURCE_STATES initialState,
                                    D3D12_RESOURCE_FLAGS flags,
                                    const wchar_t* name) {
    const auto heap = heapProperties(heapType);
    const auto desc = bufferDesc(byteCount, flags);
    ComPtr<ID3D12Resource> resource;
    checkHr(device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr,
                IID_PPV_ARGS(&resource)),
            "CreateCommittedResource(compute buffer)");
    resource->SetName(name);
    return resource;
}

std::vector<uint8_t> readBinary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open compute shader: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
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

void D3D12App::createComputePipeline() {
    D3D12_ROOT_PARAMETER parameters[4]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[2].Descriptor.ShaderRegister = 1;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[3].Constants.ShaderRegister = 0;
    parameters[3].Constants.Num32BitValues = 2;
    parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 4;
    rootDesc.pParameters = parameters;
    ComPtr<ID3DBlob> serialized, errors;
    const HRESULT serializeResult = D3D12SerializeRootSignature(
        &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(serializeResult))
        throwHr("D3D12SerializeRootSignature(compute)", serializeResult, errors.Get());
    checkHr(m_device->CreateRootSignature(
                0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                IID_PPV_ARGS(&m_computeRootSignature)),
            "CreateRootSignature(compute)");
    m_computeRootSignature->SetName(L"P07 SAXPY Compute Root Signature");

    const auto bytecode = readBinary(std::filesystem::path(shaderDirectory()) / "shared.dxil");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_computeRootSignature.Get();
    pso.CS = {bytecode.data(), bytecode.size()};
    checkHr(m_device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_computePso)),
            "CreateComputePipelineState(SAXPY)");
    m_computePso->SetName(L"P07 SAXPY Compute PSO");
    m_summary.hasComputePso = true;
    updateInfoQueueStatus();
}

std::vector<float> D3D12App::runSaxpy(const std::vector<float>& x,
                                      const std::vector<float>& y,
                                      float a) {
    if (!m_computePso) throw std::runtime_error("runSaxpy requires P07-t05 pipeline");
    if (x.empty() || x.size() != y.size())
        throw std::invalid_argument("runSaxpy requires equal non-empty inputs");
    if (x.size() > std::numeric_limits<uint32_t>::max())
        throw std::length_error("runSaxpy input exceeds the shader's 32-bit count");
    const uint64_t byteCount = x.size() * sizeof(float);
    auto inputX = createBuffer(m_device.Get(), byteCount, D3D12_HEAP_TYPE_UPLOAD,
                               D3D12_RESOURCE_STATE_GENERIC_READ,
                               D3D12_RESOURCE_FLAG_NONE, L"P07 SAXPY X");
    auto inputY = createBuffer(m_device.Get(), byteCount, D3D12_HEAP_TYPE_UPLOAD,
                               D3D12_RESOURCE_STATE_GENERIC_READ,
                               D3D12_RESOURCE_FLAG_NONE, L"P07 SAXPY Y");
    m_computeOutput = createBuffer(
        m_device.Get(), byteCount, D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"P07 SAXPY Output");
    auto readback = createBuffer(m_device.Get(), byteCount, D3D12_HEAP_TYPE_READBACK,
                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                 D3D12_RESOURCE_FLAG_NONE, L"P07 SAXPY Readback");
    void* mapped = nullptr;
    checkHr(inputX->Map(0, nullptr, &mapped), "Map SAXPY X");
    std::memcpy(mapped, x.data(), static_cast<size_t>(byteCount));
    inputX->Unmap(0, nullptr);
    checkHr(inputY->Map(0, nullptr, &mapped), "Map SAXPY Y");
    std::memcpy(mapped, y.data(), static_cast<size_t>(byteCount));
    inputY->Unmap(0, nullptr);

    waitForGpu();
    checkHr(m_allocators[0]->Reset(), "Reset allocator for SAXPY");
    checkHr(m_commandList->Reset(m_allocators[0].Get(), m_computePso.Get()),
            "Reset command list for SAXPY");
    m_commandList->SetComputeRootSignature(m_computeRootSignature.Get());
    m_commandList->SetComputeRootUnorderedAccessView(
        0, m_computeOutput->GetGPUVirtualAddress());
    m_commandList->SetComputeRootShaderResourceView(1, inputX->GetGPUVirtualAddress());
    m_commandList->SetComputeRootShaderResourceView(2, inputY->GetGPUVirtualAddress());
    uint32_t constants[2]{};
    std::memcpy(&constants[0], &a, sizeof(float));
    constants[1] = static_cast<uint32_t>(x.size());
    m_commandList->SetComputeRoot32BitConstants(3, 2, constants, 0);
    m_commandList->Dispatch(static_cast<uint32_t>((x.size() + 63) / 64), 1, 1);

    D3D12_RESOURCE_BARRIER uav{};
    uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav.UAV.pResource = m_computeOutput.Get();
    m_commandList->ResourceBarrier(1, &uav);
    auto toCopy = transition(m_computeOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                             D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_commandList->ResourceBarrier(1, &toCopy);
    m_commandList->CopyBufferRegion(readback.Get(), 0, m_computeOutput.Get(), 0, byteCount);
    auto toUav = transition(m_computeOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_commandList->ResourceBarrier(1, &toUav);
    checkHr(m_commandList->Close(), "Close SAXPY command list");
    ID3D12CommandList* lists[] = {m_commandList.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    waitForGpu();

    std::vector<float> result(x.size());
    const D3D12_RANGE readRange{0, static_cast<SIZE_T>(byteCount)};
    checkHr(readback->Map(0, &readRange, &mapped), "Map SAXPY readback");
    std::memcpy(result.data(), mapped, static_cast<size_t>(byteCount));
    const D3D12_RANGE noWrite{0, 0};
    readback->Unmap(0, &noWrite);
    updateInfoQueueStatus();
    return result;
}
} // namespace p07
