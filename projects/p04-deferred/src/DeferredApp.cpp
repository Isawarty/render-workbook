#include "DeferredApp.h"

#include "rwb/rhi/VmaUsage.h"
#include "rwb/rhi/Readback.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace p04 {
namespace {

glm::vec3 cameraForward(const CameraState& camera) {
    const float yaw = glm::radians(camera.yawDegrees);
    const float pitch = glm::radians(camera.pitchDegrees);
    return glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch),
                                    std::sin(pitch),
                                    std::sin(yaw) * std::cos(pitch)));
}

glm::vec3 surfaceToLightDirection() {
    return glm::normalize(glm::vec3(-0.45f, 0.8f, 0.6f));
}

float halfToFloat(std::uint16_t value) {
    const std::uint32_t sign = static_cast<std::uint32_t>(value & 0x8000u) << 16;
    std::uint32_t exponent = (value >> 10) & 0x1fu;
    std::uint32_t mantissa = value & 0x03ffu;
    std::uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 113;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            bits = sign | (exponent << 23) | ((mantissa & 0x03ffu) << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7f800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
    }
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

struct StagingBuffer {
    const Context* context = nullptr;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void* mapped = nullptr;

    StagingBuffer(const Context& ctx, VkDeviceSize size) : context(&ctx) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(ctx.allocator(), &bufferInfo, &allocationInfo,
                                 &handle, &allocation, &info));
        mapped = info.pMappedData;
    }

    ~StagingBuffer() {
        if (handle != VK_NULL_HANDLE) {
            vmaDestroyBuffer(context->allocator(), handle, allocation);
        }
    }
};

void colorBarrier(VkCommandBuffer cmd, VkImage image,
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
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

void depthBarrier(VkCommandBuffer cmd, VkImage image,
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
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

DeferredApp::DeferredApp(AppConfig config) : m_config(std::move(config)) {
    rwb::rhi::ContextConfig cc;
    cc.width = m_config.width;
    cc.height = m_config.height;
    cc.title = m_config.title;
    cc.enableValidation = m_config.enableValidation;
    cc.highDpiFramebuffer = m_config.highDpiFramebuffer;
    cc.windowVisible = m_config.windowVisible;
    cc.optionalFeatures.shaderDrawParameters = true;
    cc.optionalFeatures.shaderStorageImageReadWithoutFormat = true;
    // P04 回到窗口 + swapchain 路径。同步验证留给无窗口的 P03 专项；
    // macOS 的 Metal validation 与同步验证叠加时会让窗口初始化极慢。
    cc.enableSyncValidation = m_config.offscreenCapture;

    m_ctx = std::make_unique<Context>(cc);
    m_swapchain = std::make_unique<Swapchain>(*m_ctx);
    m_renderer = std::make_unique<FrameRenderer>(*m_ctx, *m_swapchain, kFramesInFlight);
    m_renderer->setResizeCallback([this] { onSwapchainResized(); });
}

DeferredApp::~DeferredApp() { cleanup(); }

void DeferredApp::init() { initUpTo(Stage::GBuffer); }

void DeferredApp::initUpTo(Stage stage) {
    if (m_renderPass != VK_NULL_HANDLE) {
        throw std::logic_error("DeferredApp::initUpTo 只能调用一次");
    }
    m_stage = stage;
    if (stageIndex(stage) >= stageIndex(Stage::Slang)) enableSlangShaders();
    createGBufferResources();
    if (stageIndex(stage) >= stageIndex(Stage::Ibl)) createIblResources();
    if (stageIndex(stage) >= stageIndex(Stage::Shadows)) createShadowResources();
    if (stageIndex(stage) >= stageIndex(Stage::Bloom)) createBloomResources();
    createRenderPass();
    createFramebuffers();
    if (stageIndex(stage) >= stageIndex(Stage::Geometry)) {
        loadModelResources();
        createGeometryPipeline();
    }
    if (stageIndex(stage) >= stageIndex(Stage::Lighting)) {
        createLightingResources();
        createLightingPipeline();
    }
}

void DeferredApp::applyCameraInput(CameraState& camera, const CameraInput& input,
                                   float deltaSeconds) {
    if (input.reset) {
        camera = CameraState{};
        return;
    }
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.1f);
    constexpr float kTurnSpeed = 75.0f;
    constexpr float kMoveSpeed = 1.6f;
    camera.yawDegrees += input.yaw * kTurnSpeed * dt;
    camera.pitchDegrees = std::clamp(camera.pitchDegrees + input.pitch * kTurnSpeed * dt,
                                     -85.0f, 85.0f);

    const float yaw = glm::radians(camera.yawDegrees);
    const glm::vec3 planarForward = glm::normalize(glm::vec3(std::cos(yaw), 0.0f,
                                                             std::sin(yaw)));
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(planarForward, worldUp));
    const float speed = kMoveSpeed * (input.boost ? 3.0f : 1.0f) * dt;
    camera.position += planarForward * input.forward * speed;
    camera.position += right * input.right * speed;
    camera.position += worldUp * input.up * speed;
}

