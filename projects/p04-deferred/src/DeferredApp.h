#pragma once

#include "rwb/rhi/Context.h"
#include "rwb/rhi/FrameRenderer.h"
#include "rwb/rhi/Readback.h"
#include "rwb/rhi/Swapchain.h"

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

VK_DEFINE_HANDLE(VmaAllocation)

namespace p04 {

using rwb::rhi::Context;
using rwb::rhi::FrameRenderer;
using rwb::rhi::Swapchain;

enum class Stage : std::uint32_t {
    GBuffer = 1,
    Geometry = 2,
    Lighting = 3,
    Ibl = 4,
    Shadows = 5,
    Bloom = 6,
    Slang = 7,
};

inline int stageIndex(Stage stage) { return static_cast<int>(stage); }

enum class GBufferSlot : std::uint32_t {
    AlbedoMetallic = 0,
    NormalRoughness = 1,
    EmissiveAo = 2,
    Count = 3,
};

struct Image {
    VkImage       handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view       = VK_NULL_HANDLE;
    VkFormat      format     = VK_FORMAT_UNDEFINED;
    std::uint32_t width      = 0;
    std::uint32_t height     = 0;

    bool valid() const { return handle != VK_NULL_HANDLE; }
};

struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;

    bool valid() const { return handle != VK_NULL_HANDLE; }
};

struct ModelVertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec4 tangent{};
    glm::vec2 uv{};
    float materialKind = 0.0f;
};

struct LightingUniform {
    glm::mat4 inverseViewProjection{1.0f};
    glm::mat4 lightViewProjection{1.0f};
    glm::vec4 cameraPosition{0.0f};
    glm::vec4 lightDirection{0.0f};
};

struct FrameMatrices {
    glm::mat4 model{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
    glm::mat4 lightViewProjection{1.0f};
};

struct AttachmentContract {
    GBufferSlot slot{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint32_t bytesPerPixel = 0;
};

struct RawAttachment {
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bytesPerPixel = 0;
    std::vector<std::uint8_t> pixels;
};

struct GBufferCapture {
    RawAttachment albedoMetallic;
    RawAttachment normalRoughness;
    RawAttachment emissiveAo;
};

struct BarrierSnapshot {
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;
};

struct AppConfig {
    std::uint32_t width = 800;
    std::uint32_t height = 600;
    bool enableValidation = true;
    bool highDpiFramebuffer = true;
    bool windowVisible = true;
    bool offscreenCapture = false;
    std::string title = "render-workbook P04 deferred";
};

struct CameraInput {
    float forward = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool boost = false;
    bool reset = false;
};

struct CameraState {
    glm::vec3 position{0.0f, 0.05f, 4.2f};
    float yawDegrees = -90.0f;
    float pitchDegrees = -0.68206f;
};

class DeferredApp {
public:
    using ExternalFrameRecorder =
        std::function<void(VkCommandBuffer, std::uint32_t)>;
    explicit DeferredApp(AppConfig config = {});
    ~DeferredApp();

    DeferredApp(const DeferredApp&) = delete;
    DeferredApp& operator=(const DeferredApp&) = delete;

    void init();
    void initUpTo(Stage stage);
    void run(int frameCount = -1);
    GBufferCapture renderAndCaptureGBuffer();
    rwb::rhi::CapturedImage renderAndCaptureFinal();
    void setExternalFrameRecorder(ExternalFrameRecorder recorder) {
        m_externalFrameRecorder = std::move(recorder);
    }
    static void applyCameraInput(CameraState& camera, const CameraInput& input,
                                 float deltaSeconds);
    static void applyMouseLook(CameraState& camera, float deltaX, float deltaY);
    static FrameMatrices buildFrameMatrices(const CameraState& camera, float aspect);
    static glm::vec3 reconstructWorldPosition(glm::vec2 uv, float depth,
                                              const glm::mat4& inverseViewProjection);
    void setCameraState(const CameraState& camera) { m_camera = camera; }
    const CameraState& cameraState() const { return m_camera; }

