// t06 —— HDR lighting -> compute bloom -> tonemap present
#include "../DeferredApp.h"

#include "rwb/core/Todo.h"

namespace p04 {

void DeferredApp::createBloomResources() {
    RWB_TODO("p04-t06 DeferredApp::createBloomResources");
}

// noexcept 清理函数属于框架：骨架抛出 TODO 后仍必须安全释放已创建的资源。
void DeferredApp::destroyBloomResources() noexcept {
    if (!m_ctx) return;
    if (m_postPipeline) vkDestroyPipeline(m_ctx->device(), m_postPipeline, nullptr);
    if (m_postLayout) vkDestroyPipelineLayout(m_ctx->device(), m_postLayout, nullptr);
    if (m_postSetLayout) vkDestroyDescriptorSetLayout(m_ctx->device(), m_postSetLayout, nullptr);
    if (m_postPool) vkDestroyDescriptorPool(m_ctx->device(), m_postPool, nullptr);
    for (VkFramebuffer framebuffer : m_postFramebuffers) {
        vkDestroyFramebuffer(m_ctx->device(), framebuffer, nullptr);
    }
    m_postFramebuffers.clear();
    if (m_postRenderPass) vkDestroyRenderPass(m_ctx->device(), m_postRenderPass, nullptr);
    if (m_bloomPipeline) vkDestroyPipeline(m_ctx->device(), m_bloomPipeline, nullptr);
    if (m_bloomLayout) vkDestroyPipelineLayout(m_ctx->device(), m_bloomLayout, nullptr);
    if (m_bloomSetLayout) vkDestroyDescriptorSetLayout(m_ctx->device(), m_bloomSetLayout, nullptr);
    if (m_bloomPool) vkDestroyDescriptorPool(m_ctx->device(), m_bloomPool, nullptr);
    if (m_postSampler) vkDestroySampler(m_ctx->device(), m_postSampler, nullptr);
    destroyImage(m_bloom);
    destroyImage(m_hdr);
    m_postPipeline = VK_NULL_HANDLE;
    m_postLayout = VK_NULL_HANDLE;
    m_postSetLayout = VK_NULL_HANDLE;
    m_postPool = VK_NULL_HANDLE;
    m_postSet = VK_NULL_HANDLE;
    m_postRenderPass = VK_NULL_HANDLE;
    m_bloomPipeline = VK_NULL_HANDLE;
    m_bloomLayout = VK_NULL_HANDLE;
    m_bloomSetLayout = VK_NULL_HANDLE;
    m_bloomPool = VK_NULL_HANDLE;
    m_bloomSet = VK_NULL_HANDLE;
    m_postSampler = VK_NULL_HANDLE;
}

} // namespace p04
