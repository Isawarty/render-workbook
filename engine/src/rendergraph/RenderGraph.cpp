#include "rwb/rendergraph/RenderGraph.h"

#include "rwb/core/Todo.h"

#include <algorithm>
#include <stdexcept>

namespace rwb::rg {

PassBuilder& PassBuilder::read(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::Read, state);
}

PassBuilder& PassBuilder::write(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::Write, state);
}

PassBuilder& PassBuilder::readWrite(ResourceHandle resource, ResourceState state) {
    return use(resource, Access::ReadWrite, state);
}

PassBuilder& PassBuilder::dependsOn(PassHandle) {
    RWB_TODO("p05-t02 PassBuilder::dependsOn");
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

void RenderGraph::addDependency(PassHandle, PassHandle) {
    RWB_TODO("p05-t02 RenderGraph::addDependency");
}

CompiledGraph RenderGraph::compile() const {
    CompiledGraph result;
    result.m_resources = m_resources;
    result.m_uses.resize(m_passes.size());
    for (std::uint32_t index = 0; index < m_passes.size(); ++index) {
        const PassNode& node = m_passes[index];
        result.m_uses[index] = node.uses;
        result.m_passes.push_back({{index}, node.name, {}, {}, node.execute});
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
