#include <catch2/catch_test_macros.hpp>

#include "D3D12App.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace p07;

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("t01 WARP core creates the command and swapchain path", "[t01]") {
    D3D12App app;
    app.initialize(Stage::Core);
    app.runFrames(2);
    const auto& state = app.summary();
    REQUIRE(state.usingWarp);
    REQUIRE(state.adapterIsSoftware);
    REQUIRE(state.debugLayerEnabled);
    REQUIRE(state.hasQueue);
    REQUIRE(state.hasCommandList);
    REQUIRE(state.commandListClosed);
    REQUIRE(state.hasSwapchain);
    REQUIRE(state.hasRtvHeap);
    REQUIRE(state.frameCount == 2);
    REQUIRE(state.allocatorCount == state.frameCount);
    REQUIRE(state.queueType == D3D12_COMMAND_LIST_TYPE_DIRECT);
    INFO(state.infoQueueErrors);
    REQUIRE(state.infoQueueClean);
}

TEST_CASE("t02 root signature PSO and shader-visible heap are live", "[t02]") {
    D3D12App app;
    app.initialize(Stage::Pipeline);
    const auto& state = app.summary();
    REQUIRE(state.hasRootSignature);
    REQUIRE(state.hasGraphicsPso);
    REQUIRE(state.hasSrvHeap);
    REQUIRE(state.srvHeapShaderVisible);
    REQUIRE(state.rootParameterCount == 2);
    INFO(state.infoQueueErrors);
    REQUIRE(state.infoQueueClean);
}

TEST_CASE("t03 transition and fence expose the required ordering", "[t03]") {
    D3D12App app;
    app.initialize(Stage::Synchronization);
    app.runFrames(3);
    REQUIRE(app.summary().hasFence);
    REQUIRE(app.summary().completedFence >= 3);
    REQUIRE(app.beginBarrier().before == D3D12_RESOURCE_STATE_PRESENT);
    REQUIRE(app.beginBarrier().after == D3D12_RESOURCE_STATE_RENDER_TARGET);
    REQUIRE(app.endBarrier().before == D3D12_RESOURCE_STATE_RENDER_TARGET);
    REQUIRE(app.endBarrier().after == D3D12_RESOURCE_STATE_PRESENT);
    REQUIRE(app.summary().infoQueueClean);
}

TEST_CASE("t04 indexed textured cube survives the WARP render path", "[t04]") {
    D3D12App app;
    app.initialize(Stage::TexturedCube);
    app.runFrames(2);
    REQUIRE(app.summary().hasCubeResources);
    REQUIRE(app.summary().hasGraphicsPso);
    REQUIRE(app.summary().indexCount == 36);
    REQUIRE(app.summary().textureWidth == 8);
    REQUIRE(app.summary().textureHeight == 8);
    REQUIRE(app.summary().textureFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
    const auto rgba = app.readbackFrameRgba8();
    REQUIRE(rgba.size() == static_cast<size_t>(app.summary().width) *
                               app.summary().height * 4);
    size_t opaquePixels = 0;
    size_t coloredPixels = 0;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        if (rgba[i + 3] == 255) ++opaquePixels;
        if (rgba[i] > 96 || rgba[i + 1] > 96 || rgba[i + 2] > 96) ++coloredPixels;
    }
    REQUIRE(opaquePixels == rgba.size() / 4);
    REQUIRE(coloredPixels > 500);
    REQUIRE(app.summary().infoQueueClean);
}

TEST_CASE("t05 compute SAXPY readback matches the CPU reference", "[t05]") {
    D3D12App app;
    app.initialize(Stage::Compute);
    const std::vector<float> x{1, 2, 3, 4, 5, 6, 7};
    const std::vector<float> y{7, 6, 5, 4, 3, 2, 1};
    const auto result = app.runSaxpy(x, y, 2.5f);
    REQUIRE(result.size() == x.size());
    for (size_t i = 0; i < result.size(); ++i) {
        REQUIRE(std::abs(result[i] - (2.5f * x[i] + y[i])) < 1e-6f);
    }
    REQUIRE(app.summary().hasComputePso);
    REQUIRE(app.summary().infoQueueClean);
}

TEST_CASE("t06 one Slang entry produces both DXIL and SPIR-V", "[t06]") {
    const auto directory = std::filesystem::path(shaderDirectory());
    const auto dxil = directory / "shared.dxil";
    const auto spirv = directory / "shared.spv";
    REQUIRE(std::filesystem::exists(dxil));
    REQUIRE(std::filesystem::exists(spirv));
    REQUIRE(std::filesystem::file_size(dxil) > 32);
    REQUIRE(std::filesystem::file_size(spirv) > 32);

    const auto dxilBytes = readText(dxil);
    const auto spirvBytes = readText(spirv);
    REQUIRE(dxilBytes.substr(0, 4) == "DXBC");
    REQUIRE(static_cast<unsigned char>(spirvBytes[0]) == 0x03);
    REQUIRE(static_cast<unsigned char>(spirvBytes[1]) == 0x02);
    REQUIRE(static_cast<unsigned char>(spirvBytes[2]) == 0x23);
    REQUIRE(static_cast<unsigned char>(spirvBytes[3]) == 0x07);
}

TEST_CASE("t07 comparison covers ownership binding barriers and synchronization", "[t07]") {
    const auto text = readText("projects/p07-d3d12/docs/vulkan-d3d12-map.md");
    REQUIRE(text.find("TODO") == std::string::npos);
    for (const std::string concept : {
             "VkDevice", "ID3D12Device", "VkQueue", "ID3D12CommandQueue",
             "VkDescriptorSet", "Descriptor Table", "vkCmdPipelineBarrier2",
             "ResourceBarrier", "VkFence", "ID3D12Fence", "WARP"}) {
        INFO("missing concept: " << concept);
        REQUIRE(text.find(concept) != std::string::npos);
    }
}
