#pragma once

// 顺序要紧：先 volk（它带进 vulkan 头），再让 GLFW 看到 Vulkan 类型。
// 全局定义了 VK_NO_PROTOTYPES，所以所有 vk* 函数指针都由 volk 动态加载，
// 我们不链接 vulkan-1.lib / libvulkan.dylib —— 这是「零 SDK 也能编译」的关键。
#include <volk.h>

// 必须是 GLFW_INCLUDE_NONE 而不是 GLFW_INCLUDE_VULKAN。
// 后者只是「额外」再 include 一次 vulkan.h，并不会阻止 glfw3.h 去 include OpenGL 头；
// 而我们一点 OpenGL 都不需要。Windows SDK 自带 GL/gl.h、macOS 走 <OpenGL/gl.h> 分支，
// 所以这个疏漏在两边都不报错，只有 Linux 会炸（那里 GL/gl.h 属于 libgl-dev）。
//
// glfwCreateWindowSurface 的声明由 glfw3.h 内部的 VK_VERSION_1_0 守卫控制，
// volk 已经定义了它，所以即便 GLFW 不 include 任何东西，声明仍然可见。
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace rwb {

// GLFW 窗口的薄封装。
// 刻意不做成挖空题：窗口系统不是这门课的教学点，Vulkan 才是。
class Window {
public:
    Window(std::uint32_t width, std::uint32_t height, const std::string& title);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void pollEvents() const;

    // 注意是 framebuffer 尺寸而不是窗口尺寸。
    // 在 macOS Retina 上两者差 2 倍，swapchain 必须用前者。
    void framebufferSize(std::uint32_t& outWidth, std::uint32_t& outHeight) const;

    // 窗口被最小化时 framebuffer 尺寸是 0x0，此时不能建 swapchain（P1-t08 会踩到）。
    bool isMinimized() const;

    // 自上次调用起窗口是否变过大小；调用后清标志。P1-t08 用。
    bool consumeResizeFlag();

    VkSurfaceKHR createSurface(VkInstance instance) const;

    // Vulkan instance 必须开启的窗口系统扩展（Windows 上是 VK_KHR_win32_surface 等）。
    static std::vector<const char*> requiredInstanceExtensions();

    GLFWwindow* handle() const { return m_window; }

private:
    static void onFramebufferResized(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window  = nullptr;
    bool        m_resized = false;
};

} // namespace rwb
