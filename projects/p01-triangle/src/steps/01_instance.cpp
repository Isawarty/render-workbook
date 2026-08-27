// t01 — instance / validation layer / debug messenger
//
// Vulkan 里第一个对象是 VkInstance：它代表「这个进程和 Vulkan 的连接」。
// OpenGL 里没有对应物，因为 OpenGL 的 context 由窗口系统隐式创建。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace p01 {
namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

// validation layer 的消息全部经这里流向 rwb::ValidationLog，
// L1 测试再去读它的计数。测试验的是你接的这根线，不是框架代劳的。
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*userData*/) {

    const char* text = (data && data->pMessage) ? data->pMessage : "";

    // 原样转交。「哪些算判分失败」由 ValidationLog 决定 ——
    // 因为这条通道上不只有 validation layer 的消息, loader 自己也会发,
    // 而 loader 的絮叨和你的代码对错无关。详见 ValidationLog.h 的注释。
    rwb::ValidationLog::instance().record(severity, types, text);

    const bool isError   = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   != 0;
    const bool isWarning = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;
    if (isError)        rwb::logError(text);
    else if (isWarning) rwb::logWarn(text);

    // 恒返回 VK_FALSE：VK_TRUE 会让触发消息的那个 Vulkan 调用直接中止，
    // 那是给 layer 开发者用的，应用层不该这么干。
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

} // namespace

bool TriangleApp::validationLayerSupported() const {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& l) {
        return std::strcmp(l.layerName, kValidationLayer) == 0;
    });
}

std::vector<const char*> TriangleApp::requiredInstanceExtensions() const {
    // 窗口系统扩展由 GLFW 告诉我们（Windows 上是 VK_KHR_surface + VK_KHR_win32_surface）
    std::vector<const char*> extensions =
        m_window ? Window::requiredInstanceExtensions() : std::vector<const char*>{};

    if (m_config.enableValidation) {
        // debug messenger 属于 instance 级扩展
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

void TriangleApp::createInstance() {
    if (m_config.enableValidation && !validationLayerSupported()) {
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
    // 全课程按 Vulkan 1.2 写：MoltenVK 对 1.3 的支持仍不完整
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = requiredInstanceExtensions();
    VkInstanceCreateFlags    flags      = 0;

    // macOS 分支：MoltenVK 不是完整 Vulkan 实现，而是「portability 实现」。
    // 不显式声明接受 portability 设备，vkCreateInstance 会直接返回
    // VK_ERROR_INCOMPATIBLE_DRIVER —— 这是 Mac 上 Vulkan 的头号劝退点。
    {
        std::uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());

        const bool hasPortability = std::any_of(
            available.begin(), available.end(), [](const VkExtensionProperties& e) {
                return std::strcmp(e.extensionName,
                                   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0;
            });
        if (hasPortability) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags                   = flags;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // 这个 create info 挂在 pNext 上，作用是让 vkCreateInstance / vkDestroyInstance
    // 自身产生的 validation 消息也能被捕获 —— 此时正式的 messenger 还不存在。
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = makeDebugMessengerInfo();
    if (m_config.enableValidation) {
        createInfo.enabledLayerCount   = 1;
        createInfo.ppEnabledLayerNames = &kValidationLayer;
        createInfo.pNext               = &debugInfo;
    }

    const VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(rwb::format("vkCreateInstance 失败, VkResult = %d", res));
    }
}

void TriangleApp::setupDebugMessenger() {
    if (!m_config.enableValidation) return;

    const VkDebugUtilsMessengerCreateInfoEXT info = makeDebugMessengerInfo();
    // vkCreateDebugUtilsMessengerEXT 是扩展函数，不在 loader 的核心导出里。
    // volkLoadInstance() 已经帮我们把它的地址取好了（否则要手动 vkGetInstanceProcAddr）。
    if (vkCreateDebugUtilsMessengerEXT(m_instance, &info, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT 失败");
    }
}

} // namespace p01
