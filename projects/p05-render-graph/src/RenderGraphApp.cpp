#include "RenderGraphApp.h"
#include "GraphOverlay.h"

#include "rwb/core/Todo.h"

#include <utility>

namespace p05 {

RenderGraphApp::RenderGraphApp(p04::AppConfig config) : m_config(std::move(config)) {
    m_config.title = "render-workbook P05 render graph";
}

RenderGraphApp::~RenderGraphApp() = default;

void RenderGraphApp::init() {
    RWB_TODO("p05-t05 RenderGraphApp::init");
}

void RenderGraphApp::run(int) {
    RWB_TODO("p05-t05 RenderGraphApp::run");
}

rwb::rhi::CapturedImage RenderGraphApp::renderAndCaptureFinal() {
    RWB_TODO("p05-t05 RenderGraphApp::renderAndCaptureFinal");
}

rwb::rhi::CapturedImage RenderGraphApp::renderAndCaptureP04Reference() {
    RWB_TODO("p05-t05 RenderGraphApp::renderAndCaptureP04Reference");
}

void RenderGraphApp::setGraphOverlayVisible(bool) {
    RWB_TODO("p05-t06 RenderGraphApp::setGraphOverlayVisible");
}

void RenderGraphApp::recordGraph(VkCommandBuffer, std::uint32_t) {
    RWB_TODO("p05-t05 RenderGraphApp::recordGraph");
}

void RenderGraphApp::rebuildGraph() {
    RWB_TODO("p05-t05 RenderGraphApp::rebuildGraph");
}

} // namespace p05
