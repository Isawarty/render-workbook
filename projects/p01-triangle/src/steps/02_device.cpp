// t02 — surface / physical device / queue family / logical device
//
// OpenGL 里「设备」是隐式的、唯一的。Vulkan 把它拆成两层：
//   VkPhysicalDevice  只读句柄，代表机器上的一块 GPU，用来查询能力
//   VkDevice          你按需开启了某些特性/扩展之后，得到的逻辑连接
// 你要显式声明用哪些队列、开哪些特性 —— 没声明的东西用了就是 undefined behaviour。
#include "../TriangleApp.h"

#include "rwb/core/Log.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

namespace p01 {
namespace {

// swapchain 不是核心功能，是设备级扩展：无窗口的计算程序不需要它。
const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

} // namespace

// surface 必须在挑物理设备「之前」建好: 「能不能把画面交给这个窗口」
// 是选设备的判据之一, 没有 surface 就无从判断。
// 这也是为什么本课把 surface 归在 t02 而不是 t03。
void TriangleApp::createSurface() {
    m_surface = m_window->createSurface(m_instance);
}

QueueFamilyIndices TriangleApp::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (std::uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount == 0) continue;

        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (!indices.graphics.has_value()) indices.graphics = i;
        }

        // 「能画」和「能把画面交给窗口系统」是两件独立的能力。
        // 多数硬件上是同一个队列族，但规范不保证，所以必须分别查。
        if (m_surface != VK_NULL_HANDLE) {
            VkBool32 presentSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupported);
            if (presentSupported && !indices.present.has_value()) indices.present = i;
        }

        if (indices.complete()) break;
    }

    // headless（没有 surface）时不需要 present 能力，直接放行
    if (m_surface == VK_NULL_HANDLE && indices.graphics.has_value()) {
        indices.present = indices.graphics;
    }
    return indices;
}

bool TriangleApp::deviceExtensionsSupported(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* required : kRequiredDeviceExtensions) {
        const bool found = std::any_of(available.begin(), available.end(),
                                       [required](const VkExtensionProperties& e) {
                                           return std::strcmp(e.extensionName, required) == 0;
                                       });
        if (!found) return false;
    }
    return true;
}

// 查询 surface 支持情况。放在 t02 是因为 isDeviceSuitable() 要用它 ——
// 「这个设备对这个窗口有没有可用的格式和呈现模式」也是选设备的判据。
SwapchainSupport TriangleApp::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &support.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, support.formats.data());
    }

    std::uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount, nullptr);
    support.presentModes.resize(modeCount);
    if (modeCount) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount,
                                                  support.presentModes.data());
    }
    return support;
}

bool TriangleApp::isDeviceSuitable(VkPhysicalDevice device) const {
    if (!findQueueFamilies(device).complete()) return false;
    if (m_surface != VK_NULL_HANDLE && !deviceExtensionsSupported(device)) return false;

    if (m_surface != VK_NULL_HANDLE) {
        // 有 surface 却没有任何可用格式/呈现模式的设备是不能用的
        const SwapchainSupport support = querySwapchainSupport(device);
        if (support.formats.empty() || support.presentModes.empty()) return false;
    }
    return true;
}

void TriangleApp::pickPhysicalDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("找不到任何支持 Vulkan 的物理设备");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // 打分挑选：优先独显。
    // 注意别无脑选第一个 —— 你这台机器上核显（AMD Radeon）排在 RTX 5070 前面。
    int              bestScore  = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (VkPhysicalDevice device : devices) {
        if (!isDeviceSuitable(device)) continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)        score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        score += static_cast<int>(props.limits.maxImageDimension2D / 1024);

        if (score > bestScore) {
            bestScore  = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "有物理设备，但没有一个同时满足: graphics 队列 + present 支持 + swapchain 扩展");
    }

    m_physicalDevice = bestDevice;
    m_queueFamilies  = findQueueFamilies(bestDevice);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(bestDevice, &props);
    rwb::logInfo(rwb::format("选中 GPU: %s", props.deviceName));
}

void TriangleApp::createLogicalDevice() {
    // graphics 和 present 有可能是同一个队列族，用 set 去重，
    // 否则给同一个 queueFamilyIndex 提交两份 DeviceQueueCreateInfo 是非法的。
    const std::set<std::uint32_t> uniqueFamilies = {
        m_queueFamilies.graphics.value(),
        m_queueFamilies.present.value(),
    };

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    for (std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &queuePriority;   // 必填，即使只有一个队列
        queueInfos.push_back(qi);
    }

    // P1 不需要任何额外特性。到 P2 做各向异性过滤时这里才会真正用上。
    VkPhysicalDeviceFeatures features{};

    std::vector<const char*> deviceExtensions = kRequiredDeviceExtensions;

    // macOS: MoltenVK 会上报 VK_KHR_portability_subset。
    // 规范规定：只要设备支持它，就「必须」在 createDevice 时开启，否则行为未定义。
    {
        std::uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, available.data());

        const bool hasPortabilitySubset = std::any_of(
            available.begin(), available.end(), [](const VkExtensionProperties& e) {
                return std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0;
            });
        if (hasPortabilitySubset) {
            deviceExtensions.push_back("VK_KHR_portability_subset");
        }
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<std::uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    // 注意: enabledLayerCount / ppEnabledLayerNames 在现代 Vulkan 里已废弃。
    // device 级 layer 不再存在, instance 上开的 layer 自动覆盖 device 调用。

    const VkResult res = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(rwb::format("vkCreateDevice 失败, VkResult = %d", res));
    }

    vkGetDeviceQueue(m_device, m_queueFamilies.graphics.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilies.present.value(),  0, &m_presentQueue);
}

} // namespace p01
