#include "rwb/rendergraph/TransientImagePool.h"

#include "rwb/core/Todo.h"
#include "rwb/rhi/VmaUsage.h"

namespace rwb::rg {

TransientImagePool::TransientImagePool(rwb::rhi::Context& context,
                                       const CompiledGraph& graph) {
    build(context, graph);
}

TransientImagePool::~TransientImagePool() { clear(); }

void TransientImagePool::build(rwb::rhi::Context&, const CompiledGraph&) {
    RWB_TODO("p05-t04 TransientImagePool::build");
}

void TransientImagePool::clear() noexcept {
    if (!m_context) return;
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

const TransientImageBinding& TransientImagePool::binding(ResourceHandle) const {
    RWB_TODO("p05-t04 TransientImagePool::binding");
}

} // namespace rwb::rg