void DeferredApp::applyMouseLook(CameraState& camera, float deltaX, float deltaY) {
    constexpr float kMouseSensitivity = 0.12f;
    camera.yawDegrees += deltaX * kMouseSensitivity;
    camera.pitchDegrees = std::clamp(camera.pitchDegrees - deltaY * kMouseSensitivity,
                                     -85.0f, 85.0f);
}

FrameMatrices DeferredApp::buildFrameMatrices(const CameraState& camera, float aspect) {
    FrameMatrices matrices;
    matrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(12.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 view = glm::lookAt(camera.position,
                                       camera.position + cameraForward(camera),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(38.0f),
                                            std::max(aspect, 0.001f), 0.1f, 20.0f);
    projection[1][1] *= -1.0f;
    matrices.viewProjection = projection * view;
    matrices.inverseViewProjection = glm::inverse(matrices.viewProjection);

    const glm::vec3 target(0.0f, -0.35f, 0.0f);
    const glm::vec3 lightPosition = target + surfaceToLightDirection() * 12.0f;
    const glm::mat4 lightView = glm::lookAt(lightPosition, target,
                                            glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjection = glm::ortho(-7.0f, 7.0f, -7.0f, 7.0f,
                                           0.1f, 30.0f);
    lightProjection[1][1] *= -1.0f;
    matrices.lightViewProjection = lightProjection * lightView;
    return matrices;
}

glm::vec3 DeferredApp::reconstructWorldPosition(
    glm::vec2 uv, float depth, const glm::mat4& inverseViewProjection) {
    glm::vec4 world = inverseViewProjection *
                      glm::vec4(uv * 2.0f - glm::vec2(1.0f), depth, 1.0f);
    return glm::vec3(world) / world.w;
}

void DeferredApp::setMouseCaptured(bool captured) {
    if (m_config.offscreenCapture || !m_ctx || !m_ctx->window()) return;
    GLFWwindow* window = m_ctx->window()->handle();
    m_mouseCaptured = captured;
    m_cursorInitialized = false;
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED
                                                   : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                         captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

void DeferredApp::setUiInteractionEnabled(bool enabled) {
    if (m_uiInteractionEnabled == enabled) return;
    m_uiInteractionEnabled = enabled;
    setMouseCaptured(!enabled);
}

void DeferredApp::updateCameraFromInput() {
    if (m_config.offscreenCapture || !m_ctx || !m_ctx->window()) return;
    GLFWwindow* window = m_ctx->window()->handle();
    const double now = glfwGetTime();
    const float deltaSeconds = m_lastCameraTime > 0.0
                                   ? static_cast<float>(now - m_lastCameraTime)
                                   : 1.0f / 60.0f;
    m_lastCameraTime = now;
    const auto down = [window](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

    if (m_uiInteractionEnabled) {
        m_escapeWasDown = down(GLFW_KEY_ESCAPE);
        m_leftMouseWasDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        return;
    }

    const bool escapeDown = down(GLFW_KEY_ESCAPE);
    if (escapeDown && !m_escapeWasDown) {
        if (m_mouseCaptured) {
            setMouseCaptured(false);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
    m_escapeWasDown = escapeDown;

    const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (leftMouseDown && !m_leftMouseWasDown && !m_mouseCaptured) {
        setMouseCaptured(true);
    }
    m_leftMouseWasDown = leftMouseDown;

    if (m_mouseCaptured) {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        if (m_cursorInitialized) {
            applyMouseLook(m_camera, static_cast<float>(cursorX - m_lastCursorX),
                           static_cast<float>(cursorY - m_lastCursorY));
        }
        m_lastCursorX = cursorX;
        m_lastCursorY = cursorY;
        m_cursorInitialized = true;
    }

    CameraInput input;
    input.forward = static_cast<float>(down(GLFW_KEY_W)) -
                    static_cast<float>(down(GLFW_KEY_S));
    input.right = static_cast<float>(down(GLFW_KEY_D)) -
                  static_cast<float>(down(GLFW_KEY_A));
    input.up = static_cast<float>(down(GLFW_KEY_E)) -
               static_cast<float>(down(GLFW_KEY_Q));
    input.yaw = static_cast<float>(down(GLFW_KEY_RIGHT)) -
                static_cast<float>(down(GLFW_KEY_LEFT));
    input.pitch = static_cast<float>(down(GLFW_KEY_UP)) -
                  static_cast<float>(down(GLFW_KEY_DOWN));
    input.boost = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
    input.reset = down(GLFW_KEY_R);
    applyCameraInput(m_camera, input, deltaSeconds);
}

void DeferredApp::run(int frameCount) {
    m_lastCameraTime = glfwGetTime();
    setMouseCaptured(!m_uiInteractionEnabled);
    m_renderer->run(frameCount,
        [this](VkCommandBuffer cmd, std::uint32_t imageIndex, std::uint32_t) {
            updateCameraFromInput();
            recordFrame(cmd, imageIndex);
        });
}

GBufferCapture DeferredApp::renderAndCaptureGBuffer() {
    if (m_renderPass == VK_NULL_HANDLE) {
        throw std::logic_error("请先调用 DeferredApp::init");
    }
    if (m_config.offscreenCapture) {
        m_ctx->immediateSubmit([this](VkCommandBuffer cmd) { recordFrame(cmd, 0); });
    } else {
        m_renderer->run(1,
            [this](VkCommandBuffer cmd, std::uint32_t imageIndex, std::uint32_t) {
                recordFrame(cmd, imageIndex);
            });
    }

    return {
        readback(m_gbuffer[0], m_contract[0].bytesPerPixel),
        readback(m_gbuffer[1], m_contract[1].bytesPerPixel),
        readback(m_gbuffer[2], m_contract[2].bytesPerPixel),
    };
}

rwb::rhi::CapturedImage DeferredApp::renderAndCaptureFinal() {
    if (m_renderPass == VK_NULL_HANDLE) {
        throw std::logic_error("请先调用 DeferredApp::initUpTo");
    }
    if (m_config.offscreenCapture) {
        m_ctx->immediateSubmit([this](VkCommandBuffer cmd) { recordFrame(cmd, 0); });
        const std::uint32_t bytesPerPixel =
            m_finalColor.format == VK_FORMAT_R8G8B8A8_UNORM ? 4u : 8u;
        const RawAttachment raw = readback(m_finalColor, bytesPerPixel,
                                           VK_IMAGE_LAYOUT_GENERAL);
        rwb::rhi::CapturedImage out;
        out.width = raw.width;
        out.height = raw.height;
        if (bytesPerPixel == 4) {
            out.pixels = raw.pixels;
            return out;
        }
        out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4);
        for (std::size_t pixel = 0; pixel < out.pixels.size() / 4; ++pixel) {
            for (std::size_t channel = 0; channel < 4; ++channel) {
                std::uint16_t half = 0;
                std::memcpy(&half, raw.pixels.data() + pixel * 8 + channel * 2,
                            sizeof(half));
                const float value = std::clamp(halfToFloat(half), 0.0f, 1.0f);
                out.pixels[pixel * 4 + channel] = static_cast<std::uint8_t>(
                    std::lround(value * 255.0f));
            }
        }
        return out;
    }
    return m_renderer->renderAndCapture(
        [this](VkCommandBuffer cmd, std::uint32_t imageIndex, std::uint32_t) {
            recordFrame(cmd, imageIndex);
        });
}

VkFormat DeferredApp::findDepthFormat() const {
    return m_ctx->findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

Buffer DeferredApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer buffer;
    buffer.size = size;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    VK_CHECK(vmaCreateBuffer(m_ctx->allocator(), &info, &allocationInfo,
                             &buffer.handle, &buffer.allocation, nullptr));
    return buffer;
}

Buffer DeferredApp::createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer buffer;
    buffer.size = size;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo allocationResult{};
    VK_CHECK(vmaCreateBuffer(m_ctx->allocator(), &info, &allocationInfo,
                             &buffer.handle, &buffer.allocation, &allocationResult));
    buffer.mapped = allocationResult.pMappedData;
    return buffer;
}

void DeferredApp::updateLightingUniform(const FrameMatrices& matrices) {
    if (!m_lightingUniform.valid()) return;
    LightingUniform uniform;
    uniform.inverseViewProjection = matrices.inverseViewProjection;
    uniform.lightViewProjection = matrices.lightViewProjection;
    uniform.cameraPosition = glm::vec4(m_camera.position, 1.0f);
    uniform.lightDirection = glm::vec4(surfaceToLightDirection(), 0.0f);
    std::memcpy(m_lightingUniform.mapped, &uniform, sizeof(uniform));
    VK_CHECK(vmaFlushAllocation(m_ctx->allocator(), m_lightingUniform.allocation,
                                0, sizeof(uniform)));
}

std::vector<float> DeferredApp::readbackIbl() const {
    if (!m_iblBuffer.valid()) return {};
    return rwb::rhi::readbackBufferAs<float>(*m_ctx, m_iblBuffer.handle,
                                              m_iblBuffer.size / sizeof(float));
}

void DeferredApp::recordFrame(VkCommandBuffer cmd, std::uint32_t imageIndex) {
    if (m_externalFrameRecorder) {
        m_externalFrameRecorder(cmd, imageIndex);
        return;
    }
    const FrameMatrices matrices = prepareFrameRecording();
    recordShadowPass(cmd, matrices);
    recordGeometryPass(cmd, imageIndex, matrices);
    recordLightingPass(cmd);
    recordBloomPass(cmd, true);
    recordTonemapPass(cmd, imageIndex, true);
}

FrameMatrices DeferredApp::prepareFrameRecording() {
    const VkExtent2D extent = m_swapchain->extent();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const FrameMatrices matrices = buildFrameMatrices(m_camera, aspect);
    if (stageIndex(m_stage) >= stageIndex(Stage::Lighting)) {
        updateLightingUniform(matrices);
    }
    return matrices;
}

void DeferredApp::recordShadowPass(VkCommandBuffer cmd, const FrameMatrices& matrices) {
    if (stageIndex(m_stage) >= stageIndex(Stage::Shadows)) {
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo shadowBegin{};
        shadowBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        shadowBegin.renderPass = m_shadowRenderPass;
        shadowBegin.framebuffer = m_shadowFramebuffer;
        shadowBegin.renderArea.extent = {kShadowSize, kShadowSize};
        shadowBegin.clearValueCount = 1;
        shadowBegin.pClearValues = &shadowClear;
        vkCmdBeginRenderPass(cmd, &shadowBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_modelVertices.handle, &offset);
        vkCmdBindIndexBuffer(cmd, m_modelIndices.handle, 0, VK_INDEX_TYPE_UINT32);
        const glm::mat4 helmetLightMvp = matrices.lightViewProjection * matrices.model;
        vkCmdPushConstants(cmd, m_shadowLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(helmetLightMvp), &helmetLightMvp);
        vkCmdDrawIndexed(cmd, m_modelIndexCount, 1, 0, 0, 0);
        vkCmdPushConstants(cmd, m_shadowLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(matrices.lightViewProjection),
                           &matrices.lightViewProjection);
        vkCmdDrawIndexed(cmd, m_groundIndexCount, 1, m_groundFirstIndex, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
}

void DeferredApp::recordGeometryPass(VkCommandBuffer cmd, std::uint32_t imageIndex,
                                     const FrameMatrices& matrices) {
    const glm::mat4 mvp = matrices.viewProjection * matrices.model;
    const glm::mat4 groundMvp = matrices.viewProjection;
    const std::array<VkClearValue, 5> clears{{
        {{{0.125f, 0.25f, 0.5f, 0.75f}}},
        {{{0.0f, 0.0f, 1.0f, 0.5f}}},
        {{{0.05f, 0.1f, 0.2f, 1.0f}}},
        {{{1.0f, 0}}},
        {{{0.015f, 0.02f, 0.03f, 1.0f}}},
    }};

    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = m_renderPass;
    begin.framebuffer = m_framebuffers.at(imageIndex);
    begin.renderArea.extent = m_swapchain->extent();
    begin.clearValueCount = static_cast<std::uint32_t>(clears.size());
    begin.pClearValues = clears.data();

    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    if (stageIndex(m_stage) >= stageIndex(Stage::Geometry)) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryPipeline);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_modelVertices.handle, &offset);
        vkCmdBindIndexBuffer(cmd, m_modelIndices.handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryLayout,
                                0, 1, &m_materialSet, 0, nullptr);
        const std::array<glm::mat4, 2> push{{mvp, matrices.model}};
        vkCmdPushConstants(cmd, m_geometryLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(push), push.data());
        vkCmdDrawIndexed(cmd, m_modelIndexCount, 1, 0, 0, 0);
        const std::array<glm::mat4, 2> groundPush{{groundMvp, glm::mat4(1.0f)}};
        vkCmdPushConstants(cmd, m_geometryLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(groundPush), groundPush.data());
        vkCmdDrawIndexed(cmd, m_groundIndexCount, 1, m_groundFirstIndex, 0, 0);
    }
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
}

void DeferredApp::recordLightingPass(VkCommandBuffer cmd) {
    if (stageIndex(m_stage) >= stageIndex(Stage::Lighting)) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingLayout,
                                0, 1, &m_lightingSet, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
}

void DeferredApp::recordBloomPass(VkCommandBuffer cmd, bool insertManualBarrier) {
    if (stageIndex(m_stage) >= stageIndex(Stage::Bloom)) {
        const VkExtent2D extent = m_swapchain->extent();
        // MoltenVK 对 render-pass external dependency 到 storage-image read 的
        // 可见性偶发不稳定；显式 image barrier 也把教学数据流直接写在消费者前。
        if (insertManualBarrier) {
            colorBarrier(cmd, m_hdr.handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomLayout,
                                0, 1, &m_bloomSet, 0, nullptr);
        const std::array<std::uint32_t, 2> bloomExtent{{extent.width, extent.height}};
        vkCmdPushConstants(cmd, m_bloomLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(bloomExtent), bloomExtent.data());
        vkCmdDispatch(cmd, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);

        m_bloomBarrier = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT};
        if (insertManualBarrier) {
            colorBarrier(cmd, m_bloom.handle, VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_GENERAL, m_bloomBarrier.srcAccess,
                         m_bloomBarrier.dstAccess, m_bloomBarrier.srcStage,
                         m_bloomBarrier.dstStage);
        }
    }
}

void DeferredApp::recordTonemapPass(VkCommandBuffer cmd, std::uint32_t imageIndex,
                                    bool insertManualBarrier) {
    if (stageIndex(m_stage) >= stageIndex(Stage::Bloom)) {
        // HDR 有 Bloom(compute) 与 Tonemap(fragment) 两个消费者。不能假设前一条
        // color->compute|fragment barrier 在经过 compute pass 后仍替第二个消费者
        // 建立了清晰的 writer scope；MoltenVK 上这会偶发读到洋红污染。
        if (insertManualBarrier) {
            colorBarrier(cmd, m_hdr.handle, VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        recordTonemapBegin(cmd, imageIndex);
        recordTonemapEnd(cmd);
    }
}

void DeferredApp::recordTonemapBegin(VkCommandBuffer cmd, std::uint32_t imageIndex) {
    const VkExtent2D extent = m_swapchain->extent();
    VkClearValue clear{};
    clear.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo postBegin{};
    postBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    postBegin.renderPass = m_postRenderPass;
    postBegin.framebuffer = m_postFramebuffers.at(imageIndex);
    postBegin.renderArea.extent = extent;
    postBegin.clearValueCount = 1;
    postBegin.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &postBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postLayout,
                            0, 1, &m_postSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void DeferredApp::recordTonemapEnd(VkCommandBuffer cmd) { vkCmdEndRenderPass(cmd); }

RawAttachment DeferredApp::readbackBloom() {
    if (!m_bloom.valid()) return {};
    return readback(m_bloom, 8, VK_IMAGE_LAYOUT_GENERAL);
}

RawAttachment DeferredApp::readbackHdr() {
    if (!m_hdr.valid()) return {};
    return readback(m_hdr, 8, VK_IMAGE_LAYOUT_GENERAL);
}

RawAttachment DeferredApp::readbackShadowDepth() {
    if (!m_shadowDepth.valid()) return {};
    m_ctx->waitIdle();
    const VkDeviceSize size = static_cast<VkDeviceSize>(m_shadowDepth.width) *
                              m_shadowDepth.height * sizeof(std::uint32_t);
    StagingBuffer staging(*m_ctx, size);
    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        depthBarrier(cmd, m_shadowDepth.handle,
                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {m_shadowDepth.width, m_shadowDepth.height, 1};
        vkCmdCopyImageToBuffer(cmd, m_shadowDepth.handle,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging.handle, 1, &region);
        VkMemoryBarrier hostRead{};
        hostRead.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        hostRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        hostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0,
                             1, &hostRead, 0, nullptr, 0, nullptr);
        depthBarrier(cmd, m_shadowDepth.handle,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });
    VK_CHECK(vmaInvalidateAllocation(m_ctx->allocator(), staging.allocation, 0, size));
    RawAttachment out;
    out.format = m_shadowDepth.format;
    out.width = m_shadowDepth.width;
    out.height = m_shadowDepth.height;
    out.bytesPerPixel = sizeof(std::uint32_t);
    out.pixels.resize(static_cast<std::size_t>(size));
    std::memcpy(out.pixels.data(), staging.mapped, out.pixels.size());
    return out;
}

RawAttachment DeferredApp::readback(const Image& image, std::uint32_t bytesPerPixel,
                                    VkImageLayout currentLayout) {
    // 教学判分优先确定性。MoltenVK 偶发在 submit fence 已 signal 后仍延迟
    // storage-image 的可见性；device idle 后再做 transfer readback 可稳定复现。
    m_ctx->waitIdle();
    const VkDeviceSize size = static_cast<VkDeviceSize>(image.width) * image.height *
                              bytesPerPixel;
    StagingBuffer staging(*m_ctx, size);

    m_ctx->immediateSubmit([&](VkCommandBuffer cmd) {
        colorBarrier(cmd, image.handle,
                     currentLayout,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {image.width, image.height, 1};
        vkCmdCopyImageToBuffer(cmd, image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging.handle, 1, &region);

        VkMemoryBarrier hostRead{};
        hostRead.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        hostRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        hostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0,
                             1, &hostRead, 0, nullptr, 0, nullptr);

        colorBarrier(cmd, image.handle,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     currentLayout,
                     VK_ACCESS_TRANSFER_READ_BIT,
                     currentLayout == VK_IMAGE_LAYOUT_GENERAL
                         ? (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
                         : VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     currentLayout == VK_IMAGE_LAYOUT_GENERAL
                         ? (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                         : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });

    VK_CHECK(vmaInvalidateAllocation(m_ctx->allocator(), staging.allocation, 0, size));
    RawAttachment out;
    out.format = image.format;
    out.width = image.width;
    out.height = image.height;
    out.bytesPerPixel = bytesPerPixel;
    out.pixels.resize(static_cast<std::size_t>(size));
    std::memcpy(out.pixels.data(), staging.mapped, out.pixels.size());
    return out;
}

void DeferredApp::onSwapchainResized() {
    m_ctx->waitIdle();
    destroySizeDependent();
    createGBufferResources();
    if (stageIndex(m_stage) >= stageIndex(Stage::Bloom)) createBloomResources();
    createRenderPass();
    createFramebuffers();
    if (stageIndex(m_stage) >= stageIndex(Stage::Geometry)) createGeometryPipeline();
    if (stageIndex(m_stage) >= stageIndex(Stage::Lighting)) {
        createLightingResources();
        createLightingPipeline();
    }
}

void DeferredApp::destroyImage(Image& image) noexcept {
    if (!m_ctx) {
        image = {};
        return;
    }
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_ctx->device(), image.view, nullptr);
    }
    if (image.handle != VK_NULL_HANDLE) {
        vmaDestroyImage(m_ctx->allocator(), image.handle, image.allocation);
    }
    image = {};
}

void DeferredApp::destroyBuffer(Buffer& buffer) noexcept {
    if (m_ctx && buffer.handle != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_ctx->allocator(), buffer.handle, buffer.allocation);
    }
    buffer = {};
}

void DeferredApp::destroySizeDependent() noexcept {
    if (!m_ctx) return;
    destroyBloomResources();
    if (m_lightingPipeline) vkDestroyPipeline(m_ctx->device(), m_lightingPipeline, nullptr);
    if (m_lightingLayout) vkDestroyPipelineLayout(m_ctx->device(), m_lightingLayout, nullptr);
    if (m_lightingSetLayout) {
        vkDestroyDescriptorSetLayout(m_ctx->device(), m_lightingSetLayout, nullptr);
    }
    if (m_descriptorPool) vkDestroyDescriptorPool(m_ctx->device(), m_descriptorPool, nullptr);
    if (m_geometryPipeline) vkDestroyPipeline(m_ctx->device(), m_geometryPipeline, nullptr);
    if (m_geometryLayout) vkDestroyPipelineLayout(m_ctx->device(), m_geometryLayout, nullptr);
    m_lightingPipeline = VK_NULL_HANDLE;
    m_lightingLayout = VK_NULL_HANDLE;
    m_lightingSetLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_lightingSet = VK_NULL_HANDLE;
    m_geometryPipeline = VK_NULL_HANDLE;
    m_geometryLayout = VK_NULL_HANDLE;
    for (VkFramebuffer framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_ctx->device(), framebuffer, nullptr);
    }
    m_framebuffers.clear();
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_ctx->device(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    destroyImage(m_depth);
    destroyImage(m_finalColor);
    for (Image& image : m_gbuffer) destroyImage(image);
    m_gbuffer.clear();
    m_contract.clear();
}

void DeferredApp::cleanup() noexcept {
    if (!m_ctx) return;
    m_ctx->waitIdle();
    m_renderer.reset();
    destroySizeDependent();
    if (m_iblPipeline) vkDestroyPipeline(m_ctx->device(), m_iblPipeline, nullptr);
    if (m_iblLayout) vkDestroyPipelineLayout(m_ctx->device(), m_iblLayout, nullptr);
    if (m_iblSetLayout) vkDestroyDescriptorSetLayout(m_ctx->device(), m_iblSetLayout, nullptr);
    if (m_iblPool) vkDestroyDescriptorPool(m_ctx->device(), m_iblPool, nullptr);
    destroyBuffer(m_iblBuffer);
    destroyBuffer(m_lightingUniform);
    if (m_materialPool) vkDestroyDescriptorPool(m_ctx->device(), m_materialPool, nullptr);
    if (m_materialSetLayout) {
        vkDestroyDescriptorSetLayout(m_ctx->device(), m_materialSetLayout, nullptr);
    }
    if (m_materialSampler) vkDestroySampler(m_ctx->device(), m_materialSampler, nullptr);
    for (Image& image : m_materialTextures) destroyImage(image);
    destroyBuffer(m_modelVertices);
    destroyBuffer(m_modelIndices);
    if (m_shadowPipeline) vkDestroyPipeline(m_ctx->device(), m_shadowPipeline, nullptr);
    if (m_shadowLayout) vkDestroyPipelineLayout(m_ctx->device(), m_shadowLayout, nullptr);
    if (m_shadowSampler) vkDestroySampler(m_ctx->device(), m_shadowSampler, nullptr);
    if (m_shadowFramebuffer) vkDestroyFramebuffer(m_ctx->device(), m_shadowFramebuffer, nullptr);
    if (m_shadowRenderPass) vkDestroyRenderPass(m_ctx->device(), m_shadowRenderPass, nullptr);
    destroyImage(m_shadowDepth);
    m_swapchain.reset();
    m_ctx.reset();
}

} // namespace p04
