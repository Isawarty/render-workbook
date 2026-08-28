// P3 的框架部分 —— 这个文件里没有挖空题。
//
// 里面的东西你都写过：Context 的创建（P1 全程）、VMA buffer 的创建与销毁、
// staging 上传（P2-t01）。P3 的考点在 compute 一侧，不在这里。
//
// 想核对自己当初怎么写的：`git diff mine/p02-t01 -- projects/p02-resources/src/steps/01_buffers.cpp`。

#include "ComputeApp.h"

#include "rwb/core/Log.h"
#include "rwb/rhi/VmaUsage.h"

#include <cstring>
#include <stdexcept>

namespace p03 {

const char* toString(ReducePath path) {
    return path == ReducePath::Subgroup ? "subgroup" : "shared";
}

std::string shaderPath(const std::string& name) { return std::string(RWB_SHADER_DIR) + "/" + name; }

// ---------------------------------------------------------------------------

ComputeApp::ComputeApp(AppConfig config) : m_config(std::move(config)) {
    rwb::rhi::ContextConfig cfg;
    cfg.headless             = true;   // P3 不出图，不需要窗口和 swapchain
    cfg.enableValidation     = m_config.enableValidation;
    cfg.enableSyncValidation = m_config.enableValidation;   // 漏 barrier 的唯一探测器
    cfg.title                = m_config.title;
    cfg.features.fragmentStoresAndAtomics = true;

    m_ctx = std::make_unique<rwb::rhi::Context>(cfg);

    const auto& sub = m_ctx->subgroupProperties();
    rwb::logInfo(rwb::format(
        "GPU: %s | compute 队列族 %u（%s）| subgroupSize %u",
        m_ctx->deviceName().c_str(),
        m_ctx->queueFamilies().compute.value_or(0u),
        m_ctx->queueFamilies().hasDedicatedCompute() ? "专用" : "与图形共用",
        sub.subgroupSize));
}

ComputeApp::~ComputeApp() { cleanup(); }

void ComputeApp::cleanup() noexcept {
    if (!m_ctx) return;
    m_ctx->waitIdle();
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_ctx->device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_ctx.reset();
}

std::string ComputeApp::deviceName() const { return m_ctx ? m_ctx->deviceName() : std::string{}; }

// ---------------------------------------------------------------------------
// 显存：P2-t01 的成果，直接给你。

Buffer ComputeApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               bool hostVisible, bool persistentlyMapped) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if (hostVisible) allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (persistentlyMapped) allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Buffer buffer;
    VmaAllocationInfo info{};
    VK_CHECK(vmaCreateBuffer(m_ctx->allocator(), &bufferInfo, &allocInfo, &buffer.handle,
                             &buffer.allocation, &info));
    buffer.size   = size;
    buffer.mapped = info.pMappedData;
    return buffer;
}

// noexcept。cleanup 路径上的东西一律不挖空 ——
// noexcept 函数里抛异常 = std::terminate = 测试进程当场消失。
void ComputeApp::destroyBuffer(Buffer& buffer) noexcept {
    if (buffer.handle == VK_NULL_HANDLE) return;
    vmaDestroyBuffer(m_ctx->allocator(), buffer.handle, buffer.allocation);
    buffer = Buffer{};
}

void ComputeApp::uploadToBuffer(const void* data, VkDeviceSize size, const Buffer& dst) {
    Buffer staging = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, true);
    std::memcpy(staging.mapped, data, static_cast<std::size_t>(size));

    try {
        m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy region{};
            region.size = size;
            vkCmdCopyBuffer(cmd, staging.handle, dst.handle, 1, &region);
        });
    } catch (...) {
        destroyBuffer(staging);
        throw;
    }
    destroyBuffer(staging);
}

// 同样是 noexcept 的销毁路径，直接给你。销毁顺序与创建顺序相反。
void ComputeApp::destroyComputePipeline(ComputePipeline& pipe) noexcept {
    VkDevice device = m_ctx->device();
    if (pipe.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipe.pipeline, nullptr);
    if (pipe.layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipe.layout, nullptr);
    if (pipe.setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, pipe.setLayout, nullptr);
    }
    pipe = ComputePipeline{};
}

} // namespace p03
