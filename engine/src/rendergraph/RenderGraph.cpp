#include "rwb/rendergraph/RenderGraph.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace rwb::rg {
namespace {

bool writes(Access access) { return access != Access::Read; }

const char* accessLabel(Access access) {
    switch (access) {
    case Access::Read: return "R";
    case Access::Write: return "W";
    case Access::ReadWrite: return "RW";
    }
    return "?";
}

void addUnique(std::vector<PassHandle>& values, PassHandle value) {
    if (std::none_of(values.begin(), values.end(),
                     [value](PassHandle item) { return item == value; })) {
        values.push_back(value);
    }
}

std::string dotEscape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        if (ch == '"' || ch == '\\') escaped.push_back('\\');
        escaped.push_back(ch);
    }
    return escaped;
}

} // namespace

PassBuilder& PassBuilder::read(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::Read, state);
}

PassBuilder& PassBuilder::write(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::Write, state);
}

PassBuilder& PassBuilder::readWrite(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::ReadWrite, state);
}

PassBuilder& PassBuilder::dependsOn(PassHandle pass) {
    m_graph.addDependency(m_pass, pass);
    return *this;
}

PassBuilder& PassBuilder::use(ResourceHandle resource, Access access, ResourceState state) {
    m_graph.addUse(m_pass, {resource, access, state});
    return *this;
}

ResourceHandle RenderGraph::createResource(ResourceDesc desc) {
    if (desc.name.empty()) throw std::invalid_argument("RenderGraph resource name 不能为空");
    desc.transient = true;
    const ResourceHandle handle{static_cast<std::uint32_t>(m_resources.size())};
    m_resources.push_back(std::move(desc));
    return handle;
}

ResourceHandle RenderGraph::importResource(ResourceDesc desc) {
    if (desc.name.empty()) throw std::invalid_argument("RenderGraph resource name 不能为空");
    desc.transient = false;
    const ResourceHandle handle{static_cast<std::uint32_t>(m_resources.size())};
    m_resources.push_back(std::move(desc));
    return handle;
}

PassHandle RenderGraph::addPass(std::string name, Setup setup, Execute execute) {
    if (name.empty()) throw std::invalid_argument("RenderGraph pass name 不能为空");
    const PassHandle handle{static_cast<std::uint32_t>(m_passes.size())};
    m_passes.push_back({std::move(name), {}, {}, std::move(execute)});
    PassBuilder builder(*this, handle);
    if (setup) setup(builder);
    return handle;
}

const ResourceDesc& RenderGraph::resource(ResourceHandle handle) const {
    if (handle.id >= m_resources.size()) throw std::out_of_range("无效的 RenderGraph resource handle");
    return m_resources[handle.id];
}

void RenderGraph::addUse(PassHandle pass, ResourceUse use) {
    if (pass.id >= m_passes.size()) throw std::out_of_range("无效的 RenderGraph pass handle");
    (void)resource(use.resource);
    auto& uses = m_passes[pass.id].uses;
    if (std::any_of(uses.begin(), uses.end(), [&](const ResourceUse& item) {
            return item.resource == use.resource;
        })) {
        throw std::logic_error("同一个 pass 不能重复声明同一 resource");
    }
    uses.push_back(use);
}

void RenderGraph::addDependency(PassHandle pass, PassHandle dependency) {
    if (pass.id >= m_passes.size() || dependency.id >= m_passes.size()) {
        throw std::out_of_range("无效的 RenderGraph dependency handle");
    }
    if (pass == dependency) throw std::logic_error("RenderGraph pass 不能依赖自身");
    addUnique(m_passes[pass.id].explicitDependencies, dependency);
}

