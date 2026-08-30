#include "rwb/platform/Window.h"

#include "rwb/core/Log.h"

#include <stdexcept>

namespace rwb {
namespace {

int g_glfwRefCount = 0;

void ensureGlfwInitialised() {
    if (g_glfwRefCount++ == 0) {
        glfwSetErrorCallback([](int code, const char* desc) {
            logError(format("GLFW 错误 %d: %s", code, desc));
        });
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit 失败");
        }
    }
}

void releaseGlfw() {
    if (--g_glfwRefCount == 0) {
        glfwTerminate();
    }
}

} // namespace

Window::Window(std::uint32_t width, std::uint32_t height, const std::string& title,
               bool highDpiFramebuffer, bool visible) {
    ensureGlfwInitialised();

    if (!glfwVulkanSupported()) {
        releaseGlfw();
        throw std::runtime_error(
            "GLFW 报告本机没有 Vulkan loader。\n"
            "  Windows: 装 Vulkan SDK 或更新显卡驱动\n"
            "  macOS:   装 Vulkan SDK for macOS（内含 MoltenVK）并确认 VK_ICD_FILENAMES 已设置");
    }

    // 不要 OpenGL context —— 这是 Vulkan 程序
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
#ifdef __APPLE__
    // 正常应用保留 Retina；golden/readback 测试可关闭它，让配置尺寸就是像素尺寸。
    // 每次都显式设置，因为 GLFW window hints 会持续影响后续创建的窗口。
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, highDpiFramebuffer ? GLFW_TRUE : GLFW_FALSE);
#else
    (void)highDpiFramebuffer;
#endif

    m_window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                                title.c_str(), nullptr, nullptr);
    if (!m_window) {
        releaseGlfw();
        throw std::runtime_error("glfwCreateWindow 失败");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, &Window::onFramebufferResized);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        releaseGlfw();
    }
}

void Window::onFramebufferResized(GLFWwindow* window, int, int) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_resized = true;
    }
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window) != 0; }
void Window::pollEvents() const  { glfwPollEvents(); }

void Window::framebufferSize(std::uint32_t& outWidth, std::uint32_t& outHeight) const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    outWidth  = static_cast<std::uint32_t>(w);
    outHeight = static_cast<std::uint32_t>(h);
}

bool Window::isMinimized() const {
    std::uint32_t w = 0, h = 0;
    framebufferSize(w, h);
    return w == 0 || h == 0;
}

bool Window::consumeResizeFlag() {
    const bool was = m_resized;
    m_resized = false;
    return was;
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult res = glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(format("glfwCreateWindowSurface 失败, VkResult = %d", res));
    }
    return surface;
}

std::vector<const char*> Window::requiredInstanceExtensions() {
    ensureGlfwInitialised();

    std::uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> result(exts, exts + count);

    releaseGlfw();
    return result;
}

} // namespace rwb
