// t01 —— VMA / vertex buffer / index buffer / staging 上传
//
// P1 的三角形顶点硬编码在 shader 里，因为「把数据搬到 GPU」这件事本身
// 就够一整节课。现在补上：显存分配、staging 中转、以及为什么需要中转。
#include "../ResourceApp.h"
#include "rwb/rhi/VmaUsage.h"

#include <cstring>

namespace p02 {

// 建一个 buffer 并分配显存。
//
// VMA_MEMORY_USAGE_AUTO 让 VMA 根据 usage flags 和你要不要 CPU 访问，
// 自己去挑内存堆。手写 findMemoryType 那一套（P1 的 Capture.cpp 里有）
// 在有 ReBAR、有多个堆的现代硬件上很容易挑错，交给 VMA 更省心也更快。
Buffer ResourceApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 bool hostVisible, bool persistentlyMapped) {
    Buffer buffer;
    buffer.size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if (hostVisible) {
        // SEQUENTIAL_WRITE：我们只会顺着写一遍，不回读。
        // 这个提示允许 VMA 选 write-combined 内存 —— 写很快，但随机读极慢。
        // 如果你要回读，必须换成 HOST_ACCESS_RANDOM_BIT。
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    if (persistentlyMapped) {
        // 常驻映射。每帧都要更新的 UBO 用它，省掉每帧一对 map/unmap。
        allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo result{};
    VK_CHECK(vmaCreateBuffer(m_ctx->allocator(), &bufferInfo, &allocInfo,
                             &buffer.handle, &buffer.allocation, &result));
    buffer.mapped = result.pMappedData;
    return buffer;
}

void ResourceApp::destroyBuffer(Buffer& buffer) noexcept {
    if (buffer.handle != VK_NULL_HANDLE && m_ctx) {
        vmaDestroyBuffer(m_ctx->allocator(), buffer.handle, buffer.allocation);
    }
    buffer = Buffer{};
}

// staging 上传：CPU 内存 -> host-visible 中转 buffer -> device-local 目标 buffer。
//
// 为什么不直接往 device-local 里写？因为显存通常「不」映射到 CPU 地址空间。
// 能同时满足 DEVICE_LOCAL + HOST_VISIBLE 的堆在老硬件上只有 256MB（ReBAR 之前），
// 顶点数据放不下。所以标准做法是：写进能映射的中转站，再让 GPU 自己拷过去。
void ResourceApp::uploadViaStaging(const void* data, VkDeviceSize size, const Buffer& dst) {
    Buffer staging = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  /*hostVisible=*/true, /*persistentlyMapped=*/true);
    std::memcpy(staging.mapped, data, static_cast<std::size_t>(size));

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, staging.handle, dst.handle, 1, &region);
    });

    // immediateSubmit 是同步等待的，回来时拷贝一定做完了，可以安全销毁中转站。
    // 真实引擎会攒一批上传再等一次 fence，那时就不能这么随手销毁了。
    destroyBuffer(staging);
}

void ResourceApp::createVertexBuffer() {
    const VkDeviceSize size = sizeof(Vertex) * m_vertices.size();

    // TRANSFER_DST：它是 staging 拷贝的目标
    // VERTEX_BUFFER：它能被 vkCmdBindVertexBuffers 绑定
    // 少写任何一个，validation layer 会当场告诉你
    m_vertexBuffer = createBuffer(size,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  /*hostVisible=*/false, /*persistentlyMapped=*/false);
    uploadViaStaging(m_vertices.data(), size, m_vertexBuffer);
}

void ResourceApp::createIndexBuffer() {
    const VkDeviceSize size = sizeof(std::uint32_t) * m_indices.size();

    m_indexBuffer = createBuffer(size,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 /*hostVisible=*/false, /*persistentlyMapped=*/false);
    uploadViaStaging(m_indices.data(), size, m_indexBuffer);
}

} // namespace p02
