// t02 —— geometry pass：真实 glTF 网格 + PBR 贴图写入三张 MRT
#include "../DeferredApp.h"
#include "rwb/core/Log.h"
#include "rwb/rhi/Shader.h"
#include "rwb/rhi/VmaUsage.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <tiny_gltf.h>
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

namespace p04 {
namespace {

struct HostBuffer {
    const Context* context = nullptr;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void* mapped = nullptr;

    HostBuffer(const Context& ctx, VkDeviceSize size) : context(&ctx) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo result{};
        VK_CHECK(vmaCreateBuffer(ctx.allocator(), &info, &allocationInfo,
                                 &handle, &allocation, &result));
        mapped = result.pMappedData;
    }
    ~HostBuffer() {
        if (handle != VK_NULL_HANDLE) {
            vmaDestroyBuffer(context->allocator(), handle, allocation);
        }
    }
};

const unsigned char* accessorData(const tinygltf::Model& model,
                                  const tinygltf::Accessor& accessor) {
    const auto& view = model.bufferViews.at(static_cast<std::size_t>(accessor.bufferView));
    const auto& buffer = model.buffers.at(static_cast<std::size_t>(view.buffer));
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

std::size_t accessorStride(const tinygltf::Model& model,
                           const tinygltf::Accessor& accessor,
                           std::size_t packedSize) {
    const auto& view = model.bufferViews.at(static_cast<std::size_t>(accessor.bufferView));
    return view.byteStride == 0 ? packedSize : view.byteStride;
}

std::vector<float> readFloatAccessor(const tinygltf::Model& model, int index,
                                     int components) {
    const auto& accessor = model.accessors.at(static_cast<std::size_t>(index));
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        throw std::runtime_error("Sci-Fi Helmet 顶点属性不是 float");
    }
    const std::size_t elementSize = static_cast<std::size_t>(components) * sizeof(float);
    const std::size_t stride = accessorStride(model, accessor, elementSize);
    const unsigned char* source = accessorData(model, accessor);
    std::vector<float> result(accessor.count * static_cast<std::size_t>(components));
    for (std::size_t i = 0; i < accessor.count; ++i) {
        std::memcpy(result.data() + i * static_cast<std::size_t>(components),
                    source + i * stride, elementSize);
    }
    return result;
}

std::vector<std::uint32_t> readIndices(const tinygltf::Model& model, int index) {
    const auto& accessor = model.accessors.at(static_cast<std::size_t>(index));
    const unsigned char* source = accessorData(model, accessor);
    const std::size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const std::size_t stride = accessorStride(model, accessor, componentSize);
    std::vector<std::uint32_t> result(accessor.count);
    for (std::size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* p = source + i * stride;
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            result[i] = *p;
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            std::uint16_t value = 0;
            std::memcpy(&value, p, sizeof(value));
            result[i] = value;
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            std::memcpy(&result[i], p, sizeof(result[i]));
        } else {
            throw std::runtime_error("Sci-Fi Helmet 索引类型不受支持");
        }
    }
    return result;
}

std::vector<std::uint8_t> rgbaPixels(const tinygltf::Image& image) {
    if (image.width <= 0 || image.height <= 0 || image.component <= 0) {
        throw std::runtime_error("PBR 贴图解码结果为空");
    }
    const std::size_t pixelCount = static_cast<std::size_t>(image.width) * image.height;
    std::vector<std::uint8_t> result(pixelCount * 4, 255);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const auto* src = image.image.data() + i * static_cast<std::size_t>(image.component);
        result[i * 4 + 0] = src[0];
        result[i * 4 + 1] = image.component > 1 ? src[1] : src[0];
        result[i * 4 + 2] = image.component > 2 ? src[2] : src[0];
        result[i * 4 + 3] = image.component > 3 ? src[3] : 255;
    }
    return result;
}

