#include "TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"
#include "rwb/core/ValidationLog.h"

#include <stdexcept>

namespace p01 {

TriangleApp::TriangleApp(AppConfig config) : m_config(config) {
    if (volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error(
            "volkInitialize 失败：找不到 Vulkan loader。先跑 p00_setup 检查环境。");
    }
    if (!m_config.headless) {
        m_window = std::make_unique<Window>(m_config.width, m_config.height, m_config.title,
                                            m_config.highDpiFramebuffer);
    }
}

TriangleApp::~TriangleApp() { cleanup(); }

void TriangleApp::initUpTo(Stage stage) {
    const int target = static_cast<int>(stage);
    auto reached = [&](Stage s) { return static_cast<int>(m_reachedStage) >= static_cast<int>(s); };
    auto advance = [&](Stage s) { m_reachedStage = s; };

    if (target >= 1 && !reached(Stage::Instance)) {
        createInstance();
        volkLoadInstance(m_instance);      // instance 级函数指针
        setupDebugMessenger();
        advance(Stage::Instance);
    }
    if (target >= 2 && !reached(Stage::Device)) {
        // surface 要在挑设备之前建好：present 支持是选设备的判据之一
        if (m_window) createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        volkLoadDevice(m_device);          // device 级函数指针（跳过 loader 派发，更快）
        advance(Stage::Device);
    }
    if (target >= 3 && !reached(Stage::Swapchain)) {
        createSwapchain();
        createImageViews();
        advance(Stage::Swapchain);
    }
    if (target >= 4 && !reached(Stage::RenderPass)) {
        createRenderPass();
        createFramebuffers();
        advance(Stage::RenderPass);
    }
    if (target >= 5 && !reached(Stage::Pipeline)) {
        createGraphicsPipeline();
        advance(Stage::Pipeline);
    }
    if (target >= 6 && !reached(Stage::Commands)) {
        createCommandPool();
        createCommandBuffers();
        advance(Stage::Commands);
    }
    if (target >= 7 && !reached(Stage::Sync)) {
        createSyncObjects();
        advance(Stage::Sync);
    }
    if (target >= 8 && !reached(Stage::Resize)) {
        // t08 本身不新建资源，它让 drawFrame 具备重建 swapchain 的能力。
        // 到这一步只是把「已支持」这件事记下来。
        advance(Stage::Resize);
    }
    if (target >= 9 && !reached(Stage::FramesInFlight)) {
        setFramesInFlight(kMaxFramesInFlight);
        advance(Stage::FramesInFlight);
    }
}

void TriangleApp::run(int frameCount) {
    int drawn = 0;
    while (true) {
        if (m_window) {
            if (m_window->shouldClose()) break;
            m_window->pollEvents();
            if (m_window->consumeResizeFlag()) m_framebufferResized = true;
            if (m_window->isMinimized()) continue;
        }
        drawFrame();
        ++drawn;
        if (frameCount >= 0 && drawn >= frameCount) break;
    }
    if (m_device) vkDeviceWaitIdle(m_device);
}

std::string TriangleApp::physicalDeviceName() const {
    if (m_physicalDevice == VK_NULL_HANDLE) return {};
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    return props.deviceName;
}

std::uint32_t TriangleApp::findMemoryType(std::uint32_t typeFilter,
                                          VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeOk = (typeFilter & (1u << i)) != 0;
        const bool propOk = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeOk && propOk) return i;
    }
    throw std::runtime_error("找不到合适的显存类型");
}

// 框架内部的 swapchain 拆除。
// 只在你的 cleanupSwapchain() 还没实现时兜底 —— 一旦你写好了 t08，走的就是你的版本。
void TriangleApp::destroySwapchainFallback() {
    for (VkFramebuffer fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();
    for (VkImageView v : m_swapchainImageViews) vkDestroyImageView(m_device, v, nullptr);
    m_swapchainImageViews.clear();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_swapchainImages.clear();
}

// 析构函数隐式 noexcept：这里抛出任何异常都会直接 std::terminate() -> abort()。
// 而 cleanupSwapchain() 是 t08 的挖空函数，在你做到 t08 之前它一定会抛。
// 所以这里必须兜住，否则 t03~t07 期间每次退出都是一个 abort 弹窗而不是可读的报错。
void TriangleApp::cleanup() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

        for (VkSemaphore s : m_renderFinishedSemaphores) vkDestroySemaphore(m_device, s, nullptr);
        for (VkSemaphore s : m_imageAvailableSemaphores) vkDestroySemaphore(m_device, s, nullptr);
        for (VkFence f : m_inFlightFences)               vkDestroyFence(m_device, f, nullptr);
        m_renderFinishedSemaphores.clear();
        m_imageAvailableSemaphores.clear();
        m_inFlightFences.clear();

        if (m_commandPool != VK_NULL_HANDLE) {
            // command buffer 随 pool 一起销毁，不用单独释放
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
            m_commandBuffers.clear();
        }

        try {
            cleanupSwapchain();
        } catch (const rwb::NotImplemented&) {
            destroySwapchainFallback();
        } catch (const std::exception& e) {
            rwb::logError(rwb::format("cleanupSwapchain 抛异常: %s", e.what()));
            destroySwapchainFallback();
        }

        if (m_pipeline != VK_NULL_HANDLE)       vkDestroyPipeline(m_device, m_pipeline, nullptr);
        if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_renderPass != VK_NULL_HANDLE)     vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_pipeline       = VK_NULL_HANDLE;
        m_pipelineLayout = VK_NULL_HANDLE;
        m_renderPass     = VK_NULL_HANDLE;

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_debugMessenger != VK_NULL_HANDLE) {
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
            m_debugMessenger = VK_NULL_HANDLE;
        }
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_window.reset();
}

} // namespace p01
