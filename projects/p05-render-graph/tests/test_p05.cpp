#include <catch2/catch_test_macros.hpp>

#include "rwb/rendergraph/RenderGraph.h"
#include "rwb/rendergraph/TransientImagePool.h"
#include "RenderGraphApp.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

using namespace rwb::rg;

namespace {

ResourceState state(VkPipelineStageFlags stages, VkAccessFlags access,
                    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED) {
    return {stages, access, layout};
}

const Lifetime& lifetimeOf(const CompiledGraph& graph, ResourceHandle resource) {
    const auto found = std::find_if(graph.lifetimes().begin(), graph.lifetimes().end(),
        [resource](const Lifetime& lifetime) { return lifetime.resource == resource; });
    REQUIRE(found != graph.lifetimes().end());
    return *found;
}

struct VisualComparison {
    bool passed = false;
    std::size_t failingBlocks = 0;
    std::size_t totalBlocks = 0;
    int worstDelta = 0;
};

VisualComparison compareFrames(const rwb::rhi::CapturedImage& actual,
                               const rwb::rhi::CapturedImage& expected,
                               const std::string& deviceName) {
    std::string lower = deviceName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    const bool software = lower.find("lavapipe") != std::string::npos ||
                          lower.find("llvmpipe") != std::string::npos ||
                          lower.find("warp") != std::string::npos ||
                          lower.find("software") != std::string::npos;
    const std::uint32_t block = software ? 1u : 16u;
    const int allowedDelta = software ? 1 : 28;
    const double allowedRatio = software ? 0.0 : 0.02;
    VisualComparison result;
    for (std::uint32_t by = 0; by < actual.height; by += block) {
        for (std::uint32_t bx = 0; bx < actual.width; bx += block) {
            std::uint64_t sums[2][4]{};
            std::uint32_t count = 0;
            for (std::uint32_t y = by; y < std::min(by + block, actual.height); ++y) {
                for (std::uint32_t x = bx; x < std::min(bx + block, actual.width); ++x) {
                    const std::size_t offset =
                        (static_cast<std::size_t>(y) * actual.width + x) * 4;
                    for (int channel = 0; channel < 4; ++channel) {
                        sums[0][channel] += actual.pixels[offset + channel];
                        sums[1][channel] += expected.pixels[offset + channel];
                    }
                    ++count;
                }
            }
            int blockDelta = 0;
            for (int channel = 0; channel < 4; ++channel) {
                blockDelta = std::max(blockDelta, static_cast<int>(std::abs(
                    static_cast<long long>(sums[0][channel] / count) -
                    static_cast<long long>(sums[1][channel] / count))));
            }
            result.worstDelta = std::max(result.worstDelta, blockDelta);
            if (blockDelta > allowedDelta) ++result.failingBlocks;
            ++result.totalBlocks;
        }
    }
    const double ratio = result.totalBlocks
        ? static_cast<double>(result.failingBlocks) / result.totalBlocks
        : 1.0;
    result.passed = ratio <= allowedRatio;
    return result;
}

} // namespace

