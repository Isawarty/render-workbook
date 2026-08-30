// t01 —— G-buffer 资源与 render-pass 契约
#include "../DeferredApp.h"

#include "rwb/core/Todo.h"

namespace p04 {

Image DeferredApp::createImage(VkFormat format, VkImageUsageFlags usage,
                               VkImageAspectFlags aspect) {
    const VkExtent2D extent = m_swapchain->extent();
    return createImage(extent.width, extent.height, format, usage, aspect);
}

Image DeferredApp::createImage(std::uint32_t, std::uint32_t, VkFormat,
                               VkImageUsageFlags, VkImageAspectFlags) {
    RWB_TODO("p04-t01 DeferredApp::createImage");
}

void DeferredApp::createGBufferResources() {
    RWB_TODO("p04-t01 DeferredApp::createGBufferResources");
}

void DeferredApp::createRenderPass() {
    RWB_TODO("p04-t01 DeferredApp::createRenderPass");
}

void DeferredApp::createFramebuffers() {
    RWB_TODO("p04-t01 DeferredApp::createFramebuffers");
}

} // namespace p04
