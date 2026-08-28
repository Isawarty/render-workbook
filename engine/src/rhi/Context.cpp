// Context —— 你在 P1 写过的初始化代码的清理版。
//
// 逐段对照 `git diff mine/p01-t02 -- projects/p01-triangle/src/steps/` 会发现
// 骨架完全一样，差别只在三处（见 Context.h 顶部注释）。
#include "rwb/rhi/Context.h"
#include "rwb/rhi/VmaUsage.h"

#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace rwb::rhi {
namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*userData*/) {

    const char* text = (data && data->pMessage) ? data->pMessage : "";
    // 原样转交；分类与判分策略在 ValidationLog 里。和 P1-t01 你写的那份一致。
    ValidationLog::instance().record(severity, types, text);

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)        logError(text);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) logWarn(text);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo() {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    return info;
}

bool hasInstanceExtension(const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
    return std::any_of(available.begin(), available.end(), [name](const VkExtensionProperties& e) {
        return std::strcmp(e.extensionName, name) == 0;
    });
}

bool hasLayerExtension(const char* layer, const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(layer, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(layer, &count, available.data());
    return std::any_of(available.begin(), available.end(), [name](const VkExtensionProperties& e) {
        return std::strcmp(e.extensionName, name) == 0;
    });
}

bool hasDeviceExtension(VkPhysicalDevice device, const char* name) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    return std::any_of(available.begin(), available.end(), [name](const VkExtensionProperties& e) {
        return std::strcmp(e.extensionName, name) == 0;
    });
}

bool validationLayerAvailable() {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& l) {
        return std::strcmp(l.layerName, kValidationLayer) == 0;
    });
}

} // namespace

// ---------------------------------------------------------------------------

Context::Context(ContextConfig config) : m_config(std::move(config)) {
    if (volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error(
            "volkInitialize 失败: 找不到 Vulkan loader。先跑 p00_setup 检查环境。");
    }
    if (!m_config.headless) {
        m_window = std::make_unique<Window>(m_config.width, m_config.height, m_config.title);
    }

    try {
        createInstance();
        volkLoadInstance(m_instance);
        setupDebugMessenger();
        if (m_window) createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        volkLoadDevice(m_device);
        createAllocator();
        createCommandPool();
    } catch (...) {
        // 构造函数抛异常时析构函数「不会」被调用，已经建好的那一半资源就漏了。
        // 手动收尾之后再往上抛。
        cleanup();
        throw;
    }
}

Context::~Context() { cleanup(); }