TEST_CASE("t01 pass 与 resource 使用声明保留执行契约", "[t01]") {
    RenderGraph graph;
    const auto color = graph.createResource({"HDR", ResourceKind::Image, true, 4096, 7});
    const auto output = graph.importResource({"Swapchain", ResourceKind::Image});
    std::vector<std::string> execution;
    graph.addPass("Lighting", [&](PassBuilder& pass) {
        pass.write(color, state(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }, [&](VkCommandBuffer) { execution.push_back("Lighting"); });
    graph.addPass("Tonemap", [&](PassBuilder& pass) {
        pass.read(color, state(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        pass.write(output, state(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }, [&](VkCommandBuffer) { execution.push_back("Tonemap"); });

    const CompiledGraph compiled = graph.compile();
    REQUIRE(compiled.passes().size() == 2);
    compiled.execute(VK_NULL_HANDLE);
    REQUIRE(execution == std::vector<std::string>{"Lighting", "Tonemap"});
}

TEST_CASE("t02 hazard 边形成稳定拓扑序且环会被拒绝", "[t02]") {
    RenderGraph graph;
    const auto gbuffer = graph.createResource({"GBuffer", ResourceKind::Image});
    const auto hdr = graph.createResource({"HDR", ResourceKind::Image});
    const auto geometry = graph.addPass("Geometry", [&](PassBuilder& pass) {
        pass.write(gbuffer, state(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    });
    graph.addPass("Lighting", [&](PassBuilder& pass) {
        pass.read(gbuffer, state(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_ACCESS_SHADER_READ_BIT));
        pass.write(hdr, state(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    });
    const CompiledGraph compiled = graph.compile();
    REQUIRE(compiled.passes()[0].handle == geometry);
    REQUIRE(compiled.passes()[1].dependencies == std::vector<PassHandle>{geometry});

    RenderGraph explicitCycle;
    const auto a = explicitCycle.addPass("A", [&](PassBuilder&) {});
    const auto b = explicitCycle.addPass("B", [&](PassBuilder& pass) { pass.dependsOn(a); });
    explicitCycle.addDependency(a, b);
    REQUIRE_THROWS_AS(explicitCycle.compile(), std::logic_error);
}

TEST_CASE("t03 写后读与 layout 变化推导精确 barrier", "[t03]") {
    RenderGraph graph;
    ResourceDesc desc{"HDR", ResourceKind::Image};
    desc.initialState = state(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                              VK_IMAGE_LAYOUT_UNDEFINED);
    const auto hdr = graph.createResource(desc);
    const auto lighting = graph.addPass("Lighting", [&](PassBuilder& pass) {
        pass.write(hdr, state(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    });
    const auto bloom = graph.addPass("Bloom", [&](PassBuilder& pass) {
        pass.read(hdr, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_ACCESS_SHADER_READ_BIT,
                             VK_IMAGE_LAYOUT_GENERAL));
    });
    const auto tonemap = graph.addPass("Tonemap", [&](PassBuilder& pass) {
        pass.read(hdr, state(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_ACCESS_SHADER_READ_BIT,
                             VK_IMAGE_LAYOUT_GENERAL));
        pass.dependsOn(bloom);
    });
    const auto compiled = graph.compile();
    REQUIRE(compiled.passes()[0].barriers.size() == 1);
    REQUIRE(compiled.passes()[1].barriers.size() == 1);
    const Barrier& barrier = compiled.passes()[1].barriers.front();
    REQUIRE(barrier.before == lighting);
    REQUIRE(barrier.after == bloom);
    REQUIRE(barrier.source.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REQUIRE(barrier.destination.layout == VK_IMAGE_LAYOUT_GENERAL);
    REQUIRE(barrier.source.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    REQUIRE(barrier.destination.access == VK_ACCESS_SHADER_READ_BIT);
    REQUIRE(compiled.passes()[2].handle == tonemap);
    REQUIRE(compiled.passes()[2].barriers.size() == 1);
    const Barrier& fanout = compiled.passes()[2].barriers.front();
    REQUIRE(fanout.before == lighting);
    REQUIRE(fanout.source.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    REQUIRE(fanout.destination.stages == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

TEST_CASE("t04 不重叠且兼容的 transient resource 复用物理槽", "[t04]") {
    RenderGraph graph;
    const auto shadow = graph.createResource({"ShadowScratch", ResourceKind::Image,
                                               true, 1024, 42});
    const auto bloom = graph.createResource({"BloomScratch", ResourceKind::Image,
                                              true, 2048, 42});
    const auto incompatible = graph.createResource({"Histogram", ResourceKind::Buffer,
                                                     true, 1024, 42});
    graph.addPass("Shadow", [&](PassBuilder& pass) {
        pass.write(shadow, state(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));
    });
    graph.addPass("ConsumeShadow", [&](PassBuilder& pass) {
        pass.read(shadow, state(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_ACCESS_SHADER_READ_BIT));
    });
    graph.addPass("Bloom", [&](PassBuilder& pass) {
        pass.write(bloom, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT));
        pass.write(incompatible, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_ACCESS_SHADER_WRITE_BIT));
    });
    const auto compiled = graph.compile();
    REQUIRE(lifetimeOf(compiled, shadow).lastUse < lifetimeOf(compiled, bloom).firstUse);
    REQUIRE(lifetimeOf(compiled, shadow).physicalSlot ==
            lifetimeOf(compiled, bloom).physicalSlot);
    REQUIRE(lifetimeOf(compiled, bloom).physicalSlot !=
            lifetimeOf(compiled, incompatible).physicalSlot);

    const std::string dot = compiled.toDot();
    REQUIRE(dot.find("digraph RenderGraph") != std::string::npos);
    REQUIRE(dot.find("ShadowScratch") != std::string::npos);
    REQUIRE(dot.find("ConsumeShadow") != std::string::npos);
}

TEST_CASE("t04 transient image 物理槽绑定同一 VMA allocation", "[t04]") {
    rwb::ValidationLog::instance().reset();
    rwb::rhi::ContextConfig config;
    config.headless = true;
    config.enableValidation = true;
    config.enableSyncValidation = true;
    rwb::rhi::Context context(config);

    RenderGraph graph;
    ResourceDesc firstDesc{"ScratchA", ResourceKind::Image, true, 64u * 64u * 4u, 99};
    firstDesc.width = 64;
    firstDesc.height = 64;
    firstDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    firstDesc.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ResourceDesc secondDesc = firstDesc;
    secondDesc.name = "ScratchB";
    const auto first = graph.createResource(firstDesc);
    const auto second = graph.createResource(secondDesc);
    graph.addPass("WriteA", [&](PassBuilder& pass) {
        pass.write(first, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
    });
    graph.addPass("ReadA", [&](PassBuilder& pass) {
        pass.read(first, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
    });
    graph.addPass("WriteB", [&](PassBuilder& pass) {
        pass.write(second, state(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
    });
    const CompiledGraph compiled = graph.compile();
    REQUIRE(lifetimeOf(compiled, first).physicalSlot ==
            lifetimeOf(compiled, second).physicalSlot);

    {
        rwb::rg::TransientImagePool pool(context, compiled);
        const auto& a = pool.binding(first);
        const auto& b = pool.binding(second);
        REQUIRE(a.image != VK_NULL_HANDLE);
        REQUIRE(b.image != VK_NULL_HANDLE);
        REQUIRE(a.image != b.image);
        REQUIRE(a.allocation == b.allocation);
        REQUIRE(a.physicalSlot == b.physicalSlot);
    }
    const auto& validation = rwb::ValidationLog::instance();
    INFO(validation.summary());
    REQUIRE(validation.errorCount() == 0);
    REQUIRE(validation.warningCount() == 0);
}

TEST_CASE("t05 P04 延迟管线由五个图 pass 调度且输出保持一致", "[t05]") {
    rwb::ValidationLog::instance().reset();
    p04::AppConfig config;
    config.width = 160;
    config.height = 120;
    config.highDpiFramebuffer = false;
    config.windowVisible = false;
    config.offscreenCapture = true;
    p05::RenderGraphApp app(config);
    app.init();

    const auto& passes = app.compiledGraph().passes();
    REQUIRE(passes.size() == 5);
    REQUIRE(passes[0].name == "Shadow");
    REQUIRE(passes[1].name == "Geometry");
    REQUIRE(passes[2].name == "Lighting");
    REQUIRE(passes[3].name == "Bloom");
    REQUIRE(passes[4].name == "Tonemap");
    REQUIRE(passes[3].barriers.size() >= 1);
    REQUIRE(passes[4].barriers.size() >= 1);

    // P04-t07 同样先走一次 G-buffer/final warm-up；MoltenVK 的首个隐藏窗口
    // storage-image 帧不作为画面基准。第二帧才与图路径做对照。
    (void)app.renderAndCaptureP04Reference();
    const auto p04Frame = app.renderAndCaptureP04Reference();
    const auto graphFrame = app.renderAndCaptureFinal();
    REQUIRE(graphFrame.width == p04Frame.width);
    REQUIRE(graphFrame.height == p04Frame.height);
    const auto comparison = compareFrames(
        graphFrame, p04Frame, app.deferred().context().deviceName());
    INFO("P04/graph structural mismatch blocks: " << comparison.failingBlocks
         << "/" << comparison.totalBlocks << ", worst delta="
         << comparison.worstDelta);
    REQUIRE(comparison.passed);

    const auto& validation = rwb::ValidationLog::instance();
    INFO(validation.summary());
    REQUIRE(validation.errorCount() == 0);
    REQUIRE(validation.warningCount() == 0);
}

TEST_CASE("t06 Graphviz 导出包含 pass、resource 与读写边", "[t06]") {
    {
        p04::AppConfig config;
        config.width = 160;
        config.height = 120;
        config.highDpiFramebuffer = false;
        config.windowVisible = false;
        config.offscreenCapture = true;
        p05::RenderGraphApp app(config);
        app.init();
        const std::string dot = app.graphviz();
        REQUIRE(dot.find("digraph RenderGraph") != std::string::npos);
        REQUIRE(dot.find("ShadowDepth") != std::string::npos);
        REQUIRE(dot.find("Geometry") != std::string::npos);
        REQUIRE(dot.find("Lighting") != std::string::npos);
        REQUIRE(dot.find("Bloom") != std::string::npos);
        REQUIRE(dot.find("Tonemap") != std::string::npos);
        REQUIRE(dot.find("label=\"R\"") != std::string::npos);
        REQUIRE(dot.find("label=\"W\"") != std::string::npos);
    }

    rwb::ValidationLog::instance().reset();
    p04::AppConfig uiConfig;
    uiConfig.width = 320;
    uiConfig.height = 240;
    uiConfig.highDpiFramebuffer = false;
    uiConfig.windowVisible = false;
    uiConfig.offscreenCapture = false;
    p05::RenderGraphApp uiApp(uiConfig);
    uiApp.init();
    uiApp.setGraphOverlayVisible(true);
    REQUIRE(uiApp.deferred().uiInteractionEnabled());
    uiApp.run(2);
    const auto& validation = rwb::ValidationLog::instance();
    INFO(validation.summary());
    REQUIRE(validation.errorCount() == 0);
    REQUIRE(validation.warningCount() == 0);
}