    Context& context() { return *m_ctx; }
    Swapchain& swapchain() { return *m_swapchain; }
    const std::vector<Image>& colorAttachments() const { return m_gbuffer; }
    const Image& depthAttachment() const { return m_depth; }
    const std::vector<AttachmentContract>& attachmentContract() const { return m_contract; }
    VkRenderPass renderPass() const { return m_renderPass; }
    const std::vector<VkFramebuffer>& framebuffers() const { return m_framebuffers; }
    Stage stage() const { return m_stage; }
    VkPipeline geometryPipeline() const { return m_geometryPipeline; }
    bool modelLoaded() const { return m_modelLoaded; }
    std::uint32_t modelVertexCount() const { return m_modelVertexCount; }
    std::uint32_t modelIndexCount() const { return m_modelIndexCount; }
    std::uint32_t groundIndexCount() const { return m_groundIndexCount; }
    std::size_t materialTextureCount() const {
        std::size_t count = 0;
        for (const Image& image : m_materialTextures) {
            if (image.valid() && image.view != VK_NULL_HANDLE) ++count;
        }
        return count;
    }
    VkPipeline lightingPipeline() const { return m_lightingPipeline; }
    VkDescriptorSetLayout lightingSetLayout() const { return m_lightingSetLayout; }
    const Buffer& lightingUniformBuffer() const { return m_lightingUniform; }
    const BarrierSnapshot& gbufferToLightingBarrier() const { return m_gbufferBarrier; }
    const Buffer& iblBuffer() const { return m_iblBuffer; }
    VkPipeline iblPipeline() const { return m_iblPipeline; }
    const BarrierSnapshot& iblBarrier() const { return m_iblBarrier; }
    std::vector<float> readbackIbl() const;
    const Image& shadowDepth() const { return m_shadowDepth; }
    VkRenderPass shadowRenderPass() const { return m_shadowRenderPass; }
    VkPipeline shadowPipeline() const { return m_shadowPipeline; }
    VkSampler shadowSampler() const { return m_shadowSampler; }
    std::uint32_t pcfRadius() const { return 1; }
    RawAttachment readbackShadowDepth();
    const Image& hdrImage() const { return m_hdr; }
    const Image& bloomImage() const { return m_bloom; }
    VkPipeline bloomPipeline() const { return m_bloomPipeline; }
    VkPipeline tonemapPipeline() const { return m_postPipeline; }
    const BarrierSnapshot& bloomBarrier() const { return m_bloomBarrier; }
    RawAttachment readbackBloom();
    RawAttachment readbackHdr();
    bool usingSlang() const { return m_useSlang; }

    // P05 的窄接入面：保持 P04 的资源、场景、相机和 swapchain 生命周期，
    // 只把一帧的命令录制拆成可以由 Render Graph 调度的 pass。
    FrameMatrices prepareFrameRecording();
    void recordShadowPass(VkCommandBuffer cmd, const FrameMatrices& matrices);
    void recordGeometryPass(VkCommandBuffer cmd, std::uint32_t imageIndex,
                            const FrameMatrices& matrices);
    void recordLightingPass(VkCommandBuffer cmd);
    void recordBloomPass(VkCommandBuffer cmd, bool insertManualBarrier);
    void recordTonemapPass(VkCommandBuffer cmd, std::uint32_t imageIndex,
                           bool insertManualBarrier);
    void recordTonemapBegin(VkCommandBuffer cmd, std::uint32_t imageIndex);
    void recordTonemapEnd(VkCommandBuffer cmd);
    VkRenderPass postRenderPass() const { return m_postRenderPass; }
    void setUiInteractionEnabled(bool enabled);
    bool uiInteractionEnabled() const { return m_uiInteractionEnabled; }

    static constexpr std::uint32_t kFramesInFlight = 1;
    static constexpr std::uint32_t kGeometrySubpass = 0;
    static constexpr std::uint32_t kLightingSubpass = 1;

private:
    // ====== t01 —— src/steps/01_gbuffer.cpp ======
    Image createImage(VkFormat format, VkImageUsageFlags usage,
                      VkImageAspectFlags aspect);
    Image createImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                      VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void createGBufferResources();
    void createRenderPass();
    void createFramebuffers();

    // ====== t02 —— src/steps/02_geometry.cpp ======
    void loadModelResources();
    void createGeometryPipeline();

