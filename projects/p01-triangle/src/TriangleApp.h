#pragma once

#include "rwb/platform/Window.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace p01 {

// Window 定义在 rwb 命名空间里（engine/include/rwb/platform/Window.h）
using rwb::Window;

// P1 的初始化被切成 9 段，正好对应 9 个 task。
// 每个 task 只需要让「到自己这一段为止」的初始化跑通。
//
// 测试因此可以精确地只驱动到某一段，比如 t03 的测试只 initUpTo(Stage::Swapchain)，
// 不会因为 t05 还没写而失败。
enum class Stage {
    Instance      = 1,   // t01: instance + validation layer + debug messenger
    Device        = 2,   // t02: surface + physical device + queue family + logical device
    Swapchain     = 3,   // t03: swapchain + image views
    RenderPass    = 4,   // t04: render pass + framebuffer
    Pipeline      = 5,   // t05: shader module + graphics pipeline
    Commands      = 6,   // t06: command pool + command buffer 录制
    Sync          = 7,   // t07: semaphore/fence + 提交 + present
    Resize        = 8,   // t08: swapchain 重建
    FramesInFlight = 9,  // t09: 多帧并行
};

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;

    bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

struct AppConfig {
    std::uint32_t width           = 800;
    std::uint32_t height          = 600;
    bool          enableValidation = true;
    bool          headless        = false;   // 不创建窗口时用于纯 instance/device 测试
    bool          highDpiFramebuffer = true;
    std::string   title           = "render-workbook P01 triangle";
};

class TriangleApp {
public:
    explicit TriangleApp(AppConfig config = {});
    ~TriangleApp();

    TriangleApp(const TriangleApp&)            = delete;
    TriangleApp& operator=(const TriangleApp&) = delete;

    // 按顺序执行各段初始化，直到（含）stage 为止。
    void initUpTo(Stage stage);

    // 渲染 frameCount 帧后返回。frameCount < 0 表示一直渲染到窗口关闭。
    void run(int frameCount = -1);

    // 渲染一帧并把结果读回内存（8-bit RGBA）。L3 golden 测试用。
    // 这段是框架提供的：截图涉及的 staging buffer / layout transition 属于 P2 教学点，
    // 在 P1 阶段直接给你，不作为考点。
    struct CapturedFrame {
        std::uint32_t             width  = 0;
        std::uint32_t             height = 0;
        std::vector<std::uint8_t> pixels;
    };
    CapturedFrame renderAndCapture();

    // --- 供测试检查内部状态 --------------------------------------------
    VkInstance         instance()          const { return m_instance; }
    VkDebugUtilsMessengerEXT debugMessenger() const { return m_debugMessenger; }
    VkPhysicalDevice   physicalDevice()    const { return m_physicalDevice; }
    VkDevice           device()            const { return m_device; }
    VkSurfaceKHR       surface()           const { return m_surface; }
    VkSwapchainKHR     swapchain()         const { return m_swapchain; }
    VkRenderPass       renderPass()        const { return m_renderPass; }
    VkPipeline         pipeline()          const { return m_pipeline; }
    VkCommandPool      commandPool()       const { return m_commandPool; }
    VkExtent2D         swapchainExtent()   const { return m_swapchainExtent; }
    VkFormat           swapchainFormat()   const { return m_swapchainFormat; }
    const QueueFamilyIndices& queueFamilies() const { return m_queueFamilies; }
    const std::vector<VkImage>&     swapchainImages()      const { return m_swapchainImages; }
    const std::vector<VkImageView>& swapchainImageViews()  const { return m_swapchainImageViews; }
    const std::vector<VkFramebuffer>& framebuffers()       const { return m_framebuffers; }
    const std::vector<VkCommandBuffer>& commandBuffers()   const { return m_commandBuffers; }
    std::uint32_t      framesInFlight()    const { return m_framesInFlight; }
    std::string        physicalDeviceName() const;
    Window*            window()            const { return m_window.get(); }

    // t08 用：强制下一帧走 swapchain 重建路径
    void requestResize() { m_framebufferResized = true; }
    std::uint32_t swapchainGeneration() const { return m_swapchainGeneration; }

    static constexpr std::uint32_t kMaxFramesInFlight = 2;

private:
    // --- t01 ---------------------------------------------------------
    void createInstance();
    void setupDebugMessenger();
    std::vector<const char*> requiredInstanceExtensions() const;
    bool validationLayerSupported() const;

    // --- t02 ---------------------------------------------------------
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool deviceExtensionsSupported(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;

    // --- t03 ---------------------------------------------------------
    void createSwapchain();
    void createImageViews();

    // --- t04 ---------------------------------------------------------
    void createRenderPass();
    void createFramebuffers();

    // --- t05 ---------------------------------------------------------
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<std::uint32_t>& code) const;

    // --- t06 ---------------------------------------------------------
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);

    // --- t07 / t09 ---------------------------------------------------
    void createSyncObjects();
    void drawFrame();

    // --- t08 ---------------------------------------------------------
    void recreateSwapchain();
    void cleanupSwapchain();

    // --- t09 ---------------------------------------------------------
public:
    // 把「同时在飞的帧数」调到 n，并重建对应的 per-frame 资源。
    void setFramesInFlight(std::uint32_t n);
private:

    // --- 框架提供 ------------------------------------------------------
    void cleanup() noexcept;
    void destroySwapchainFallback();
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    AppConfig m_config;
    std::unique_ptr<Window> m_window;

    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;

    VkPhysicalDevice   m_physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilies;
    VkDevice           m_device        = VK_NULL_HANDLE;
    VkQueue            m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue            m_presentQueue  = VK_NULL_HANDLE;

    VkSwapchainKHR             m_swapchain       = VK_NULL_HANDLE;
    VkFormat                   m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D                 m_swapchainExtent{};
    std::vector<VkImage>       m_swapchainImages;
    std::vector<VkImageView>   m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::uint32_t              m_swapchainGeneration = 0;

    VkRenderPass     m_renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;

    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;

    std::uint32_t m_framesInFlight     = 1;   // t09 之前是 1（单帧串行）
    std::uint32_t m_currentFrame       = 0;
    bool          m_framebufferResized = false;
    Stage         m_reachedStage       = static_cast<Stage>(0);
};

} // namespace p01
