#include "GraphOverlay.h"

#include "rwb/core/Todo.h"

namespace p05 {

GraphOverlay::~GraphOverlay() { shutdown(); }

void GraphOverlay::setVisible(p04::DeferredApp&, bool) {
    RWB_TODO("p05-t06 GraphOverlay::setVisible");
}

void GraphOverlay::init(p04::DeferredApp&) {
    RWB_TODO("p05-t06 GraphOverlay::init");
}

void GraphOverlay::shutdown() {
    m_initialized = false;
    m_glfwBackendInitialized = false;
    m_vulkanBackendInitialized = false;
    m_frameReady = false;
    m_app = nullptr;
    m_renderPass = VK_NULL_HANDLE;
}

void GraphOverlay::beginFrame(p04::DeferredApp&, const rwb::rg::CompiledGraph&) {
    RWB_TODO("p05-t06 GraphOverlay::beginFrame");
}

void GraphOverlay::drawGraph(const rwb::rg::CompiledGraph&) {
    RWB_TODO("p05-t06 GraphOverlay::drawGraph");
}

void GraphOverlay::render(VkCommandBuffer) {
    RWB_TODO("p05-t06 GraphOverlay::render");
}

} // namespace p05