CompiledGraph RenderGraph::compile() const {
    const std::size_t passCount = m_passes.size();
    std::vector<std::vector<PassHandle>> dependencies(passCount);
    std::vector<PassHandle> lastWriter(m_resources.size());
    std::vector<std::vector<PassHandle>> readers(m_resources.size());

    for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex) {
        const PassHandle pass{passIndex};
        dependencies[passIndex] = m_passes[passIndex].explicitDependencies;
        for (const ResourceUse& use : m_passes[passIndex].uses) {
            const std::uint32_t resourceIndex = use.resource.id;
            if (use.access == Access::Read) {
                if (lastWriter[resourceIndex]) {
                    addUnique(dependencies[passIndex], lastWriter[resourceIndex]);
                }
                addUnique(readers[resourceIndex], pass);
            } else {
                if (lastWriter[resourceIndex]) {
                    addUnique(dependencies[passIndex], lastWriter[resourceIndex]);
                }
                for (PassHandle reader : readers[resourceIndex]) {
                    addUnique(dependencies[passIndex], reader);
                }
                readers[resourceIndex].clear();
                lastWriter[resourceIndex] = pass;
            }
        }
    }

    std::vector<std::vector<PassHandle>> outgoing(passCount);
    std::vector<std::uint32_t> indegree(passCount, 0);
    for (std::uint32_t passIndex = 0; passIndex < passCount; ++passIndex) {
        for (PassHandle dependency : dependencies[passIndex]) {
            outgoing[dependency.id].push_back({passIndex});
            ++indegree[passIndex];
        }
    }

    std::priority_queue<std::uint32_t, std::vector<std::uint32_t>, std::greater<>> ready;
    for (std::uint32_t i = 0; i < passCount; ++i) {
        if (indegree[i] == 0) ready.push(i);
    }
    std::vector<PassHandle> order;
    while (!ready.empty()) {
        const std::uint32_t current = ready.top();
        ready.pop();
        order.push_back({current});
        for (PassHandle next : outgoing[current]) {
            if (--indegree[next.id] == 0) ready.push(next.id);
        }
    }
    if (order.size() != passCount) {
        throw std::logic_error("RenderGraph 包含有向环，无法拓扑排序");
    }

    CompiledGraph result;
    result.m_resources = m_resources;
    result.m_uses.resize(passCount);
    struct PreviousUse {
        bool valid = false;
        PassHandle pass;
        Access access = Access::Read;
        ResourceState state{};
    };
    std::vector<PreviousUse> previous(m_resources.size());
    struct LastWriter {
        bool valid = false;
        PassHandle pass;
        ResourceState state{};
        VkPipelineStageFlags visibleStages = 0;
        VkAccessFlags visibleAccess = 0;
    };
    std::vector<LastWriter> writers(m_resources.size());
    std::vector<std::uint32_t> position(passCount);

    for (std::uint32_t orderIndex = 0; orderIndex < order.size(); ++orderIndex) {
        const PassHandle handle = order[orderIndex];
        position[handle.id] = orderIndex;
        const PassNode& node = m_passes[handle.id];
        CompiledPass compiled{handle, node.name, dependencies[handle.id], {}, node.execute};
        result.m_uses[handle.id] = node.uses;
        for (const ResourceUse& use : node.uses) {
            PreviousUse& before = previous[use.resource.id];
            LastWriter& writer = writers[use.resource.id];
            const ResourceState source = before.valid
                ? before.state
                : m_resources[use.resource.id].initialState;
            const bool firstTransition = !before.valid &&
                (source.layout != use.state.layout || source.access != use.state.access);
            const bool hazard = before.valid &&
                (writes(before.access) || writes(use.access) || source.layout != use.state.layout);
            const bool writerNotVisible = use.access == Access::Read && writer.valid &&
                (((writer.visibleStages & use.state.stages) != use.state.stages) ||
                 ((writer.visibleAccess & use.state.access) != use.state.access));
            if (writerNotVisible && !writes(before.access) &&
                before.state.layout == use.state.layout) {
                compiled.barriers.push_back({use.resource, writer.pass, handle,
                                             writer.state, use.state});
                writer.visibleStages |= use.state.stages;
                writer.visibleAccess |= use.state.access;
            } else if (firstTransition || hazard) {
                compiled.barriers.push_back({use.resource,
                    before.valid ? before.pass : PassHandle{}, handle, source, use.state});
                if (writer.valid && !writes(use.access)) {
                    writer.visibleStages |= use.state.stages;
                    writer.visibleAccess |= use.state.access;
                }
            }
            if (writes(use.access)) {
                writer = {true, handle, use.state, 0, 0};
            }
            before = {true, handle, use.access, use.state};
        }
        result.m_passes.push_back(std::move(compiled));
    }

    const std::uint32_t unused = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> first(m_resources.size(), unused);
    std::vector<std::uint32_t> last(m_resources.size(), 0);
    for (PassHandle pass : order) {
        for (const ResourceUse& use : m_passes[pass.id].uses) {
            first[use.resource.id] = std::min(first[use.resource.id], position[pass.id]);
            last[use.resource.id] = std::max(last[use.resource.id], position[pass.id]);
        }
    }

    struct Slot {
        ResourceKind kind;
        std::uint64_t key;
        std::uint64_t capacity;
        std::uint32_t lastUse;
        bool reusable;
    };
    std::vector<Slot> slots;
    for (std::uint32_t resourceIndex = 0; resourceIndex < m_resources.size(); ++resourceIndex) {
        if (first[resourceIndex] == unused) continue;
        const ResourceDesc& desc = m_resources[resourceIndex];
        std::uint32_t slotIndex = unused;
        if (desc.transient) {
            for (std::uint32_t i = 0; i < slots.size(); ++i) {
                Slot& slot = slots[i];
                if (slot.reusable && slot.kind == desc.kind &&
                    slot.key == desc.compatibilityKey && slot.lastUse < first[resourceIndex]) {
                    slotIndex = i;
                    slot.capacity = std::max(slot.capacity, desc.sizeBytes);
                    slot.lastUse = last[resourceIndex];
                    break;
                }
            }
        }
        if (slotIndex == unused) {
            slotIndex = static_cast<std::uint32_t>(slots.size());
            slots.push_back({desc.kind, desc.compatibilityKey, desc.sizeBytes,
                             last[resourceIndex], desc.transient});
        }
        result.m_lifetimes.push_back({{resourceIndex}, first[resourceIndex],
                                      last[resourceIndex], slotIndex});
    }
    return result;
}

