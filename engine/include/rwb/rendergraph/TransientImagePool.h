#pragma once

#include "rwb/rendergraph/RenderGraph.h"
#include "rwb/rhi/Context.h"

#include <cstdint>
#include <vector>

VK_DEFINE_HANDLE(VmaAllocation)

namespace rwb::rg {

struct TransientImageBinding {
    ResourceHandle resource;
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    std::uint32_t physicalSlot = 0;
};

class TransientImagePool {
public:
    TransientImagePool() = default;
    TransientImagePool(rwb::rhi::Context& context, const CompiledGraph& graph);
    ~TransientImagePool();

    TransientImagePool(const TransientImagePool&) = delete;
    TransientImagePool& operator=(const TransientImagePool&) = delete;

    void build(rwb::rhi::Context& context, const CompiledGraph& graph);
    void clear() noexcept;
    const TransientImageBinding& binding(ResourceHandle resource) const;
    const std::vector<TransientImageBinding>& bindings() const { return m_bindings; }

private:
    struct Slot {
        std::uint32_t id = 0;
        VkImage ownerImage = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        ResourceDesc desc;
    };
    struct OwnedBinding {
        TransientImageBinding binding;
        bool ownsAllocation = false;
    };

    rwb::rhi::Context* m_context = nullptr;
    std::vector<Slot> m_slots;
    std::vector<OwnedBinding> m_ownedBindings;
    std::vector<TransientImageBinding> m_bindings;
};

} // namespace rwb::rg
