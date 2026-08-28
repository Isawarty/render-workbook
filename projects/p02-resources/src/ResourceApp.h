#pragma once

#include "rwb/rhi/Context.h"
#include "rwb/rhi/FrameRenderer.h"
#include "rwb/rhi/Shader.h"
#include "rwb/rhi/Swapchain.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// VMA 句柄的前向声明。用到 vmaCreateBuffer 的 .cpp 自己 include rwb/rhi/VmaUsage.h。
VK_DEFINE_HANDLE(VmaAllocation)

namespace p02 {

using rwb::rhi::Context;
using rwb::rhi::FrameRenderer;
using rwb::rhi::ShaderModule;
using rwb::rhi::Swapchain;

// P2 的八个阶段，正好对应八道题。
// 和 P1 一样，测试可以只驱动到某一段，不会因为后面的题没做而失败。
enum class Stage {
    Buffers       = 1,  // t01: VMA + vertex/index buffer + staging 上传
    Uniforms      = 2,  // t02: UBO + descriptor set layout / pool / set
    PushConstants = 3,  // t03: push constant，与 UBO 做对比
    Texture       = 4,  // t04: image 创建 + staging 上传 + layout transition
    Sampler       = 5,  // t05: image view + sampler + mipmap 生成
    Depth         = 6,  // t06: 深度缓冲 + depth test
    Model         = 7,  // t07: glTF 加载 + 多 mesh 绘制
    Msaa          = 8,  // t08: 多重采样
};

// 阶段比较用。写成自由函数，好让 src/steps/ 里的各个文件都能用。
inline int stageIndex(Stage s) { return static_cast<int>(s); }

// ---------------------------------------------------------------------------
// 顶点。
//
// P1 的三角形顶点是硬编码在 vertex shader 里的（gl_VertexIndex 查表）。
// 从这里开始，顶点是真正的 GPU 内存里的数据，需要你告诉管线怎么解读它。
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;

    // 「一个顶点占多少字节、按什么频率前进」
    static VkVertexInputBindingDescription bindingDescription();
    // 「每个 location 从这个结构的哪个偏移、按什么格式读」
    static std::vector<VkVertexInputAttributeDescription> attributeDescriptions();

    bool operator==(const Vertex& o) const {
        return pos == o.pos && color == o.color && uv == o.uv;
    }
};

// 每帧更新一次的相机数据。放 UBO 里（t02）。
struct CameraUniform {
    glm::mat4 view;
    glm::mat4 proj;
};

// 每次 draw call 都可能不同的数据。放 push constant 里（t03）。
//
// 64 字节，正好卡在规范保证的 128 字节下限之内 —— 这是 push constant
// 能不能用的硬约束，t03 的任务书会展开讲。
struct ObjectPush {
    glm::mat4 model;
};

// --- VMA 资源的薄封装。你在 t01/t04 亲手把它们填出来 --------------------
struct Buffer {
    VkBuffer      handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize  size       = 0;
    void*         mapped     = nullptr;   // 只有持久映射的 buffer 才非空

    bool valid() const { return handle != VK_NULL_HANDLE; }
};

struct Image {
    VkImage       handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view       = VK_NULL_HANDLE;
    VkFormat      format     = VK_FORMAT_UNDEFINED;
    std::uint32_t width      = 0;
    std::uint32_t height     = 0;
    std::uint32_t mipLevels  = 1;

    bool valid() const { return handle != VK_NULL_HANDLE; }
};

// 一次 draw 的单位：索引缓冲里的一段 + 一个 model 矩阵。
struct MeshDraw {
    std::uint32_t firstIndex  = 0;
    std::uint32_t indexCount  = 0;
    std::int32_t  vertexOffset = 0;
    glm::mat4     model{1.0f};
};

struct AppConfig {
    std::uint32_t width            = 800;
    std::uint32_t height           = 600;
    bool          enableValidation = true;
    std::string   title            = "render-workbook P02 resources";
};

// ---------------------------------------------------------------------------
class ResourceApp {
public:
    explicit ResourceApp(AppConfig config = {});
    ~ResourceApp();

    ResourceApp(const ResourceApp&)            = delete;
    ResourceApp& operator=(const ResourceApp&) = delete;

    void initUpTo(Stage stage);
    void run(int frameCount = -1);
    rwb::rhi::CapturedImage renderAndCapture();

    // --- 供测试检查内部状态 ----------------------------------------------
    Context&       context()    { return *m_ctx; }
    Swapchain&     swapchain()  { return *m_swapchain; }
    FrameRenderer& renderer()   { return *m_renderer; }

