#include "rwb/rendergraph/RenderGraph.h"

#include "rwb/core/Todo.h"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace rwb::rg {
namespace {

void addUnique(std::vector<PassHandle>& values, PassHandle value) {
    if (std::none_of(values.begin(), values.end(),
                     [value](PassHandle item) { return item == value; })) {
        values.push_back(value);
    }
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

PassBuilder& PassBuilder::use(ResourceHandle resource, Access access,
                              ResourceState state) {
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
    if (handle.id >= m_resources.size()) {
        throw std::out_of_range("无效的 RenderGraph resource handle");
    }
    return m_resources[handle.id];
}

void RenderGraph::addUse(PassHandle pass, ResourceUse use) {
    if (pass.id >= m_passes.size()) {
        throw std::out_of_range("无效的 RenderGraph pass handle");
    }
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
    for (std::uint32_t index = 0; index < passCount; ++index) {
        if (indegree[index] == 0) ready.push(index);
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
    for (PassHandle handle : order) {
        const PassNode& node = m_passes[handle.id];
        result.m_uses[handle.id] = node.uses;
        result.m_passes.push_back(
            {handle, node.name, dependencies[handle.id], {}, node.execute});
    }
    return result;
}

void CompiledGraph::execute(VkCommandBuffer commandBuffer) const {
    for (const CompiledPass& pass : m_passes) {
        if (pass.execute) pass.execute(commandBuffer);
    }
}

std::string CompiledGraph::toDot() const {
    RWB_TODO("p05-t06 CompiledGraph::toDot");
}

} // namespace rwb::rg
