// t02 — surface / physical device / queue family / logical device
//
// 任务书: projects/p01-triangle/docs/t02-device.md
// 判分:   ctest --preset win-msvc -R p01-t02
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

namespace p01 {

void TriangleApp::createSurface() {
    // TODO(p01-t02):
    //   m_surface = m_window->createSurface(m_instance);
    //
    //   为什么 surface 在这一题而不是 swapchain 那一题: 选物理设备时要判断
    //   「这个设备能不能把画面交给这个窗口」, 没有 surface 就无从判断。
    RWB_TODO("p01-t02 TriangleApp::createSurface");
}

QueueFamilyIndices TriangleApp::findQueueFamilies(VkPhysicalDevice device) const {
    // TODO(p01-t02):
    //   遍历队列族，找出:
    //     - 支持 VK_QUEUE_GRAPHICS_BIT 的族 -> indices.graphics
    //     - 对 m_surface 支持 present 的族 -> indices.present
    //       （用 vkGetPhysicalDeviceSurfaceSupportKHR 查，别假设和 graphics 是同一个）
    //   m_surface 为 VK_NULL_HANDLE（headless）时，present 直接沿用 graphics。
    RWB_TODO("p01-t02 TriangleApp::findQueueFamilies");
}

bool TriangleApp::deviceExtensionsSupported(VkPhysicalDevice device) const {
    // TODO(p01-t02):
    //   确认设备支持 VK_KHR_SWAPCHAIN_EXTENSION_NAME。
    RWB_TODO("p01-t02 TriangleApp::deviceExtensionsSupported");
}

SwapchainSupport TriangleApp::querySwapchainSupport(VkPhysicalDevice device) const {
    // TODO(p01-t02):
    //   查询三件事填进 SwapchainSupport:
    //     capabilities  — vkGetPhysicalDeviceSurfaceCapabilitiesKHR
    //     formats       — vkGetPhysicalDeviceSurfaceFormatsKHR
    //     presentModes  — vkGetPhysicalDeviceSurfacePresentModesKHR
    RWB_TODO("p01-t02 TriangleApp::querySwapchainSupport");
}

bool TriangleApp::isDeviceSuitable(VkPhysicalDevice device) const {
    // TODO(p01-t02):
    //   综合判断: 队列族齐全 + 扩展支持 + （有 surface 时）swapchain 格式和呈现模式非空。
    RWB_TODO("p01-t02 TriangleApp::isDeviceSuitable");
}

void TriangleApp::pickPhysicalDevice() {
    // TODO(p01-t02):
    //   枚举物理设备，在「可用」的里面挑一个最好的 -> m_physicalDevice,
    //   并把它的队列族记进 m_queueFamilies。
    //
    //   注意别直接取第 0 个: 很多机器上核显排在独显前面
    //   （跑一下 p00_setup 看看你这台的顺序）。
    RWB_TODO("p01-t02 TriangleApp::pickPhysicalDevice");
}

void TriangleApp::createLogicalDevice() {
    // TODO(p01-t02):
    //   1. graphics 和 present 可能是同一个族 —— 去重，否则提交重复的
    //      VkDeviceQueueCreateInfo 是非法的
    //   2. 开启 VK_KHR_swapchain 扩展
    //   3. macOS: 如果设备上报了 VK_KHR_portability_subset，规范要求你「必须」开启它
    //   4. vkCreateDevice -> m_device
    //   5. vkGetDeviceQueue 取出 m_graphicsQueue / m_presentQueue
    RWB_TODO("p01-t02 TriangleApp::createLogicalDevice");
}

} // namespace p01
