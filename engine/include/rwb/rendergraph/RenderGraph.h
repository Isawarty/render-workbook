#pragma once

#include <volk.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace rwb::rg {

struct ResourceHandle {
    std::uint32_t id = UINT32_MAX;
    explicit operator bool() const { return id != UINT32_MAX; }
};

struct PassHandle {
    std::uint32_t id = UINT32_MAX;
    explicit operator bool() const { return id != UINT32_MAX; }
};

inline bool operator==(ResourceHandle lhs, ResourceHandle rhs) { return lhs.id == rhs.id; }
inline bool operator==(PassHandle lhs, PassHandle rhs) { return lhs.id == rhs.id; }

enum class ResourceKind { Image, Buffer };
enum class Access { Read, Write, ReadWrite };

struct ResourceState {
    VkPipelineStageFlags stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags access = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct ResourceDesc {
    std::string name;
    ResourceKind kind = ResourceKind::Image;
    bool transient = true;
    std::uint64_t sizeBytes = 0;
    std::uint64_t compatibilityKey = 0;
    ResourceState initialState{};
    VkImage image = VK_NULL_HANDLE;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags imageUsage = 0;
};

struct ResourceUse {
    ResourceHandle resource;
    Access access = Access::Read;
    ResourceState state{};
};

struct Barrier {
    ResourceHandle resource;
    PassHandle before;
    PassHandle after;
    ResourceState source{};
    ResourceState destination{};
};

struct Lifetime {
    ResourceHandle resource;
    std::uint32_t firstUse = 0;
    std::uint32_t lastUse = 0;
    std::uint32_t physicalSlot = 0;
};

struct CompiledPass {
    PassHandle handle;
    std::string name;
    std::vector<PassHandle> dependencies;
    std::vector<Barrier> barriers;
    std::function<void(VkCommandBuffer)> execute;
};

class RenderGraph;

class PassBuilder {
public:
    PassBuilder& read(ResourceHandle resource, ResourceState state);
    PassBuilder& write(ResourceHandle resource, ResourceState state);
    PassBuilder& readWrite(ResourceHandle resource, ResourceState state);
    PassBuilder& dependsOn(PassHandle pass);

private:
    friend class RenderGraph;
    PassBuilder(RenderGraph& graph, PassHandle pass) : m_graph(graph), m_pass(pass) {}
    PassBuilder& use(ResourceHandle resource, Access access, ResourceState state);

    RenderGraph& m_graph;
    PassHandle m_pass;
};

class CompiledGraph {
public:
    const std::vector<CompiledPass>& passes() const { return m_passes; }
    const std::vector<Lifetime>& lifetimes() const { return m_lifetimes; }
    const std::vector<ResourceDesc>& resources() const { return m_resources; }
    void execute(VkCommandBuffer commandBuffer) const;
    std::string toDot() const;

private:
    friend class RenderGraph;
    std::vector<ResourceDesc> m_resources;
    std::vector<CompiledPass> m_passes;
    std::vector<Lifetime> m_lifetimes;
    std::vector<std::vector<ResourceUse>> m_uses;
};

class RenderGraph {
public:
    using Setup = std::function<void(PassBuilder&)>;
    using Execute = std::function<void(VkCommandBuffer)>;

    ResourceHandle createResource(ResourceDesc desc);
    ResourceHandle importResource(ResourceDesc desc);
    PassHandle addPass(std::string name, Setup setup, Execute execute = {});
    void addDependency(PassHandle pass, PassHandle dependency);
    CompiledGraph compile() const;

    const ResourceDesc& resource(ResourceHandle handle) const;

private:
    friend class PassBuilder;
    struct PassNode {
        std::string name;
        std::vector<ResourceUse> uses;
        std::vector<PassHandle> explicitDependencies;
        Execute execute;
    };

    void addUse(PassHandle pass, ResourceUse use);
    std::vector<ResourceDesc> m_resources;
    std::vector<PassNode> m_passes;
};

} // namespace rwb::rg
