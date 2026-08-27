#include "EnvProbe.h"

#include "rwb/core/File.h"
#include "rwb/core/Log.h"

#include <volk.h>

#include <algorithm>
#include <cstring>

namespace p00 {
namespace {

std::vector<std::string> enumerateInstanceExtensions() {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());

    std::vector<std::string> names;
    names.reserve(count);
    for (const VkExtensionProperties& p : props) names.emplace_back(p.extensionName);
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> enumerateInstanceLayers() {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> props(count);
    vkEnumerateInstanceLayerProperties(&count, props.data());

    std::vector<std::string> names;
    names.reserve(count);
    for (const VkLayerProperties& p : props) names.emplace_back(p.layerName);
    std::sort(names.begin(), names.end());
    return names;
}

bool contains(const std::vector<std::string>& v, const char* what) {
    return std::find(v.begin(), v.end(), std::string(what)) != v.end();
}

} // namespace

std::string versionToString(std::uint32_t version) {
    return rwb::format("%u.%u.%u", VK_API_VERSION_MAJOR(version),
                          VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
}

EnvReport probeEnvironment() {
    EnvReport report;

    // volk 负责把所有 vk* 函数指针加载出来。
    // 全局定义了 VK_NO_PROTOTYPES，所以不调这一步的话，之后每个 vk 调用都是空指针。
    if (volkInitialize() != VK_SUCCESS) {
        report.error =
            "volkInitialize 失败：找不到 Vulkan loader。\n"
            "  Windows: 装 Vulkan SDK 或更新显卡驱动\n"
            "  macOS:   装 Vulkan SDK for macOS（内含 MoltenVK）\n"
            "  Linux:   apt install mesa-vulkan-drivers 或对应厂商驱动";
        return report;
    }
    report.volkOk = true;

    report.instanceExtensions = enumerateInstanceExtensions();
    report.instanceLayers     = enumerateInstanceLayers();
    report.validationLayerAvailable = contains(report.instanceLayers, "VK_LAYER_KHRONOS_validation");

    std::uint32_t apiVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) {
        vkEnumerateInstanceVersion(&apiVersion);
    }
    report.instanceApiVersion = apiVersion;

    VkApplicationInfo appInfo{};
    appInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "render-workbook p00 env probe";
    appInfo.apiVersion       = VK_API_VERSION_1_2;

    std::vector<const char*> extensions;
    VkInstanceCreateFlags    flags = 0;

    // macOS / MoltenVK 不是「完整 Vulkan 实现」，而是 portability 实现。
    // 不带这两样，vkCreateInstance 会直接返回 VK_ERROR_INCOMPATIBLE_DRIVER。
    // P1-t01 你要自己再写一遍这段，这里先让环境自检能在 Mac 上跑通。
    if (contains(report.instanceExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags                   = flags;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        report.error = rwb::format("vkCreateInstance 失败, VkResult = %d", res);
        return report;
    }
    report.instanceOk = true;
    volkLoadInstance(instance);

    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    if (deviceCount > 0) {
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
    }

    for (VkPhysicalDevice pd : physicalDevices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        DeviceInfo info;
        info.name          = props.deviceName;
        info.apiVersion    = props.apiVersion;
        info.driverVersion = props.driverVersion;
        info.vendorId      = props.vendorID;
        info.isDiscrete    = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

        std::uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, queues.data());
        for (const VkQueueFamilyProperties& q : queues) {
            if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) info.hasGraphics = true;
            if (q.queueFlags & VK_QUEUE_COMPUTE_BIT)  info.hasCompute  = true;
        }
        report.devices.push_back(std::move(info));
    }

    vkDestroyInstance(instance, nullptr);
    return report;
}

std::size_t probeCompiledShader(const std::string& spvPath) {
    // readSpirv 内部会校验 magic number 与 4 字节对齐
    return rwb::readSpirv(spvPath).size();
}

} // namespace p00