void Context::createInstance() {
    if (m_config.enableValidation && !validationLayerAvailable()) {
        throw std::runtime_error(
            "请求了 validation layer 但系统里没有。\n"
            "  它来自 Vulkan SDK，不是显卡驱动自带的。先跑 p00_setup 确认。");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = m_config.title.c_str();
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName        = "render-workbook";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    std::vector<const char*> extensions =
        m_window ? Window::requiredInstanceExtensions() : std::vector<const char*>{};
    bool useLayerSettings = false;
    bool useValidationFeatures = false;
    if (m_config.enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (m_config.enableSyncValidation) {
            useLayerSettings =
                hasLayerExtension(kValidationLayer, VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
            useValidationFeatures =
                !useLayerSettings &&
                hasLayerExtension(kValidationLayer, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
            if (!useLayerSettings && !useValidationFeatures) {
                throw std::runtime_error(
                    "请求了 synchronization validation，但 validation layer 不支持 "
                    "VK_EXT_layer_settings 或 VK_EXT_validation_features；请更新 Vulkan SDK。");
            }
            extensions.push_back(useLayerSettings ? VK_EXT_LAYER_SETTINGS_EXTENSION_NAME
                                                  : VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }
    }

    VkInstanceCreateFlags flags = 0;
    if (hasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        // macOS / MoltenVK：不声明接受 portability 设备，vkCreateInstance 直接
        // 返回 VK_ERROR_INCOMPATIBLE_DRIVER。P1-t01 讲过。
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags                   = flags;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugInfo = makeDebugMessengerInfo();

    // 新 layer 优先用 VK_EXT_layer_settings；Ubuntu LTS 等仍可能只提供旧的
    // VK_EXT_validation_features。两条路径都真正开启 sync validation，不能静默降级。
    const VkBool32 enableSync = VK_TRUE;
    VkLayerSettingEXT syncSetting{};
    syncSetting.pLayerName   = kValidationLayer;
    syncSetting.pSettingName = "validate_sync";
    syncSetting.type         = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
    syncSetting.valueCount   = 1;
    syncSetting.pValues      = &enableSync;

    VkLayerSettingsCreateInfoEXT layerSettings{};
    layerSettings.sType        = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    layerSettings.settingCount = 1;
    layerSettings.pSettings    = &syncSetting;

    const VkValidationFeatureEnableEXT syncFeature =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount = 1;
    validationFeatures.pEnabledValidationFeatures    = &syncFeature;

    if (m_config.enableValidation) {
        createInfo.enabledLayerCount   = 1;
        createInfo.ppEnabledLayerNames = &kValidationLayer;
        // 两个结构都挂在 instance 的 pNext 链上。
        if (useLayerSettings) {
            layerSettings.pNext = &debugInfo;
            createInfo.pNext = &layerSettings;
        } else if (useValidationFeatures) {
            validationFeatures.pNext = &debugInfo;
            createInfo.pNext = &validationFeatures;
        } else {
            createInfo.pNext = &debugInfo;             // 覆盖 create/destroy instance 本身
        }
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
}

void Context::setupDebugMessenger() {
    if (!m_config.enableValidation) return;
    const VkDebugUtilsMessengerCreateInfoEXT info = makeDebugMessengerInfo();
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &info, nullptr, &m_debugMessenger));
}

void Context::createSurface() { m_surface = m_window->createSurface(m_instance); }

std::vector<const char*> Context::requiredDeviceExtensions() const {
    std::vector<const char*> exts = m_config.extraDeviceExtensions;
    if (!m_config.headless) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    return exts;
}

QueueFamilies Context::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilies indices;

    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (std::uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount == 0) continue;
        const VkQueueFlags f = families[i].queueFlags;

        if ((f & VK_QUEUE_GRAPHICS_BIT) && !indices.graphics.has_value()) indices.graphics = i;

        // 专用 compute 队列族 = 支持 COMPUTE 但不支持 GRAPHICS。
        // NVIDIA/AMD 上通常存在，集显和 MoltenVK 上通常没有 —— 所以下面必须有回退。
        if ((f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) &&
            !indices.compute.has_value()) {
            indices.compute = i;
        }

        if (m_surface != VK_NULL_HANDLE) {
            VkBool32 presentSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupported);
            if (presentSupported && !indices.present.has_value()) indices.present = i;
        }
    }

    if (m_surface == VK_NULL_HANDLE && indices.graphics.has_value()) {
        indices.present = indices.graphics;    // headless: 不需要 present 能力
    }
    if (!indices.compute.has_value()) {
        // 规范保证：任何支持 GRAPHICS 的队列族必然也支持 COMPUTE。
        indices.compute = indices.graphics;
    }
    return indices;
}

bool Context::deviceSuitable(VkPhysicalDevice device) const {
    if (!findQueueFamilies(device).complete()) return false;

    for (const char* required : requiredDeviceExtensions()) {
        if (!hasDeviceExtension(device, required)) return false;
    }

    if (m_surface != VK_NULL_HANDLE) {
        std::uint32_t formats = 0, modes = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formats, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modes, nullptr);
        if (formats == 0 || modes == 0) return false;
    }

    // 请求了设备不支持的特性时，早点在这里淘汰它，
    // 好过在 vkCreateDevice 里拿一个语焉不详的 VK_ERROR_FEATURE_NOT_PRESENT。
    VkPhysicalDeviceFeatures have{};
    vkGetPhysicalDeviceFeatures(device, &have);
    const DeviceFeatures& want = m_config.features;
    if (want.samplerAnisotropy && !have.samplerAnisotropy) return false;
    if (want.sampleRateShading && !have.sampleRateShading) return false;
    if (want.fillModeNonSolid  && !have.fillModeNonSolid)  return false;
    if (want.independentBlend  && !have.independentBlend)  return false;
    if (want.shaderInt64       && !have.shaderInt64)       return false;
    if (want.multiDrawIndirect && !have.multiDrawIndirect) return false;
    if (want.fragmentStoresAndAtomics && !have.fragmentStoresAndAtomics) return false;

    return true;
}

void Context::pickPhysicalDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("找不到任何支持 Vulkan 的物理设备");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // 打分挑选，优先独显。别无脑选第一个：很多机器上核显排在独显前面。
    int              bestScore = -1;
    VkPhysicalDevice best      = VK_NULL_HANDLE;
    for (VkPhysicalDevice device : devices) {
        if (!deviceSuitable(device)) continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)        score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        score += static_cast<int>(props.limits.maxImageDimension2D / 1024);

        if (score > bestScore) { bestScore = score; best = device; }
    }
    if (best == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "有物理设备，但没有一个同时满足所需的队列 / 扩展 / 特性。\n"
            "  请求的特性写在 ContextConfig::features 里，检查是不是要多了。");
    }

    m_physicalDevice = best;
    m_queueFamilies  = findQueueFamilies(best);
    vkGetPhysicalDeviceProperties(best, &m_properties);

    // subgroup 能力要走 properties2 链。P3-t02 判断降级路径靠它。
    m_subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &m_subgroup;
    vkGetPhysicalDeviceProperties2(best, &props2);

    logInfo(format("选中 GPU: %s (subgroupSize=%u%s)", m_properties.deviceName,
                   m_subgroup.subgroupSize,
                   m_queueFamilies.hasDedicatedCompute() ? ", 有专用 compute 队列" : ""));
}

