// t02 —— UBO / descriptor set layout / pool / set
//
// push constant 和 UBO 都是「往 shader 里塞数据」，区别在于量级与更新频率。
// 这一题先建立 descriptor 这套机制：它是 Vulkan 里资源绑定的全部答案，
// 后面的纹理、storage buffer、G-Buffer 输入全都走它。
#include "../ResourceApp.h"
#include "rwb/rhi/VmaUsage.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstring>

namespace p02 {

// descriptor set layout = 「这一组资源里，第几号槽位放什么类型的东西、哪些 shader 阶段能看到」。
// 它是一份「模板」，不含任何具体资源；具体资源在 createDescriptorSets 里才填。
void ResourceApp::createDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    VkDescriptorSetLayoutBinding camera{};
    camera.binding         = 0;
    camera.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camera.descriptorCount = 1;                              // 不是数组，就是 1
    camera.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;     // 只有顶点着色器要用
    bindings.push_back(camera);

    // t05 之后才有纹理。布局必须和当前 pipeline 用的 shader 严格一致 ——
    // shader 里没声明 binding 1，布局里却有，在多数驱动上能过，但那是运气；
    // 反过来（shader 有、布局没有）则一定报错。
    if (stageIndex(m_reached) >= stageIndex(Stage::Sampler)) {
        VkDescriptorSetLayoutBinding sampler{};
        sampler.binding         = 1;
        sampler.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler.descriptorCount = 1;
        sampler.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(sampler);
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    info.pBindings    = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &info, nullptr, &m_descriptorSetLayout));
}

// 每个在飞帧一份 UBO。
//
// 只建一份会出事：CPU 在为第 N+1 帧写 UBO 时，GPU 可能还在读第 N 帧的同一块内存。
// 这是「多帧并行」真正的代价 —— 不只是命令缓冲要复制，所有 CPU 每帧会改的资源都要。
void ResourceApp::createUniformBuffers() {
    m_uniformBuffers.resize(kFramesInFlight);
    for (Buffer& buffer : m_uniformBuffers) {
        buffer = createBuffer(sizeof(CameraUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              /*hostVisible=*/true, /*persistentlyMapped=*/true);
    }
}

// 池按「每种类型要发多少个」预先声明。发超了返回 VK_ERROR_OUT_OF_POOL_MEMORY，
// 不会自动扩容 —— 这是 Vulkan 一贯的风格：容量由你规划，运行期不做隐式分配。
void ResourceApp::createDescriptorPool() {
    std::vector<VkDescriptorPoolSize> sizes;

    VkDescriptorPoolSize uniform{};
    uniform.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform.descriptorCount = kFramesInFlight;
    sizes.push_back(uniform);

    if (stageIndex(m_reached) >= stageIndex(Stage::Sampler)) {
        VkDescriptorPoolSize sampler{};
        sampler.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler.descriptorCount = kFramesInFlight;
        sizes.push_back(sampler);
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
    info.pPoolSizes    = sizes.data();
    info.maxSets       = kFramesInFlight;    // 最多能分出几个 set，和上面的计数是两回事
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &info, nullptr, &m_descriptorPool));
}

// 从池里分出 set，再把「具体是哪个 buffer / 哪张图」写进去。
void ResourceApp::createDescriptorSets() {
    const std::vector<VkDescriptorSetLayout> layouts(kFramesInFlight, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool     = m_descriptorPool;
    alloc.descriptorSetCount = kFramesInFlight;
    alloc.pSetLayouts        = layouts.data();

    m_descriptorSets.resize(kFramesInFlight);
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &alloc, m_descriptorSets.data()));

    for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
        std::vector<VkWriteDescriptorSet> writes;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i].handle;
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(CameraUniform);   // 也可以用 VK_WHOLE_SIZE

        VkWriteDescriptorSet cameraWrite{};
        cameraWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cameraWrite.dstSet          = m_descriptorSets[i];
        cameraWrite.dstBinding      = 0;
        cameraWrite.dstArrayElement = 0;
        cameraWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraWrite.descriptorCount = 1;
        cameraWrite.pBufferInfo     = &bufferInfo;
        writes.push_back(cameraWrite);

        VkDescriptorImageInfo imageInfo{};
        if (m_texture.valid() && m_sampler != VK_NULL_HANDLE) {
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = m_texture.view;
            imageInfo.sampler     = m_sampler;

            VkWriteDescriptorSet samplerWrite{};
            samplerWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            samplerWrite.dstSet          = m_descriptorSets[i];
            samplerWrite.dstBinding      = 1;
            samplerWrite.dstArrayElement = 0;
            samplerWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerWrite.descriptorCount = 1;
            samplerWrite.pImageInfo      = &imageInfo;
            writes.push_back(samplerWrite);
        }

        // 注意：bufferInfo / imageInfo 是栈上的，vkUpdateDescriptorSets
        // 必须在它们还活着的时候调用。把 write 攒到循环外再统一提交是常见 bug。
        vkUpdateDescriptorSets(m_ctx->device(), static_cast<std::uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }
}

void ResourceApp::updateUniformBuffer(std::uint32_t frameIndex) {
    const VkExtent2D extent = m_swapchain->extent();
    const float aspect = static_cast<float>(extent.width) /
                         static_cast<float>(extent.height == 0 ? 1 : extent.height);

    CameraUniform ubo{};
    ubo.view = glm::lookAt(glm::vec3(0.0f, 1.2f, 3.2f),    // 眼睛
                           glm::vec3(0.0f, 0.0f, 0.0f),    // 看向
                           glm::vec3(0.0f, 1.0f, 0.0f));   // 上方向

    ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 30.0f);
    // glm 出来的投影矩阵按 OpenGL 约定：NDC 的 +Y 朝上。Vulkan 的 +Y 朝下。
    // 不翻这一下，画面上下颠倒 —— 而且因为是对称场景，你可能过好几题才发现。
    ubo.proj[1][1] *= -1.0f;

    // 常驻映射，直接写。内存是 HOST_COHERENT 的（VMA 在 AUTO + SEQUENTIAL_WRITE
    // 之下会优先选），所以不需要 vmaFlushAllocation。
    std::memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));
}

} // namespace p02
