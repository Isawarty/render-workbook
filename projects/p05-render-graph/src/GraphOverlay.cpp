#include "GraphOverlay.h"

#include "DeferredApp.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace p05 {

GraphOverlay::~GraphOverlay() { shutdown(); }

void GraphOverlay::setVisible(p04::DeferredApp& app, bool visible) {
    m_visible = visible;
    app.setUiInteractionEnabled(visible);
}

void GraphOverlay::init(p04::DeferredApp& app) {
    if (!app.context().window()) return;
    shutdown();
    m_app = &app;
    m_renderPass = app.postRenderPass();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = 1.1f;
    io.IniFilename = nullptr;

    if (!ImGui_ImplGlfw_InitForVulkan(app.context().window()->handle(), true)) {
        shutdown();
        throw std::runtime_error("ImGui GLFW backend 初始化失败");
    }
    m_glfwBackendInitialized = true;
    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = app.context().instance();
    info.PhysicalDevice = app.context().physicalDevice();
    info.Device = app.context().device();
    info.QueueFamily = app.context().queueFamilies().graphics.value();
    info.Queue = app.context().graphicsQueue();
    info.RenderPass = m_renderPass;
    info.MinImageCount = std::max(2u, app.swapchain().imageCount());
    info.ImageCount = std::max(2u, app.swapchain().imageCount());
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.DescriptorPoolSize = 32;
    if (!ImGui_ImplVulkan_Init(&info)) {
        shutdown();
        throw std::runtime_error("ImGui Vulkan backend 初始化失败");
    }
    m_vulkanBackendInitialized = true;
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        shutdown();
        throw std::runtime_error("ImGui 字体纹理上传失败");
    }
    m_initialized = true;
}

void GraphOverlay::shutdown() {
    if (!m_initialized && ImGui::GetCurrentContext() == nullptr) return;
    if (m_app) (void)vkDeviceWaitIdle(m_app->context().device());
    if (m_vulkanBackendInitialized) ImGui_ImplVulkan_Shutdown();
    if (m_glfwBackendInitialized) ImGui_ImplGlfw_Shutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    m_initialized = false;
    m_glfwBackendInitialized = false;
    m_vulkanBackendInitialized = false;
    m_frameReady = false;
    m_app = nullptr;
    m_renderPass = VK_NULL_HANDLE;
}

void GraphOverlay::beginFrame(p04::DeferredApp& app,
                              const rwb::rg::CompiledGraph& graph) {
    if (!app.context().window()) return;
    if (!m_initialized || m_renderPass != app.postRenderPass()) init(app);

    GLFWwindow* window = app.context().window()->handle();
    const bool f1Down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    const bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (f1Down && !m_f1WasDown) {
        setVisible(app, !m_visible);
    }
    if (m_visible && escapeDown && !m_escapeWasDown) {
        setVisible(app, false);
    }
    m_f1WasDown = f1Down;
    m_escapeWasDown = escapeDown;
    if (!m_visible) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawGraph(graph);
    app.setUiInteractionEnabled(m_visible);
    ImGui::Render();
    m_frameReady = true;
}

void GraphOverlay::drawGraph(const rwb::rg::CompiledGraph& graph) {
    ImGui::SetNextWindowSize(ImVec2(520.0f, 430.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.94f);
    if (!ImGui::Begin("P05 Render Graph  (F1 / Esc)", &m_visible)) {
        ImGui::End();
        return;
    }

    std::size_t barrierCount = 0;
    for (const auto& pass : graph.passes()) barrierCount += pass.barriers.size();
    ImGui::Text("%zu passes  |  %zu resources  |  %zu inferred barriers",
                graph.passes().size(), graph.lifetimes().size(), barrierCount);
    ImGui::Separator();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = std::max(80.0f, ImGui::GetContentRegionAvail().x /
                                         std::max(1.0f, static_cast<float>(graph.passes().size())));
    for (std::size_t i = 0; i < graph.passes().size(); ++i) {
        const ImVec2 min(origin.x + i * width + 3.0f, origin.y + 4.0f);
        const ImVec2 max(min.x + width - 10.0f, min.y + 42.0f);
        draw->AddRectFilled(min, max, IM_COL32(46, 92, 135, 230), 5.0f);
        draw->AddRect(min, max, IM_COL32(116, 190, 255, 255), 5.0f);
        draw->AddText(ImVec2(min.x + 7.0f, min.y + 12.0f), IM_COL32_WHITE,
                      graph.passes()[i].name.c_str());
        if (i + 1 < graph.passes().size()) {
            const ImVec2 from(max.x + 2.0f, (min.y + max.y) * 0.5f);
            const ImVec2 to(origin.x + (i + 1) * width + 1.0f, from.y);
            draw->AddLine(from, to, IM_COL32(116, 190, 255, 255), 2.0f);
            draw->AddTriangleFilled(to, ImVec2(to.x - 6.0f, to.y - 4.0f),
                                    ImVec2(to.x - 6.0f, to.y + 4.0f),
                                    IM_COL32(116, 190, 255, 255));
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 58.0f));

    if (ImGui::BeginTable("graph-details", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Depends on");
        ImGui::TableSetupColumn("Barriers");
        ImGui::TableHeadersRow();
        for (const auto& pass : graph.passes()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(pass.name.c_str());
            ImGui::TableSetColumnIndex(1);
            if (pass.dependencies.empty()) ImGui::TextDisabled("root");
            for (std::size_t i = 0; i < pass.dependencies.size(); ++i) {
                if (i) ImGui::SameLine(0.0f, 3.0f);
                ImGui::Text("#%u", pass.dependencies[i].id);
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu", pass.barriers.size());
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Transient lifetimes / alias slots");
    for (const auto& lifetime : graph.lifetimes()) {
        ImGui::Text("resource #%u   [%u, %u]   slot %u", lifetime.resource.id,
                    lifetime.firstUse, lifetime.lastUse, lifetime.physicalSlot);
    }
    ImGui::End();
}

void GraphOverlay::render(VkCommandBuffer commandBuffer) {
    if (!m_frameReady) return;
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    m_frameReady = false;
}

} // namespace p05