void Context::createLogicalDevice() {
    const std::set<std::uint32_t> uniqueFamilies = {
        m_queueFamilies.graphics.value(),
        m_queueFamilies.present.value(),
        m_queueFamilies.compute.value(),
    };

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    for (std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &queuePriority;
        queueInfos.push_back(qi);
    }

    const DeviceFeatures& want = m_config.features;
    VkPhysicalDeviceFeatures have{};
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &have);

    VkPhysicalDeviceFeatures features{};
    // 必须有的：能走到这里说明 deviceSuitable 已经确认过了，直接开。
    features.samplerAnisotropy = want.samplerAnisotropy ? VK_TRUE : VK_FALSE;
    features.sampleRateShading = want.sampleRateShading ? VK_TRUE : VK_FALSE;
    features.fillModeNonSolid  = want.fillModeNonSolid  ? VK_TRUE : VK_FALSE;
    features.independentBlend  = want.independentBlend  ? VK_TRUE : VK_FALSE;
    features.shaderInt64       = want.shaderInt64       ? VK_TRUE : VK_FALSE;
    features.multiDrawIndirect = want.multiDrawIndirect ? VK_TRUE : VK_FALSE;
    features.fragmentStoresAndAtomics = want.fragmentStoresAndAtomics ? VK_TRUE : VK_FALSE;

    // 可选的：设备支持才开。调用方必须查 enabledFeatures() 才能用。
    const DeviceFeatures& opt = m_config.optionalFeatures;
    if (opt.samplerAnisotropy && have.samplerAnisotropy) features.samplerAnisotropy = VK_TRUE;
    if (opt.sampleRateShading && have.sampleRateShading) features.sampleRateShading = VK_TRUE;
    if (opt.fillModeNonSolid  && have.fillModeNonSolid)  features.fillModeNonSolid  = VK_TRUE;
    if (opt.independentBlend  && have.independentBlend)  features.independentBlend  = VK_TRUE;
    if (opt.shaderInt64       && have.shaderInt64)       features.shaderInt64       = VK_TRUE;
    if (opt.multiDrawIndirect && have.multiDrawIndirect) features.multiDrawIndirect = VK_TRUE;
    if (opt.fragmentStoresAndAtomics && have.fragmentStoresAndAtomics) {
        features.fragmentStoresAndAtomics = VK_TRUE;
    }

    m_enabledFeatures = features;

    std::vector<const char*> extensions = requiredDeviceExtensions();
    if (hasDeviceExtension(m_physicalDevice, "VK_KHR_portability_subset")) {
        // 规范规定：设备上报了它就必须开启，否则行为未定义。
        extensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<std::uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, m_queueFamilies.graphics.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilies.present.value(),  0, &m_presentQueue);
    vkGetDeviceQueue(m_device, m_queueFamilies.compute.value(),  0, &m_computeQueue);
}

void Context::createAllocator() {
    // volk 之下必须显式把这两个入口给 VMA，它才能自己把剩下的函数取齐。
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo info{};
    info.vulkanApiVersion = VK_API_VERSION_1_2;
    info.physicalDevice   = m_physicalDevice;
    info.device           = m_device;
    info.instance         = m_instance;
    info.pVulkanFunctions = &functions;

    VK_CHECK(vmaCreateAllocator(&info, &m_allocator));
}

void Context::createCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // RESET_COMMAND_BUFFER: 允许逐个 buffer 调 vkBeginCommandBuffer 重录，
    // 而不必整池 vkResetCommandPool。每帧重录时要的就是这个。
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = m_queueFamilies.graphics.value();
    VK_CHECK(vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool));
}

// ---------------------------------------------------------------------------

std::string Context::deviceName() const {
    if (m_physicalDevice == VK_NULL_HANDLE) return {};
    return m_properties.deviceName;
}

std::uint32_t Context::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeOk = (typeFilter & (1u << i)) != 0;
        const bool propOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk) return i;
    }
    throw std::runtime_error("找不到合适的显存类型");
}

VkFormat Context::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        const VkFormatFeatureFlags have =
            (tiling == VK_IMAGE_TILING_LINEAR) ? props.linearTilingFeatures
                                               : props.optimalTilingFeatures;
        if ((have & features) == features) return format;
    }
    throw std::runtime_error("候选格式里没有一个满足要求的 feature flags");
}

void Context::immediateSubmit(const std::function<void(VkCommandBuffer)>& record) const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    record(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submit, fence));

    // 同步等待。教学期这样最容易推理；真实引擎会攒一批再等。
    VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void Context::waitIdle() const {
    if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);
}

void Context::cleanup() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        if (m_allocator != VK_NULL_HANDLE) {
            // VMA 在这里会报「还有 N 块内存没释放」—— 它是你的显存泄漏探测器，
            // 别忽略它。销毁顺序：所有 Buffer/Image 必须先于 allocator 死掉。
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
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

} // namespace rwb::rhi
