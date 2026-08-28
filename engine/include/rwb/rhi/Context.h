#pragma once

#include "rwb/platform/Window.h"
#include "rwb/rhi/Vk.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// VMA 的句柄。这里前向声明，免得把 vk_mem_alloc.h（一万多行）拖进每个 include 它的地方。
// 真要用 vmaCreateBuffer 之类的 API 时，在 .cpp 里自己 include vk_mem_alloc.h。
VK_DEFINE_HANDLE(VmaAllocator)

namespace rwb::rhi {

// ---------------------------------------------------------------------------
// Context —— 一台机器上「Vulkan 的一次会话」。
//
// ## 为什么这段代码不再是挖空题
//
// 这里面每一行你都在 P1 亲手写过：instance、validation layer、debug messenger、
// surface、物理设备打分、队列族查找、逻辑设备、portability subset。P2 起它变成框架，
// 因为再写第四遍不会让你多懂任何东西，只会让 P4 的 G-Buffer 排到三周之后。
//
// 但它是「你写过的代码的清理版」，不是黑盒。建议做 P2-t01 之前先花二十分钟
// 把 Context.cpp 和你自己的 `mine/p01-t02` 对着读一遍，差异都在注释里解释了。
//
// 相对 P1 的三处实质改动：
//   1. 队列族多找一个「专用 compute 队列」（P3-t06 的跨队列同步要用）
//   2. 设备特性变成显式开关（P1 里是空的 VkPhysicalDeviceFeatures{}）
//   3. 接管 VMA allocator 的创建 —— 创建不是教学点，「怎么用」才是（P2-t01）
// ---------------------------------------------------------------------------

// 逻辑设备要开启的特性。默认全关：Vulkan 的规矩是没显式声明的能力用了就是 UB，
// 而 validation layer 会替你抓这件事 —— 这正是 L1 判分能发现的典型错误。
struct DeviceFeatures {
    bool samplerAnisotropy = false;   // P2-t04 各向异性过滤
    bool sampleRateShading = false;   // P2-t08 逐采样着色（MSAA 质量）
    bool fillModeNonSolid  = false;   // 线框模式，调试用
    bool independentBlend  = false;   // P4 多附件各自不同的混合状态
    bool shaderInt64       = false;
    bool multiDrawIndirect = false;   // P3-t07 / GPU-driven 方向
};

struct ContextConfig {
    std::uint32_t width  = 800;
    std::uint32_t height = 600;
    std::string   title  = "render-workbook";

    // headless = 不建窗口、不建 surface、不要求 swapchain 扩展。
    // P3（纯 compute）全程用它；它也是 CI 上最省事的模式。
    bool headless         = false;
    bool enableValidation = true;

    // 必须有的特性：设备不支持就直接把它淘汰掉（宁可选不出设备也不静默降级）
    DeviceFeatures           features;
    // 有则用、没有就算了的特性。典型是各向异性过滤：
    // NVIDIA / Apple Silicon 上都有，但 lavapipe 这种纯软件渲染器可能没有，
    // 而「CI 跑不了」不该成为「这门课不能教各向异性」的理由。
    // 用之前查 Context::enabledFeatures()，别假设它开了。
    DeviceFeatures           optionalFeatures;
    std::vector<const char*> extraDeviceExtensions;
};

// 队列族索引。graphics/present 同 P1；compute 是新增的。
struct QueueFamilies {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;
    // 「专用」compute 队列族：支持 COMPUTE 但不支持 GRAPHICS。
    // 有独立 DMA/compute 引擎的硬件上，用它才能真正和图形工作并行。
    // 找不到时回退到 graphics 族 —— 规范保证 graphics 族必然支持 compute。
    std::optional<std::uint32_t> compute;

    bool complete() const { return graphics.has_value() && present.has_value(); }
    bool hasDedicatedCompute() const {
        return compute.has_value() && graphics.has_value() && *compute != *graphics;
    }
};

class Context {
public:
    explicit Context(ContextConfig config = {});
    ~Context();

    Context(const Context&)            = delete;
    Context& operator=(const Context&) = delete;

    // --- 句柄 ------------------------------------------------------------
    VkInstance       instance()       const { return m_instance; }
    VkSurfaceKHR     surface()        const { return m_surface; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice         device()         const { return m_device; }
    VmaAllocator     allocator()      const { return m_allocator; }
    Window*          window()         const { return m_window.get(); }

    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue()  const { return m_presentQueue; }
    VkQueue computeQueue()  const { return m_computeQueue; }
    const QueueFamilies& queueFamilies() const { return m_queueFamilies; }

    // 图形队列族的 command pool。带 RESET_COMMAND_BUFFER，可以逐个 buffer 重录。
    VkCommandPool commandPool() const { return m_commandPool; }

    const ContextConfig& config() const { return m_config; }

    // --- 查询 ------------------------------------------------------------
    std::string deviceName() const;
    const VkPhysicalDeviceProperties& properties() const { return m_properties; }
    // P3-t02 的 subgroup 归约要读 subgroupSize / supportedOperations。
    // MoltenVK 上这里的能力集明显更窄，那一题的降级路径就是照它判断的。
    const VkPhysicalDeviceSubgroupProperties& subgroupProperties() const { return m_subgroup; }

    // 实际在 vkCreateDevice 时开启了哪些特性。
    // optionalFeatures 里的东西可能没开成，用之前必须查这里 ——
    // 没开就用等于未定义行为，validation layer 会替你抓住，但那已经是 L1 判失败了。
    const VkPhysicalDeviceFeatures& enabledFeatures() const { return m_enabledFeatures; }

    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags props) const;

    // 从候选里挑第一个满足 tiling+features 的格式。深度格式选择会用到（P2-t05）。
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features) const;

    // --- 一次性提交 --------------------------------------------------------
    // 录一个 ONE_TIME_SUBMIT 的 command buffer，提交到图形队列，等它做完。
    //
    // staging 上传、layout transition、mipmap 生成全都靠它。
    // 它是同步等待的 —— 教学期这样最容易推理；真实引擎会用 transfer 队列 + 异步 fence。
    void immediateSubmit(const std::function<void(VkCommandBuffer)>& record) const;

    // 等 GPU 全部空闲。销毁资源前必须调。
    void waitIdle() const;

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();
    void createCommandPool();
    void cleanup() noexcept;

    QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
    bool          deviceSuitable(VkPhysicalDevice device) const;
    std::vector<const char*> requiredDeviceExtensions() const;

    ContextConfig           m_config;
    std::unique_ptr<Window> m_window;

    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;

    VkPhysicalDevice                   m_physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties         m_properties{};
    VkPhysicalDeviceSubgroupProperties m_subgroup{};
    VkPhysicalDeviceFeatures           m_enabledFeatures{};

    QueueFamilies m_queueFamilies;
    VkDevice      m_device        = VK_NULL_HANDLE;
    VkQueue       m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue       m_presentQueue  = VK_NULL_HANDLE;
    VkQueue       m_computeQueue  = VK_NULL_HANDLE;

    VmaAllocator  m_allocator   = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
};

} // namespace rwb::rhi
