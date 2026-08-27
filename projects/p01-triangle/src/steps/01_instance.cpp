// t01 — instance / validation layer / debug messenger
//
// 任务书: projects/p01-triangle/docs/t01-instance.md
// 判分:   ctest --preset win-msvc -R p01-t01
#include "../TriangleApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace p01 {

bool TriangleApp::validationLayerSupported() const {
    // TODO(p01-t01):
    //   枚举 instance layer，判断 "VK_LAYER_KHRONOS_validation" 是否在其中。
    //   提示: vkEnumerateInstanceLayerProperties 是「先查数量再查内容」的两段式调用，
    //         这个模式在 Vulkan 里到处都是。
    RWB_TODO("p01-t01 TriangleApp::validationLayerSupported");
}

std::vector<const char*> TriangleApp::requiredInstanceExtensions() const {
    // TODO(p01-t01):
    //   1. 从 Window::requiredInstanceExtensions() 拿到窗口系统需要的扩展
    //      （headless 时 m_window 为空，返回空表即可）
    //   2. 若 m_config.enableValidation，追加 VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    RWB_TODO("p01-t01 TriangleApp::requiredInstanceExtensions");
}

void TriangleApp::createInstance() {
    // TODO(p01-t01):
    //   1. 若要求了 validation 但系统没有，抛异常并说清楚原因
    //   2. 填 VkApplicationInfo，apiVersion 用 VK_API_VERSION_1_2
    //   3. 收集扩展；在 macOS 上还要处理 portability
    //      （查 VK_KHR_portability_enumeration 是否可用，可用则同时加扩展和 create flag，
    //        否则 MoltenVK 会直接返回 VK_ERROR_INCOMPATIBLE_DRIVER）
    //   4. 开启 validation layer
    //   5. 把一个 VkDebugUtilsMessengerCreateInfoEXT 挂到 pNext 上，
    //      好让 vkCreateInstance/vkDestroyInstance 自身的消息也能被捕获
    //   6. vkCreateInstance -> m_instance
    RWB_TODO("p01-t01 TriangleApp::createInstance");
}

void TriangleApp::setupDebugMessenger() {
    // TODO(p01-t01):
    //   创建 VkDebugUtilsMessengerEXT -> m_debugMessenger。
    //
    //   回调里必须把消息转交给:
    //       rwb::ValidationLog::instance().record(severity, types, pMessage);
    //   注意是「原样转交」——severity 和 types 都要传，不要自己先过滤。
    //   哪些消息算判分失败由框架决定（这条通道上不只有 validation layer 的消息，
    //   loader 自己也会发，详见 ValidationLog.h）。
    //   不接这根线，L1 测试收不到任何东西，validation 报错也不会让测试变红。
    //
    //   回调恒返回 VK_FALSE。
    RWB_TODO("p01-t01 TriangleApp::setupDebugMessenger");
}

} // namespace p01