void CompiledGraph::execute(VkCommandBuffer commandBuffer) const {
    for (const CompiledPass& pass : m_passes) {
        if (commandBuffer != VK_NULL_HANDLE) {
            for (const Barrier& barrier : pass.barriers) {
                const ResourceDesc& resource = m_resources[barrier.resource.id];
                // 第一次使用的 layout transition 仍由创建资源或 render pass 负责；
                // Render Graph 接管的是 pass 之间的 hazard。没有实体 image 的资源
                // 是 render-pass subpass/input attachment，由原生 subpass dependency 管理。
                if (!barrier.before || resource.image == VK_NULL_HANDLE) continue;
                VkImageMemoryBarrier imageBarrier{};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageBarrier.srcAccessMask = barrier.source.access;
                imageBarrier.dstAccessMask = barrier.destination.access;
                imageBarrier.oldLayout = barrier.source.layout;
                imageBarrier.newLayout = barrier.destination.layout;
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.image = resource.image;
                imageBarrier.subresourceRange.aspectMask = resource.aspectMask;
                imageBarrier.subresourceRange.levelCount = 1;
                imageBarrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(commandBuffer,
                                     barrier.source.stages,
                                     barrier.destination.stages,
                                     0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
            }
        }
        if (pass.execute) pass.execute(commandBuffer);
    }
}

std::string CompiledGraph::toDot() const {
    std::ostringstream out;
    out << "digraph RenderGraph {\n  rankdir=LR;\n";
    for (std::uint32_t i = 0; i < m_resources.size(); ++i) {
        out << "  r" << i << " [shape=ellipse,label=\""
            << dotEscape(m_resources[i].name) << "\"];\n";
    }
    for (const CompiledPass& pass : m_passes) {
        out << "  p" << pass.handle.id << " [shape=box,label=\""
            << dotEscape(pass.name) << "\"];\n";
        for (const ResourceUse& use : m_uses[pass.handle.id]) {
            if (use.access == Access::Read) {
                out << "  r" << use.resource.id << " -> p" << pass.handle.id;
            } else {
                out << "  p" << pass.handle.id << " -> r" << use.resource.id;
            }
            out << " [label=\"" << accessLabel(use.access) << "\"];\n";
        }
    }
    out << "}\n";
    return out.str();
}

} // namespace rwb::rg
