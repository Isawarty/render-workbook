#include "rwb/rendergraph/RenderGraph.h"

#include "rwb/core/Todo.h"

namespace rwb::rg {

PassBuilder& PassBuilder::read(ResourceHandle, ResourceState) {
    RWB_TODO("p05-t01 PassBuilder::read");
}

PassBuilder& PassBuilder::write(ResourceHandle, ResourceState) {
    RWB_TODO("p05-t01 PassBuilder::write");
}

PassBuilder& PassBuilder::readWrite(ResourceHandle, ResourceState) {
    RWB_TODO("p05-t01 PassBuilder::readWrite");
}

PassBuilder& PassBuilder::dependsOn(PassHandle) {
    RWB_TODO("p05-t02 PassBuilder::dependsOn");
}

PassBuilder& PassBuilder::use(ResourceHandle, Access, ResourceState) {
    RWB_TODO("p05-t01 PassBuilder::use");
}

ResourceHandle RenderGraph::createResource(ResourceDesc) {
    RWB_TODO("p05-t01 RenderGraph::createResource");
}

ResourceHandle RenderGraph::importResource(ResourceDesc) {
    RWB_TODO("p05-t01 RenderGraph::importResource");
}

PassHandle RenderGraph::addPass(std::string, Setup, Execute) {
    RWB_TODO("p05-t01 RenderGraph::addPass");
}

const ResourceDesc& RenderGraph::resource(ResourceHandle) const {
    RWB_TODO("p05-t01 RenderGraph::resource");
}

void RenderGraph::addUse(PassHandle, ResourceUse) {
    RWB_TODO("p05-t01 RenderGraph::addUse");
}

void RenderGraph::addDependency(PassHandle, PassHandle) {
    RWB_TODO("p05-t02 RenderGraph::addDependency");
}

CompiledGraph RenderGraph::compile() const {
    RWB_TODO("p05-t01 RenderGraph::compile");
}

void CompiledGraph::execute(VkCommandBuffer) const {
    RWB_TODO("p05-t01 CompiledGraph::execute");
}

std::string CompiledGraph::toDot() const {
    RWB_TODO("p05-t06 CompiledGraph::toDot");
}

} // namespace rwb::rg
