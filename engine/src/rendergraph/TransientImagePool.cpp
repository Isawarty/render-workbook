#include "rwb/rendergraph/TransientImagePool.h"

#include "rwb/rhi/VmaUsage.h"

#include <algorithm>
#include <stdexcept>

namespace rwb::rg {
namespace {

VkImageCreateInfo imageInfo(const ResourceDesc& desc) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = desc.format;
    info.extent = {desc.width, desc.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = desc.imageUsage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return info;
}

bool compatible(const ResourceDesc& lhs, const ResourceDesc& rhs) {
    return lhs.kind == rhs.kind && lhs.compatibilityKey == rhs.compatibilityKey &&
           lhs.width == rhs.width && lhs.height == rhs.height &&
           lhs.format == rhs.format && lhs.imageUsage == rhs.imageUsage;
}

} // namespace

TransientImagePool::TransientImagePool(rwb::rhi::Context& context,
                                       const CompiledGraph& graph) {
    build(context, graph);
}

TransientImagePool::~TransientImagePool() { clear(); }

void TransientImagePool::build(rwb::rhi::Context& context,
                               const CompiledGraph& graph) {
    clear();
    m_context = &context;
    try {
        for (const Lifetime& lifetime : graph.lifetimes()) {
            const ResourceDesc& desc = graph.resources().at(lifetime.resource.id);
            if (!desc.transient || desc.kind != ResourceKind::Image ||
                desc.image != VK_NULL_HANDLE || desc.width == 0 || desc.height == 0 ||
                desc.format == VK_FORMAT_UNDEFINED || desc.imageUsage == 0) {
                continue;
            }

            auto slotIt = std::find_if(m_slots.begin(), m_slots.end(),
                [&](const Slot& slot) { return slot.id == lifetime.physicalSlot; });
            VkImage image = VK_NULL_HANDLE;
            bool owner = false;
            if (slotIt == m_slots.end()) {
                Slot slot;
                slot.id = lifetime.physicalSlot;
                slot.desc = desc;
                VmaAllocationCreateInfo allocationInfo{};
                allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                allocationInfo.flags = VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT;
                const VkImageCreateInfo createInfo = imageInfo(desc);
                VK_CHECK(vmaCreateImage(context.allocator(), &createInfo, &allocationInfo,
                                        &slot.ownerImage, &slot.allocation, nullptr));
                image = slot.ownerImage;
                owner = true;
                m_slots.push_back(slot);
                slotIt = std::prev(m_slots.end());
            } else {
                if (!compatible(slotIt->desc, desc)) {
                    throw std::logic_error(
                        "同一 transient physical slot 含不兼容的 image 描述");
                }
                const VkImageCreateInfo createInfo = imageInfo(desc);
                VK_CHECK(vmaCreateAliasingImage(context.allocator(), slotIt->allocation,
                                                &createInfo, &image));
            }
            TransientImageBinding binding{lifetime.resource, image, slotIt->allocation,
                                          lifetime.physicalSlot};
            m_ownedBindings.push_back({binding, owner});
            m_bindings.push_back(binding);
        }
    } catch (...) {
        clear();
        throw;
    }
}

void TransientImagePool::clear() noexcept {
    if (!m_context) return;
    // Resource cleanup must remain non-throwing, including from the destructor.
    // Ignore a device-lost result here and still release the host-side handles.
    (void)vkDeviceWaitIdle(m_context->device());
    for (const OwnedBinding& owned : m_ownedBindings) {
        if (!owned.ownsAllocation && owned.binding.image != VK_NULL_HANDLE) {
            vkDestroyImage(m_context->device(), owned.binding.image, nullptr);
        }
    }
    for (const Slot& slot : m_slots) {
        if (slot.ownerImage != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context->allocator(), slot.ownerImage, slot.allocation);
        }
    }
    m_bindings.clear();
    m_ownedBindings.clear();
    m_slots.clear();
    m_context = nullptr;
}

const TransientImageBinding& TransientImagePool::binding(ResourceHandle resource) const {
    const auto found = std::find_if(m_bindings.begin(), m_bindings.end(),
        [resource](const TransientImageBinding& item) { return item.resource == resource; });
    if (found == m_bindings.end()) {
        throw std::out_of_range("resource 没有 materialized transient image");
    }
    return *found;
}

} // namespace rwb::rg
