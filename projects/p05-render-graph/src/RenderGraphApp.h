#pragma once

#include "DeferredApp.h"
#include "rwb/rendergraph/RenderGraph.h"

#include <cstdint>
#include <memory>
#include <string>

namespace p05 {

class GraphOverlay;

class RenderGraphApp {
public:
    explicit RenderGraphApp(p04::AppConfig config = {});
    ~RenderGraphApp();

    RenderGraphApp(const RenderGraphApp&) = delete;
    RenderGraphApp& operator=(const RenderGraphApp&) = delete;

    void init();
    void run(int frameCount = -1);
    rwb::rhi::CapturedImage renderAndCaptureFinal();
    rwb::rhi::CapturedImage renderAndCaptureP04Reference();
    void setGraphOverlayVisible(bool visible);

    const rwb::rg::CompiledGraph& compiledGraph() const { return m_compiled; }
    std::string graphviz() const { return m_compiled.toDot(); }
    p04::DeferredApp& deferred() { return *m_deferred; }

private:
    void rebuildGraph();
    void recordGraph(VkCommandBuffer cmd, std::uint32_t imageIndex);

    p04::AppConfig m_config;
    std::unique_ptr<p04::DeferredApp> m_deferred;
    std::unique_ptr<GraphOverlay> m_overlay;
    rwb::rg::CompiledGraph m_compiled;
    std::uint32_t m_graphWidth = 0;
    std::uint32_t m_graphHeight = 0;
};

} // namespace p05
