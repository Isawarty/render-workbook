// 截图回读 —— 框架提供，不是考点。
//
// 它涉及 staging buffer、image layout transition、pipeline barrier，
// 这些都是 P2 的教学内容。在 P1 阶段直接给你，让 L3 golden 测试能跑起来。
// 你现在不需要看懂这个文件，做完 P2-t04 之后再回来看会很清楚。
#include "TriangleApp.h"

#include "rwb/core/Log.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace p01 {
namespace {

void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                     VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool formatIsBgra(VkFormat f) {
    return f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_B8G8R8A8_UNORM;
}

} // namespace

TriangleApp::CapturedFrame TriangleApp::renderAndCapture() {
    const std::uint32_t width  = m_swapchainExtent.width;
    const std::uint32_t height = m_swapchainExtent.height;
    const VkDeviceSize  size   = static_cast<VkDeviceSize>(width) * height * 4;

    // --- host-visible 目标 buffer ---------------------------------------
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("截图: vkCreateBuffer 失败");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, buffer, nullptr);
        throw std::runtime_error("截图: vkAllocateMemory 失败");
    }
    vkBindBufferMemory(m_device, buffer, memory, 0);

    // --- 取一张图像并渲染 ------------------------------------------------
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence acquireFence = VK_NULL_HANDLE;
    vkCreateFence(m_device, &fenceInfo, nullptr, &acquireFence);

    std::uint32_t imageIndex = 0;
    const VkResult acquireRes = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                                      VK_NULL_HANDLE, acquireFence, &imageIndex);
    if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        vkDestroyFence(m_device, acquireFence, nullptr);
        vkFreeMemory(m_device, memory, nullptr);
        vkDestroyBuffer(m_device, buffer, nullptr);
        throw std::runtime_error(rwb::format("截图: acquire 失败, VkResult = %d", acquireRes));
    }
    vkWaitForFences(m_device, 1, &acquireFence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_device, acquireFence, nullptr);

    VkCommandBufferAllocateInfo cbInfo{};
    cbInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbInfo.commandPool        = m_commandPool;
    cbInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbInfo.commandBufferCount = 2;
    VkCommandBuffer cmds[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    vkAllocateCommandBuffers(m_device, &cbInfo, cmds);

    // cmds[0]: 正常渲染（复用你写的 recordCommandBuffer）
    recordCommandBuffer(cmds[0], imageIndex);

    // cmds[1]: PRESENT_SRC -> TRANSFER_SRC -> 拷贝 -> 转回 PRESENT_SRC
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmds[1], &begin);

    VkImage image = m_swapchainImages[imageIndex];
    transitionImage(cmds[1], image,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;   // 0 = 紧密排列
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cmds[1], image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    transitionImage(cmds[1], image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_READ_BIT, 0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    vkEndCommandBuffer(cmds[1]);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 2;
    submit.pCommandBuffers    = cmds;

    VkFence done = VK_NULL_HANDLE;
    vkCreateFence(m_device, &fenceInfo, nullptr, &done);
    vkQueueSubmit(m_graphicsQueue, 1, &submit, done);
    vkWaitForFences(m_device, 1, &done, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_device, done, nullptr);

    // --- 读回并转成 RGBA -------------------------------------------------
    CapturedFrame frame;
    frame.width  = width;
    frame.height = height;
    frame.pixels.resize(static_cast<std::size_t>(size));

    void* mapped = nullptr;
    vkMapMemory(m_device, memory, 0, size, 0, &mapped);
    std::memcpy(frame.pixels.data(), mapped, static_cast<std::size_t>(size));
    vkUnmapMemory(m_device, memory);

    if (formatIsBgra(m_swapchainFormat)) {
        for (std::size_t i = 0; i + 3 < frame.pixels.size(); i += 4) {
            std::swap(frame.pixels[i], frame.pixels[i + 2]);
        }
    }
    for (std::size_t i = 3; i < frame.pixels.size(); i += 4) {
        frame.pixels[i] = 255;   // swapchain 的 alpha 不可靠, 强制不透明
    }

    vkFreeCommandBuffers(m_device, m_commandPool, 2, cmds);
    vkFreeMemory(m_device, memory, nullptr);
    vkDestroyBuffer(m_device, buffer, nullptr);
    return frame;
}

} // namespace p01