void imageBarrier(VkCommandBuffer cmd, VkImage image,
                  VkImageLayout oldLayout, VkImageLayout newLayout,
                  VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                  VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

void DeferredApp::loadModelResources() {
    if (m_modelLoaded) return;
    const std::string modelPath = std::string(RWB_P04_ASSET_DIR) +
                                  "/scifi-helmet/SciFiHelmet.gltf";
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error;
    std::string warning;
    if (!loader.LoadASCIIFromFile(&model, &error, &warning, modelPath)) {
        throw std::runtime_error("glTF 加载失败: " + error);
    }
    if (!warning.empty()) rwb::logWarn("glTF: " + warning);
    if (model.meshes.size() != 1 || model.meshes[0].primitives.size() != 1 ||
        model.images.size() != 4) {
        throw std::runtime_error("Sci-Fi Helmet 资产契约发生变化");
    }

    const auto& primitive = model.meshes[0].primitives[0];
    auto attribute = [&](const char* name) {
        const auto found = primitive.attributes.find(name);
        if (found == primitive.attributes.end()) {
            throw std::runtime_error(std::string("Sci-Fi Helmet 缺少属性: ") + name);
        }
        return found->second;
    };
    const auto positions = readFloatAccessor(model, attribute("POSITION"), 3);
    const auto normals = readFloatAccessor(model, attribute("NORMAL"), 3);
    const auto tangents = readFloatAccessor(model, attribute("TANGENT"), 4);
    const auto uvs = readFloatAccessor(model, attribute("TEXCOORD_0"), 2);
    auto indices = readIndices(model, primitive.indices);
    const std::size_t vertexCount = positions.size() / 3;
    std::vector<ModelVertex> vertices(vertexCount);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        vertices[i].position = {positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]};
        vertices[i].normal = {normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
        vertices[i].tangent = {tangents[i * 4], tangents[i * 4 + 1],
                               tangents[i * 4 + 2], tangents[i * 4 + 3]};
        vertices[i].uv = {uvs[i * 2], uvs[i * 2 + 1]};
    }

    m_modelVertexCount = static_cast<std::uint32_t>(vertices.size());
    m_modelIndexCount = static_cast<std::uint32_t>(indices.size());
    m_groundFirstIndex = static_cast<std::uint32_t>(indices.size());
    constexpr float groundY = -1.5f;
    const std::array<ModelVertex, 4> ground{{
        {{-7.0f, groundY, -7.0f}, {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, 1.0f},
        {{ 7.0f, groundY, -7.0f}, {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, 1.0f},
        {{ 7.0f, groundY,  7.0f}, {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, 1.0f},
        {{-7.0f, groundY,  7.0f}, {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, 1.0f},
    }};
    const std::uint32_t groundBase = static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), ground.begin(), ground.end());
    const std::array<std::uint32_t, 6> groundIndices{{
        groundBase + 0, groundBase + 2, groundBase + 1,
        groundBase + 0, groundBase + 3, groundBase + 2,
    }};
    indices.insert(indices.end(), groundIndices.begin(), groundIndices.end());
    m_groundIndexCount = static_cast<std::uint32_t>(groundIndices.size());

    auto uploadBuffer = [&](const void* source, VkDeviceSize size,
                            VkBufferUsageFlags usage) {
        HostBuffer staging(*m_ctx, size);
        std::memcpy(staging.mapped, source, static_cast<std::size_t>(size));
        Buffer destination = createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy copy{0, 0, size};
            vkCmdCopyBuffer(cmd, staging.handle, destination.handle, 1, &copy);
        });
        return destination;
    };
    m_modelVertices = uploadBuffer(vertices.data(), vertices.size() * sizeof(ModelVertex),
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_modelIndices = uploadBuffer(indices.data(), indices.size() * sizeof(std::uint32_t),
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    for (std::size_t i = 0; i < m_materialTextures.size(); ++i) {
        const auto pixels = rgbaPixels(model.images[i]);
        const VkFormat format = i == 0 ? VK_FORMAT_R8G8B8A8_SRGB
                                       : VK_FORMAT_R8G8B8A8_UNORM;
        Image& texture = m_materialTextures[i];
        texture = createImage(static_cast<std::uint32_t>(model.images[i].width),
                              static_cast<std::uint32_t>(model.images[i].height), format,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_IMAGE_ASPECT_COLOR_BIT);
        HostBuffer staging(*m_ctx, pixels.size());
        std::memcpy(staging.mapped, pixels.data(), pixels.size());
        m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, texture.handle, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent = {texture.width, texture.height, 1};
            vkCmdCopyBufferToImage(cmd, staging.handle, texture.handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            imageBarrier(cmd, texture.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }

    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.maxLod = 0.0f;
    VK_CHECK(vkCreateSampler(m_ctx->device(), &sampler, nullptr, &m_materialSampler));

    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    for (std::uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layout.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &layout, nullptr,
                                         &m_materialSetLayout));
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4};
    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = 1;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &pool, nullptr, &m_materialPool));
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = m_materialPool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &m_materialSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &allocate, &m_materialSet));
    std::array<VkDescriptorImageInfo, 4> imageInfos{};
    std::array<VkWriteDescriptorSet, 4> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        imageInfos[i] = {m_materialSampler, m_materialTextures[i].view,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_materialSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imageInfos[i];
    }
    vkUpdateDescriptorSets(m_ctx->device(), static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    m_modelLoaded = true;
    rwb::logInfo(rwb::format("P04 Sci-Fi Helmet: %u vertices, %u indices, 4 PBR textures",
                             m_modelVertexCount, m_modelIndexCount));
}

void DeferredApp::createGeometryPipeline() {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.size = sizeof(glm::mat4) * 2;
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_materialSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_geometryLayout));

    const std::string dir = m_useSlang ? std::string(RWB_SLANG_SHADER_DIR) + "/"
                                       : std::string(RWB_SHADER_DIR) + "/";
    const std::string suffix = m_useSlang ? ".slang.spv" : ".spv";
    auto vert = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + "geometry.vert" + suffix);
    auto frag = rwb::rhi::ShaderModule::fromFile(m_ctx->device(), dir + "geometry.frag" + suffix);
    const VkPipelineShaderStageCreateInfo stages[] = {
        vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT), frag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    VkVertexInputBindingDescription binding{0, sizeof(ModelVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 5> attributes{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ModelVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ModelVertex, normal)},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelVertex, tangent)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ModelVertex, uv)},
        {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(ModelVertex, materialKind)},
    }};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkExtent2D extent = m_swapchain->extent();
    VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    std::array<VkPipelineColorBlendAttachmentState, 3> attachments{};
    for (auto& attachment : attachments) {
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    blend.pAttachments = attachments.data();
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &msaa;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.layout = m_geometryLayout;
    info.renderPass = m_renderPass;
    info.subpass = kGeometrySubpass;
    VK_CHECK(vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &info, nullptr,
                                       &m_geometryPipeline));
}

} // namespace p04
