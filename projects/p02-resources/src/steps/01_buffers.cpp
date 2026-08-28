// t01 —— VMA / vertex buffer / index buffer / staging 上传
//
// 任务书: projects/p02-resources/docs/t01-buffers.md
// 判分:   ctest --preset win-msvc -R p02-t01
#include "../ResourceApp.h"
#include "rwb/core/Todo.h"
#include "rwb/rhi/VmaUsage.h"

#include <cstring>

namespace p02 {

Buffer ResourceApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 bool hostVisible, bool persistentlyMapped) {
    // TODO(p02-t01):
    //   用 vmaCreateBuffer 建一个 buffer 并分配显存，填好返回的 Buffer。
    //   1. VkBufferCreateInfo: size / usage / sharingMode = EXCLUSIVE
    //   2. VmaAllocationCreateInfo:
    //        usage = VMA_MEMORY_USAGE_AUTO —— 让 VMA 根据 usage flags 自己挑堆
    //        hostVisible        时加 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    //          （注意：这个提示允许 VMA 选 write-combined 内存，写快但随机读极慢。
    //            要回读的话必须换成 HOST_ACCESS_RANDOM_BIT）
    //        persistentlyMapped 时加 VMA_ALLOCATION_CREATE_MAPPED_BIT
    //   3. 第五个出参 VmaAllocationInfo 里的 pMappedData 就是 Buffer::mapped
    //   4. 别忘了填 Buffer::size —— 测试会拿它和场景数据的字节数比对
    RWB_TODO("p02-t01 ResourceApp::createBuffer");
}

// 这个「不」是挖空题，直接给你。
//
// 原因：它是 noexcept，而 cleanup() 在任何阶段都会调它。
// 挖空的话，你在做完这道题之前每次程序退出都会是一个 abort 弹窗
// （noexcept 函数里抛异常 = std::terminate），而不是可读的报错。
// P1 的析构函数上已经栽过一次，这里不重复。
void ResourceApp::destroyBuffer(Buffer& buffer) noexcept {
    if (buffer.handle != VK_NULL_HANDLE && m_ctx) {
        vmaDestroyBuffer(m_ctx->allocator(), buffer.handle, buffer.allocation);
    }
    buffer = Buffer{};
}

void ResourceApp::uploadViaStaging(const void* data, VkDeviceSize size, const Buffer& dst) {
    // TODO(p02-t01):
    //   CPU 内存 -> host-visible 中转 buffer -> device-local 目标 buffer。
    //   1. 用 createBuffer 建一个 TRANSFER_SRC 的中转 buffer（hostVisible + 常驻映射）
    //   2. memcpy 进去
    //   3. m_ctx->immediateSubmit 里用 vkCmdCopyBuffer 拷到 dst
    //   4. immediateSubmit 是同步等待的，回来时拷贝一定做完了，销毁中转 buffer
    //
    //   想清楚：为什么不直接往 device-local 里写？
    RWB_TODO("p02-t01 ResourceApp::uploadViaStaging");
}

void ResourceApp::createVertexBuffer() {
    // TODO(p02-t01):
    //   把 m_vertices 传进显存。
    //   usage 要同时包含 TRANSFER_DST（staging 的目标）和 VERTEX_BUFFER（能被绑定）。
    //   hostVisible = false —— 顶点数据一次写入、反复被 GPU 读，该待在 device-local 里。
    RWB_TODO("p02-t01 ResourceApp::createVertexBuffer");
}

void ResourceApp::createIndexBuffer() {
    // TODO(p02-t01):
    //   同上，把 m_indices 传进显存。usage 换成 TRANSFER_DST | INDEX_BUFFER。
    //   注意索引是 std::uint32_t（框架在 recordFrame 里按 VK_INDEX_TYPE_UINT32 绑定）。
    RWB_TODO("p02-t01 ResourceApp::createIndexBuffer");
}

} // namespace p02
