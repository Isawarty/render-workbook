#pragma once

#include "rwb/rendergraph/RenderGraph.h"

#include <volk.h>

namespace p04 { class DeferredApp; }

namespace p05 {

class GraphOverlay {
public:
    GraphOverlay() = default;
    ~GraphOverlay();

    GraphOverlay(const GraphOverlay&) = delete;
    GraphOverlay& operator=(const GraphOverlay&) = delete;

    void init(p04::DeferredApp& app);
    void beginFrame(p04::DeferredApp& app, const rwb::rg::CompiledGraph& graph);
    void render(VkCommandBuffer commandBuffer);
    void setVisible(p04::DeferredApp& app, bool visible);
    bool visible() const { return m_visible; }

private:
    void shutdown();
    void drawGraph(const rwb::rg::CompiledGraph& graph);

    p04::DeferredApp* m_app = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    bool m_initialized = false;
    bool m_glfwBackendInitialized = false;
    bool m_vulkanBackendInitialized = false;
    bool m_visible = false;
    bool m_f1WasDown = false;
    bool m_escapeWasDown = false;
    bool m_frameReady = false;
};

} // namespace p05
