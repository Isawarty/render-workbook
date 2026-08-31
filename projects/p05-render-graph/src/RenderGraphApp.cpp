#include "RenderGraphApp.h"
#include "GraphOverlay.h"

#include <utility>

namespace p05 {
namespace {

rwb::rg::ResourceState imageState(VkPipelineStageFlags stages,
                                  VkAccessFlags access,
                                  VkImageLayout layout) {
    return {stages, access, layout};
}

rwb::rg::ResourceDesc imageDesc(std::string name, std::uint64_t bytes,
                                std::uint64_t compatibility,
                                VkImage image = VK_NULL_HANDLE) {
    rwb::rg::ResourceDesc desc;
    desc.name = std::move(name);
    desc.kind = rwb::rg::ResourceKind::Image;
    desc.sizeBytes = bytes;
    desc.compatibilityKey = compatibility;
    desc.initialState = imageState(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                                   VK_IMAGE_LAYOUT_GENERAL);
    desc.image = image;
    return desc;
}

} // namespace

RenderGraphApp::RenderGraphApp(p04::AppConfig config) : m_config(std::move(config)) {
    m_config.title = "render-workbook P05 render graph";
}

RenderGraphApp::~RenderGraphApp() {
    if (m_deferred) m_deferred->setExternalFrameRecorder({});
}

void RenderGraphApp::init() {
    m_deferred = std::make_unique<p04::DeferredApp>(m_config);
    m_deferred->initUpTo(p04::Stage::Slang);
    if (!m_config.offscreenCapture) {
        m_overlay = std::make_unique<GraphOverlay>();
        m_overlay->init(*m_deferred);
    }
    rebuildGraph();
    m_deferred->setExternalFrameRecorder(
        [this](VkCommandBuffer cmd, std::uint32_t imageIndex) {
            recordGraph(cmd, imageIndex);
        });
}

void RenderGraphApp::run(int frameCount) { m_deferred->run(frameCount); }

rwb::rhi::CapturedImage RenderGraphApp::renderAndCaptureFinal() {
    return m_deferred->renderAndCaptureFinal();
}

rwb::rhi::CapturedImage RenderGraphApp::renderAndCaptureP04Reference() {
    m_deferred->setExternalFrameRecorder({});
    auto frame = m_deferred->renderAndCaptureFinal();
    m_deferred->setExternalFrameRecorder(
        [this](VkCommandBuffer cmd, std::uint32_t imageIndex) {
            recordGraph(cmd, imageIndex);
        });
    return frame;
}

void RenderGraphApp::setGraphOverlayVisible(bool visible) {
    if (m_overlay) m_overlay->setVisible(*m_deferred, visible);
}

void RenderGraphApp::recordGraph(VkCommandBuffer cmd, std::uint32_t imageIndex) {
    const VkExtent2D extent = m_deferred->swapchain().extent();
    if (extent.width != m_graphWidth || extent.height != m_graphHeight) rebuildGraph();

    // 每帧矩阵由 P04 的相机状态生成；pass callback 捕获本帧值，图结构本身保持稳定。
    const p04::FrameMatrices matrices = m_deferred->prepareFrameRecording();
    rwb::rg::RenderGraph frameGraph;
    const std::uint64_t pixels = static_cast<std::uint64_t>(extent.width) * extent.height;
    auto shadow = frameGraph.importResource(imageDesc("ShadowDepth", 256u * 256u * 4u, 1));
    auto gbuffer = frameGraph.createResource(imageDesc("GBuffer", pixels * 16u, 2));
    auto depth = frameGraph.createResource(imageDesc("SceneDepth", pixels * 4u, 3));
    auto hdr = frameGraph.createResource(imageDesc("HDR", pixels * 8u, 4,
                                                    m_deferred->hdrImage().handle));
    auto bloom = frameGraph.createResource(imageDesc("Bloom", pixels * 8u, 4,
                                                      m_deferred->bloomImage().handle));
    auto swapchain = frameGraph.importResource(imageDesc("Swapchain", pixels * 4u, 5));

    frameGraph.addPass("Shadow", [&](rwb::rg::PassBuilder& pass) {
        pass.write(shadow, imageState(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
    }, [this, matrices](VkCommandBuffer commandBuffer) {
        m_deferred->recordShadowPass(commandBuffer, matrices);
    });
    frameGraph.addPass("Geometry", [&](rwb::rg::PassBuilder& pass) {
        pass.write(gbuffer, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        pass.write(depth, imageState(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
    }, [this, imageIndex, matrices](VkCommandBuffer commandBuffer) {
        m_deferred->recordGeometryPass(commandBuffer, imageIndex, matrices);
    });
    frameGraph.addPass("Lighting", [&](rwb::rg::PassBuilder& pass) {
        pass.read(gbuffer, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                      VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        pass.read(depth, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
        pass.read(shadow, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
        pass.write(hdr, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_IMAGE_LAYOUT_GENERAL));
    }, [this](VkCommandBuffer commandBuffer) {
        m_deferred->recordLightingPass(commandBuffer);
    });
    frameGraph.addPass("Bloom", [&](rwb::rg::PassBuilder& pass) {
        pass.read(hdr, imageState(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_IMAGE_LAYOUT_GENERAL));
        pass.write(bloom, imageState(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_GENERAL));
    }, [this](VkCommandBuffer commandBuffer) {
        m_deferred->recordBloomPass(commandBuffer, false);
    });
    frameGraph.addPass("Tonemap", [&](rwb::rg::PassBuilder& pass) {
        pass.read(hdr, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_IMAGE_LAYOUT_GENERAL));
        pass.read(bloom, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT,
                                    VK_IMAGE_LAYOUT_GENERAL));
        pass.write(swapchain, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    }, [this, imageIndex](VkCommandBuffer commandBuffer) {
        if (m_overlay) m_overlay->beginFrame(*m_deferred, m_compiled);
        m_deferred->recordTonemapBegin(commandBuffer, imageIndex);
        if (m_overlay) m_overlay->render(commandBuffer);
        m_deferred->recordTonemapEnd(commandBuffer);
    });
    frameGraph.compile().execute(cmd);
}

void RenderGraphApp::rebuildGraph() {
    const VkExtent2D extent = m_deferred->swapchain().extent();
    m_graphWidth = extent.width;
    m_graphHeight = extent.height;
    const std::uint64_t pixels = static_cast<std::uint64_t>(extent.width) * extent.height;
    rwb::rg::RenderGraph graph;
    auto shadow = graph.importResource(imageDesc("ShadowDepth", 256u * 256u * 4u, 1));
    auto gbuffer = graph.createResource(imageDesc("GBuffer", pixels * 16u, 2));
    auto depth = graph.createResource(imageDesc("SceneDepth", pixels * 4u, 3));
    auto hdr = graph.createResource(imageDesc("HDR", pixels * 8u, 4));
    auto bloom = graph.createResource(imageDesc("Bloom", pixels * 8u, 4));
    auto swapchain = graph.importResource(imageDesc("Swapchain", pixels * 4u, 5));
    graph.addPass("Shadow", [&](rwb::rg::PassBuilder& pass) {
        pass.write(shadow, imageState(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
    });
    graph.addPass("Geometry", [&](rwb::rg::PassBuilder& pass) {
        pass.write(gbuffer, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        pass.write(depth, imageState(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
    });
    graph.addPass("Lighting", [&](rwb::rg::PassBuilder& pass) {
        pass.read(gbuffer, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                      VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        pass.read(depth, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
        pass.read(shadow, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
        pass.write(hdr, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_IMAGE_LAYOUT_GENERAL));
    });
    graph.addPass("Bloom", [&](rwb::rg::PassBuilder& pass) {
        pass.read(hdr, imageState(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_IMAGE_LAYOUT_GENERAL));
        pass.write(bloom, imageState(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_GENERAL));
    });
    graph.addPass("Tonemap", [&](rwb::rg::PassBuilder& pass) {
        pass.read(hdr, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_IMAGE_LAYOUT_GENERAL));
        pass.read(bloom, imageState(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_ACCESS_SHADER_READ_BIT,
                                    VK_IMAGE_LAYOUT_GENERAL));
        pass.write(swapchain, imageState(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    });
    m_compiled = graph.compile();
}

} // namespace p05