    // ====== t03 —— src/steps/03_lighting.cpp ======
    void createLightingResources();
    void createLightingPipeline();

    // ====== t04-t07 ======================================================
    void createIblResources();
    void createShadowResources();
    void createBloomResources();
    void destroyBloomResources() noexcept;
    void enableSlangShaders();

    // ====== 框架提供 =====================================================
    VkFormat findDepthFormat() const;
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    Buffer createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void recordFrame(VkCommandBuffer cmd, std::uint32_t imageIndex);
    void updateCameraFromInput();
    void updateLightingUniform(const FrameMatrices& matrices);
    void setMouseCaptured(bool captured);
    RawAttachment readback(const Image& image, std::uint32_t bytesPerPixel,
                           VkImageLayout currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void onSwapchainResized();
    void destroySizeDependent() noexcept;
    void destroyImage(Image& image) noexcept;
    void destroyBuffer(Buffer& buffer) noexcept;
    void cleanup() noexcept;

    AppConfig m_config;
    Stage m_stage = Stage::GBuffer;
    bool m_useSlang = false;
    ExternalFrameRecorder m_externalFrameRecorder;
    CameraState m_camera{};
    double m_lastCameraTime = 0.0;
    double m_lastCursorX = 0.0;
    double m_lastCursorY = 0.0;
    bool m_mouseCaptured = false;
    bool m_cursorInitialized = false;
    bool m_escapeWasDown = false;
    bool m_leftMouseWasDown = false;
    bool m_uiInteractionEnabled = false;
    std::unique_ptr<Context> m_ctx;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<FrameRenderer> m_renderer;

    std::vector<Image> m_gbuffer;
    Image m_depth;
    Image m_finalColor;
    std::vector<AttachmentContract> m_contract;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    VkPipelineLayout m_geometryLayout = VK_NULL_HANDLE;
    VkPipeline m_geometryPipeline = VK_NULL_HANDLE;
    Buffer m_modelVertices;
    Buffer m_modelIndices;
    std::uint32_t m_modelVertexCount = 0;
    std::uint32_t m_modelIndexCount = 0;
    std::uint32_t m_groundFirstIndex = 0;
    std::uint32_t m_groundIndexCount = 0;
    bool m_modelLoaded = false;
    std::array<Image, 4> m_materialTextures{};
    VkSampler m_materialSampler = VK_NULL_HANDLE;
    VkDescriptorPool m_materialPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_materialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_materialSet = VK_NULL_HANDLE;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_lightingSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingSet = VK_NULL_HANDLE;
    VkPipelineLayout m_lightingLayout = VK_NULL_HANDLE;
    VkPipeline m_lightingPipeline = VK_NULL_HANDLE;
    Buffer m_lightingUniform;
    BarrierSnapshot m_gbufferBarrier{};

    Buffer m_iblBuffer;
    VkDescriptorPool m_iblPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_iblSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_iblSet = VK_NULL_HANDLE;
    VkPipelineLayout m_iblLayout = VK_NULL_HANDLE;
    VkPipeline m_iblPipeline = VK_NULL_HANDLE;
    BarrierSnapshot m_iblBarrier{};

    Image m_shadowDepth;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_shadowFramebuffer = VK_NULL_HANDLE;
    VkSampler m_shadowSampler = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
    static constexpr std::uint32_t kShadowSize = 256;

    Image m_hdr;
    Image m_bloom;
    VkSampler m_postSampler = VK_NULL_HANDLE;
    VkDescriptorPool m_bloomPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bloomSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_bloomSet = VK_NULL_HANDLE;
    VkPipelineLayout m_bloomLayout = VK_NULL_HANDLE;
    VkPipeline m_bloomPipeline = VK_NULL_HANDLE;
    BarrierSnapshot m_bloomBarrier{};
    VkRenderPass m_postRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_postFramebuffers;
    VkDescriptorPool m_postPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_postSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_postSet = VK_NULL_HANDLE;
    VkPipelineLayout m_postLayout = VK_NULL_HANDLE;
    VkPipeline m_postPipeline = VK_NULL_HANDLE;
};

} // namespace p04