    const Buffer& vertexBuffer() const { return m_vertexBuffer; }
    const Buffer& indexBuffer()  const { return m_indexBuffer; }
    const std::vector<Buffer>& uniformBuffers() const { return m_uniformBuffers; }
    VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorPool      descriptorPool()      const { return m_descriptorPool; }
    const std::vector<VkDescriptorSet>& descriptorSets() const { return m_descriptorSets; }
    const Image&   texture()       const { return m_texture; }
    VkSampler      sampler()       const { return m_sampler; }
    const Image&   depthImage()    const { return m_depth; }
    const Image&   colorTarget()   const { return m_colorTarget; }
    VkSampleCountFlagBits sampleCount() const { return m_sampleCount; }
    VkRenderPass   renderPass()    const { return m_renderPass; }
    VkPipeline     pipeline()      const { return m_pipeline; }
    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }

    const std::vector<Vertex>&      vertices() const { return m_vertices; }
    const std::vector<std::uint32_t>& indices() const { return m_indices; }
    const std::vector<MeshDraw>&    draws()    const { return m_draws; }
    std::string deviceName() const { return m_ctx ? m_ctx->deviceName() : std::string{}; }

    // 让测试能读到你写的 push constant 范围声明（t03）
    std::vector<VkPushConstantRange> pushConstantRangesForTest() const;

    static constexpr std::uint32_t kFramesInFlight = 2;
    // t04/t05 用的程序化棋盘纹理尺寸。取 2 的幂，好让 mip 链算得整齐。
    static constexpr std::uint32_t kTextureSize = 256;

private:
    // ====== t01 —— src/steps/01_buffers.cpp ======
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                        bool hostVisible, bool persistentlyMapped);
    void   destroyBuffer(Buffer& buffer) noexcept;
    // 通过 staging buffer 把 CPU 数据搬进 device-local buffer
    void   uploadViaStaging(const void* data, VkDeviceSize size, const Buffer& dst);
    void   createVertexBuffer();
    void   createIndexBuffer();

    // ====== t02 —— src/steps/02_uniforms.cpp ======
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(std::uint32_t frameIndex);

    // ====== t03 —— src/steps/03_pushconstants.cpp ======
    std::vector<VkPushConstantRange> pushConstantRanges() const;
    void pushObjectData(VkCommandBuffer cmd, const ObjectPush& data) const;

    // ====== t04 —— src/steps/04_texture.cpp ======
    Image createImage(std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels,
                      VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                      VkImageUsageFlags usage);
    void  destroyImage(Image& image) noexcept;
    void  transitionImageLayout(VkCommandBuffer cmd, const Image& image,
                                VkImageLayout oldLayout, VkImageLayout newLayout,
                                VkImageAspectFlags aspect);
    void  copyBufferToImage(VkCommandBuffer cmd, const Buffer& src, const Image& dst);
    void  createTextureImage();

    // ====== t05 —— src/steps/05_sampler.cpp ======
    VkImageView createImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspect, std::uint32_t mipLevels);
    void        generateMipmaps(VkCommandBuffer cmd, const Image& image);
    void        createTextureImageView();
    void        createTextureSampler();

    // ====== t06 —— src/steps/06_depth.cpp ======
    VkFormat findDepthFormat() const;
    void     createDepthResources();

    // ====== t07 —— src/steps/07_model.cpp ======
    void loadModel(const std::string& path);

    // ====== t08 —— src/steps/08_msaa.cpp ======
    VkSampleCountFlagBits maxUsableSampleCount() const;
    void                  createColorResources();

    // ====== 框架提供（P1 已经教过，或不是教学点） =========================
    void buildScene(Stage stage);
    void createRenderPass();
    void createFramebuffers();
    void createGraphicsPipeline();
    void recordFrame(VkCommandBuffer cmd, std::uint32_t imageIndex, std::uint32_t frameIndex);
    void onSwapchainResized();
    void destroySizeDependent() noexcept;
    void cleanup() noexcept;
    static std::vector<std::uint8_t> checkerboardPixels(std::uint32_t size);

    AppConfig m_config;
    Stage     m_reached = static_cast<Stage>(0);

    std::unique_ptr<Context>       m_ctx;
    std::unique_ptr<Swapchain>     m_swapchain;
    std::unique_ptr<FrameRenderer> m_renderer;

    // 场景数据（CPU 侧）
    std::vector<Vertex>        m_vertices;
    std::vector<std::uint32_t> m_indices;
    std::vector<MeshDraw>      m_draws;

    // t01
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;

    // t02
    VkDescriptorSetLayout        m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool             m_descriptorPool      = VK_NULL_HANDLE;
    std::vector<Buffer>          m_uniformBuffers;
    std::vector<VkDescriptorSet> m_descriptorSets;

    // t04 / t05
    Image     m_texture;
    VkSampler m_sampler = VK_NULL_HANDLE;

    // t06
    Image m_depth;

    // t08
    Image                 m_colorTarget;
    VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;

    // 框架
    VkRenderPass               m_renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout           m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline                 m_pipeline       = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    float                      m_time = 0.0f;
};

} // namespace p02
