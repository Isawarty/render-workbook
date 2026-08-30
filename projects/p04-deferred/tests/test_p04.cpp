#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "DeferredApp.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

using namespace p04;

namespace {

struct ValidationGuard {
    ValidationGuard() { rwb::ValidationLog::instance().reset(); }
    void requireClean() const {
        const auto& log = rwb::ValidationLog::instance();
        INFO(log.summary());
        REQUIRE(log.errorCount() == 0);
        REQUIRE(log.warningCount() == 0);
    }
};

std::unique_ptr<DeferredApp> makeApp(Stage stage = Stage::GBuffer) {
    AppConfig config;
    config.width = 160;
    config.height = 120;
    config.highDpiFramebuffer = false;
    config.windowVisible = false;
    config.offscreenCapture = true;
    config.title = "render-workbook p04 test";
    auto app = std::make_unique<DeferredApp>(config);
    app->initUpTo(stage);
    return app;
}

std::size_t pixelOffset(const RawAttachment& image, std::uint32_t x, std::uint32_t y) {
    return (static_cast<std::size_t>(y) * image.width + x) * image.bytesPerPixel;
}

std::size_t pixelOffset(const rwb::rhi::CapturedImage& image,
                        std::uint32_t x, std::uint32_t y) {
    return (static_cast<std::size_t>(y) * image.width + x) * 4;
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
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x03ffu;
            bits = sign | (exponent << 23) | (mantissa << 13);
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

void requireUnormPixel(const RawAttachment& image, std::size_t offset,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    REQUIRE(image.pixels.size() >= offset + 4);
    REQUIRE(std::abs(static_cast<int>(image.pixels[offset + 0]) - r) <= 1);
    REQUIRE(std::abs(static_cast<int>(image.pixels[offset + 1]) - g) <= 1);
    REQUIRE(std::abs(static_cast<int>(image.pixels[offset + 2]) - b) <= 1);
    REQUIRE(std::abs(static_cast<int>(image.pixels[offset + 3]) - a) <= 1);
}

bool isClearAlbedo(const RawAttachment& image, std::size_t offset) {
    return image.pixels[offset + 0] == 32 && image.pixels[offset + 1] == 64 &&
           image.pixels[offset + 2] == 128 && image.pixels[offset + 3] == 191;
}

} // namespace

TEST_CASE("t01 G-buffer 附件契约", "[t01]") {
    ValidationGuard guard;
    auto app = makeApp();

    REQUIRE(app->renderPass() != VK_NULL_HANDLE);
    REQUIRE(app->colorAttachments().size() == 3);
    REQUIRE(app->attachmentContract().size() == 3);
    REQUIRE(app->framebuffers().size() == app->swapchain().imageCount());

    const VkExtent2D extent = app->swapchain().extent();
    for (std::size_t i = 0; i < 3; ++i) {
        const Image& image = app->colorAttachments()[i];
        const AttachmentContract& contract = app->attachmentContract()[i];
        REQUIRE(image.valid());
        REQUIRE(image.view != VK_NULL_HANDLE);
        REQUIRE(image.width == extent.width);
        REQUIRE(image.height == extent.height);
        REQUIRE(image.format == contract.format);
        REQUIRE((contract.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0);
        REQUIRE((contract.usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) != 0);
        REQUIRE((contract.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0);
        REQUIRE(contract.finalLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    REQUIRE(app->attachmentContract()[0].format == VK_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(app->attachmentContract()[1].format == VK_FORMAT_R16G16B16A16_SFLOAT);
    REQUIRE(app->attachmentContract()[2].format == VK_FORMAT_R8G8B8A8_UNORM);

    const Image& depth = app->depthAttachment();
    REQUIRE(depth.valid());
    REQUIRE(depth.view != VK_NULL_HANDLE);
    REQUIRE(depth.width == extent.width);
    REQUIRE(depth.height == extent.height);
    REQUIRE((depth.format == VK_FORMAT_D32_SFLOAT ||
             depth.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
             depth.format == VK_FORMAT_D24_UNORM_S8_UINT));

    app.reset();
    guard.requireClean();
}

TEST_CASE("t01 clear-only pass 写入三张 G-buffer", "[t01]") {
    ValidationGuard guard;
    auto app = makeApp();
    const GBufferCapture capture = app->renderAndCaptureGBuffer();
    REQUIRE(capture.albedoMetallic.bytesPerPixel == 4);
    REQUIRE(capture.normalRoughness.bytesPerPixel == 8);
    REQUIRE(capture.emissiveAo.bytesPerPixel == 4);
    requireUnormPixel(capture.albedoMetallic, 0, 32, 64, 128, 191);
    requireUnormPixel(capture.emissiveAo, 0, 13, 26, 51, 255);

    REQUIRE(capture.normalRoughness.pixels.size() >= 8);
    std::uint16_t channels[4]{};
    std::memcpy(channels, capture.normalRoughness.pixels.data(), sizeof(channels));
    REQUIRE(halfToFloat(channels[0]) == 0.0f);
    REQUIRE(halfToFloat(channels[1]) == 0.0f);
    REQUIRE(halfToFloat(channels[2]) == 1.0f);
    REQUIRE(halfToFloat(channels[3]) == 0.5f);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t02 geometry pass 真实写入 MRT", "[t02]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Geometry);
    REQUIRE(app->geometryPipeline() != VK_NULL_HANDLE);
    REQUIRE(app->modelLoaded());
    REQUIRE(app->modelVertexCount() == 70074);
    REQUIRE(app->modelIndexCount() == 70074);
    REQUIRE(app->groundIndexCount() == 6);
    REQUIRE(app->materialTextureCount() == 4);
    const GBufferCapture capture = app->renderAndCaptureGBuffer();

    const std::size_t corner = pixelOffset(capture.albedoMetallic, 0, 0);
    const std::size_t center = pixelOffset(capture.albedoMetallic,
                                           capture.albedoMetallic.width / 2,
                                           capture.albedoMetallic.height / 2);
    INFO("模型外必须保留 t01 的 clear 值，证明真实网格没有覆盖整屏");
    requireUnormPixel(capture.albedoMetallic, corner, 32, 64, 128, 191);
    INFO("中心像素必须来自 Sci-Fi Helmet 的 base-color / metallic 贴图");
    const bool centerIsClear = capture.albedoMetallic.pixels[center + 0] == 32 &&
                               capture.albedoMetallic.pixels[center + 1] == 64 &&
                               capture.albedoMetallic.pixels[center + 2] == 128 &&
                               capture.albedoMetallic.pixels[center + 3] == 191;
    REQUIRE_FALSE(centerIsClear);
    REQUIRE(capture.albedoMetallic.pixels[center + 0] >= 35);
    REQUIRE(capture.albedoMetallic.pixels[center + 0] <= 65);
    REQUIRE(capture.albedoMetallic.pixels[center + 1] >= 35);
    REQUIRE(capture.albedoMetallic.pixels[center + 1] <= 65);
    REQUIRE(capture.albedoMetallic.pixels[center + 2] >= 35);
    REQUIRE(capture.albedoMetallic.pixels[center + 2] <= 65);
    REQUIRE(capture.albedoMetallic.pixels[center + 3] >= 35);
    REQUIRE(capture.albedoMetallic.pixels[center + 3] <= 70);

    const std::size_t normalCenter = pixelOffset(capture.normalRoughness,
                                                 capture.normalRoughness.width / 2,
                                                 capture.normalRoughness.height / 2);
    std::uint16_t channels[4]{};
    std::memcpy(channels, capture.normalRoughness.pixels.data() + normalCenter,
                sizeof(channels));
    const float nx = halfToFloat(channels[0]);
    const float ny = halfToFloat(channels[1]);
    const float nz = halfToFloat(channels[2]);
    const float normalLength = std::sqrt(nx * nx + ny * ny + nz * nz);
    INFO("中心法线必须来自切线空间法线贴图并保持归一化");
    REQUIRE(nx < -0.53f);
    REQUIRE(nz > 0.82f);
    REQUIRE(nz < 0.87f);
    REQUIRE(normalLength > 0.9f);
    REQUIRE(normalLength < 1.1f);
    INFO("roughness 必须来自 metallic-roughness 贴图的 G 通道");
    REQUIRE(halfToFloat(channels[3]) >= 0.04f);
    REQUIRE(halfToFloat(channels[3]) <= 1.0f);

    const std::size_t emissiveCenter = pixelOffset(capture.emissiveAo,
                                                   capture.emissiveAo.width / 2,
                                                   capture.emissiveAo.height / 2);
    INFO("Sci-Fi Helmet 没有 emissive 材质，不能用 UV 扫描线伪造全表面发光");
    REQUIRE(capture.emissiveAo.pixels[emissiveCenter + 0] == 0);
    REQUIRE(capture.emissiveAo.pixels[emissiveCenter + 1] == 0);
    REQUIRE(capture.emissiveAo.pixels[emissiveCenter + 2] == 0);
    REQUIRE(capture.emissiveAo.pixels[emissiveCenter + 3] > 0);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t02 P04 漫游相机输入是帧率无关的纯逻辑", "[t02]") {
    CameraState camera;
    CameraInput forward;
    forward.forward = 1.0f;
    DeferredApp::applyCameraInput(camera, forward, 0.1f);
    REQUIRE(camera.position.x == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(camera.position.z == Catch::Approx(4.04f).margin(0.001f));

    CameraInput strafeAndTurn;
    strafeAndTurn.right = 1.0f;
    strafeAndTurn.yaw = 1.0f;
    DeferredApp::applyCameraInput(camera, strafeAndTurn, 0.1f);
    REQUIRE(camera.position.x > 0.15f);
    REQUIRE(camera.yawDegrees == Catch::Approx(-82.5f));

    CameraInput boostedRise;
    boostedRise.up = 1.0f;
    boostedRise.boost = true;
    DeferredApp::applyCameraInput(camera, boostedRise, 0.1f);
    REQUIRE(camera.position.y == Catch::Approx(0.53f));

    CameraInput reset;
    reset.reset = true;
    DeferredApp::applyCameraInput(camera, reset, 0.1f);
    REQUIRE(camera.position.x == Catch::Approx(0.0f));
    REQUIRE(camera.position.y == Catch::Approx(0.05f));
    REQUIRE(camera.position.z == Catch::Approx(4.2f));
    REQUIRE(camera.yawDegrees == Catch::Approx(-90.0f));

    DeferredApp::applyMouseLook(camera, 25.0f, -10.0f);
    REQUIRE(camera.yawDegrees == Catch::Approx(-87.0f));
    REQUIRE(camera.pitchDegrees == Catch::Approx(0.51794f).margin(0.0001f));
    DeferredApp::applyMouseLook(camera, 0.0f, -10000.0f);
    REQUIRE(camera.pitchDegrees == Catch::Approx(85.0f));
}

TEST_CASE("P04 固定光源矩阵与深度重建契约", "[logic]") {
    const CameraState defaultCamera;
    CameraState movedCamera = defaultCamera;
    movedCamera.position = {1.2f, 0.8f, 3.1f};
    movedCamera.yawDegrees = -112.0f;
    movedCamera.pitchDegrees = -9.0f;
    const FrameMatrices first = DeferredApp::buildFrameMatrices(defaultCamera, 4.0f / 3.0f);
    const FrameMatrices moved = DeferredApp::buildFrameMatrices(movedCamera, 4.0f / 3.0f);

    bool cameraMatrixChanged = false;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            REQUIRE(first.lightViewProjection[column][row] ==
                    Catch::Approx(moved.lightViewProjection[column][row]).margin(0.000001f));
            cameraMatrixChanged = cameraMatrixChanged ||
                std::abs(first.viewProjection[column][row] -
                         moved.viewProjection[column][row]) > 0.0001f;
        }
    }
    INFO("观察相机变化时 camera VP 必须变化，但固定 light VP 不得变化");
    REQUIRE(cameraMatrixChanged);

    const glm::vec3 expectedWorld(0.35f, -0.42f, 0.18f);
    const glm::vec4 clip = first.viewProjection * glm::vec4(expectedWorld, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const glm::vec2 uv = glm::vec2(ndc) * 0.5f + glm::vec2(0.5f);
    const glm::vec3 reconstructed = DeferredApp::reconstructWorldPosition(
        uv, ndc.z, first.inverseViewProjection);
    REQUIRE(reconstructed.x == Catch::Approx(expectedWorld.x).margin(0.0001f));
    REQUIRE(reconstructed.y == Catch::Approx(expectedWorld.y).margin(0.0001f));
    REQUIRE(reconstructed.z == Catch::Approx(expectedWorld.z).margin(0.0001f));

    REQUIRE(sizeof(LightingUniform) == sizeof(float) * 40);
    REQUIRE(offsetof(LightingUniform, inverseViewProjection) == 0);
    REQUIRE(offsetof(LightingUniform, lightViewProjection) == sizeof(float) * 16);
    REQUIRE(offsetof(LightingUniform, cameraPosition) == sizeof(float) * 32);
    REQUIRE(offsetof(LightingUniform, lightDirection) == sizeof(float) * 36);
}

TEST_CASE("t03 Cook-Torrance lighting 消费 G-buffer", "[t03]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Lighting);
    REQUIRE(app->geometryPipeline() != VK_NULL_HANDLE);
    REQUIRE(app->lightingPipeline() != VK_NULL_HANDLE);
    REQUIRE(app->lightingSetLayout() != VK_NULL_HANDLE);
    REQUIRE(app->lightingUniformBuffer().valid());
    REQUIRE(app->lightingUniformBuffer().mapped != nullptr);
    const BarrierSnapshot& barrier = app->gbufferToLightingBarrier();
    REQUIRE((barrier.srcStage & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) != 0);
    REQUIRE((barrier.srcAccess & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0);
    REQUIRE((barrier.dstAccess & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0);
    const auto frame = app->renderAndCaptureFinal();
    REQUIRE(frame.width == 160);
    REQUIRE(frame.height == 120);
    const std::size_t center = pixelOffset(frame, frame.width / 2, frame.height / 2);
    const std::size_t corner = pixelOffset(frame, 0, 0);
    const int centerLuma = frame.pixels[center] + frame.pixels[center + 1] +
                           frame.pixels[center + 2];
    const int cornerLuma = frame.pixels[corner] + frame.pixels[corner + 1] +
                           frame.pixels[corner + 2];
    INFO("光照后的头盔中心必须明显亮于清屏背景");
    REQUIRE(centerLuma > cornerLuma + 60);
    REQUIRE(frame.pixels[center + 3] == 255);
    const std::size_t upperSky = pixelOffset(frame, 0, frame.height / 3);
    const int skyDifference =
        std::abs(static_cast<int>(frame.pixels[corner + 0]) -
                 static_cast<int>(frame.pixels[upperSky + 0])) +
        std::abs(static_cast<int>(frame.pixels[corner + 1]) -
                 static_cast<int>(frame.pixels[upperSky + 1])) +
        std::abs(static_cast<int>(frame.pixels[corner + 2]) -
                 static_cast<int>(frame.pixels[upperSky + 2]));
    INFO("背景必须是有顶部/地平线层次的程序化天空，而不是统一 clear color");
    REQUIRE(skyDifference > 20);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t04 compute 生成并消费 IBL 数据", "[t04]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Ibl);
    REQUIRE(app->iblBuffer().valid());
    REQUIRE(app->iblPipeline() != VK_NULL_HANDLE);
    const BarrierSnapshot& barrier = app->iblBarrier();
    REQUIRE(barrier.srcStage == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    REQUIRE((barrier.dstStage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) != 0);
    REQUIRE((barrier.srcAccess & VK_ACCESS_SHADER_WRITE_BIT) != 0);
    REQUIRE((barrier.dstAccess & VK_ACCESS_SHADER_READ_BIT) != 0);

    const std::vector<float> ibl = app->readbackIbl();
    REQUIRE(ibl.size() == 22 * 4);
    REQUIRE(ibl[0] == Catch::Approx(0.45f));
    REQUIRE(ibl[1] == Catch::Approx(0.45f));
    REQUIRE(ibl[2] == Catch::Approx(0.45f));
    REQUIRE(ibl[3] == Catch::Approx(1.0f));
    INFO("最后一级 prefilter 必须比 irradiance 暗，并记录 roughness=1");
    REQUIRE(ibl[20] == Catch::Approx(0.45f * 0.68f));
    REQUIRE(ibl[23] == Catch::Approx(1.0f));
    INFO("BRDF LUT 首格包含 scale/bias 与采样坐标，不能是未初始化的零");
    REQUIRE(ibl[24] > 0.1f);
    REQUIRE(ibl[25] > 0.04f);
    REQUIRE(ibl[26] == Catch::Approx(0.125f));
    REQUIRE(ibl[27] == Catch::Approx(0.125f));

    const auto frame = app->renderAndCaptureFinal();
    const std::size_t center = pixelOffset(frame, frame.width / 2, frame.height / 2);
    REQUIRE(frame.pixels[center] > 20);
    REQUIRE(frame.pixels[center + 3] == 255);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t05 shadow depth pass 与 3x3 PCF", "[t05]") {
    ValidationGuard guard;
    auto shadowApp = makeApp(Stage::Shadows);
    REQUIRE(shadowApp->shadowDepth().valid());
    REQUIRE(shadowApp->shadowDepth().view != VK_NULL_HANDLE);
    REQUIRE(shadowApp->shadowDepth().width == 256);
    REQUIRE(shadowApp->shadowDepth().height == 256);
    REQUIRE(shadowApp->shadowRenderPass() != VK_NULL_HANDLE);
    REQUIRE(shadowApp->shadowPipeline() != VK_NULL_HANDLE);
    REQUIRE(shadowApp->shadowSampler() != VK_NULL_HANDLE);
    REQUIRE(shadowApp->pcfRadius() == 1);

    const auto shadowFrame = shadowApp->renderAndCaptureFinal();
    const RawAttachment shadowDepth = shadowApp->readbackShadowDepth();
    REQUIRE((shadowDepth.format == VK_FORMAT_D32_SFLOAT ||
             shadowDepth.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
             shadowDepth.format == VK_FORMAT_D24_UNORM_S8_UINT));
    float minShadowDepth = 1.0f;
    for (std::size_t offset = 0; offset + 3 < shadowDepth.pixels.size(); offset += 4) {
        float value = 1.0f;
        if (shadowDepth.format == VK_FORMAT_D24_UNORM_S8_UINT) {
            std::uint32_t packed = 0;
            std::memcpy(&packed, shadowDepth.pixels.data() + offset, sizeof(packed));
            value = static_cast<float>(packed & 0x00ffffffu) / 16777215.0f;
        } else {
            std::memcpy(&value, shadowDepth.pixels.data() + offset, sizeof(value));
        }
        minShadowDepth = std::min(minShadowDepth, value);
    }
    INFO("shadow depth pass 必须把真实头盔写进深度图，不能保持 clear=1");
    REQUIRE(minShadowDepth < 0.99f);
    std::size_t shadowedPixels = 0;
    double shadowX = 0.0;
    double shadowY = 0.0;
    for (std::uint32_t y = 0; y < shadowFrame.height; ++y) {
        for (std::uint32_t x = 0; x < shadowFrame.width; ++x) {
            const std::size_t offset = pixelOffset(shadowFrame, x, y);
            if (shadowFrame.pixels[offset + 3] < 200) {
                ++shadowedPixels;
                shadowX += x;
                shadowY += y;
            }
        }
    }
    INFO("离屏 alpha 直接携带 PCF visibility，绕过 shadow sampling 时必须归零");
    INFO("绕过世界坐标重建会让正确阴影足迹缩到约千个像素");
    REQUIRE(shadowedPixels > shadowFrame.width * shadowFrame.height / 14);
    INFO("shadow footprint centroid: " << shadowX / shadowedPixels << ", "
         << shadowY / shadowedPixels << "; count=" << shadowedPixels);
    INFO("额外翻转 light-space UV 会把阴影足迹扩散到超过四分之一画面");
    REQUIRE(shadowedPixels < shadowFrame.width * shadowFrame.height / 4);

    CameraState movedCamera = shadowApp->cameraState();
    movedCamera.position = {1.1f, 0.7f, 3.3f};
    movedCamera.yawDegrees = -108.0f;
    movedCamera.pitchDegrees = -8.0f;
    shadowApp->setCameraState(movedCamera);
    (void)shadowApp->renderAndCaptureFinal();
    const RawAttachment movedShadowDepth = shadowApp->readbackShadowDepth();
    INFO("观察相机变化不得改变固定光源生成的 shadow map");
    REQUIRE(movedShadowDepth.format == shadowDepth.format);
    REQUIRE(movedShadowDepth.width == shadowDepth.width);
    REQUIRE(movedShadowDepth.height == shadowDepth.height);
    const bool shadowMapStable = movedShadowDepth.pixels == shadowDepth.pixels;
    REQUIRE(shadowMapStable);

    shadowApp.reset();
    guard.requireClean();
}

TEST_CASE("t06 HDR compute bloom 与 tonemap", "[t06]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Bloom);
    REQUIRE(app->hdrImage().valid());
    REQUIRE(app->bloomImage().valid());
    REQUIRE(app->hdrImage().format == VK_FORMAT_R16G16B16A16_SFLOAT);
    REQUIRE(app->bloomImage().format == VK_FORMAT_R16G16B16A16_SFLOAT);
    REQUIRE(app->bloomPipeline() != VK_NULL_HANDLE);
    REQUIRE(app->tonemapPipeline() != VK_NULL_HANDLE);

    // MoltenVK may compile/translate the compute+post chain lazily on first use.
    // Grade the steady-state frame, matching the native app after its first present.
    (void)app->renderAndCaptureFinal();
    const auto frame = app->renderAndCaptureFinal();
    const BarrierSnapshot& barrier = app->bloomBarrier();
    REQUIRE(barrier.srcStage == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    REQUIRE((barrier.dstStage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) != 0);
    REQUIRE((barrier.dstStage & VK_PIPELINE_STAGE_TRANSFER_BIT) != 0);
    REQUIRE(barrier.srcAccess == VK_ACCESS_SHADER_WRITE_BIT);
    REQUIRE((barrier.dstAccess & VK_ACCESS_SHADER_READ_BIT) != 0);
    REQUIRE((barrier.dstAccess & VK_ACCESS_TRANSFER_READ_BIT) != 0);

    const RawAttachment hdr = app->readbackHdr();
    float maxHdr = 0.0f;
    std::size_t hdrShadowedPixels = 0;
    for (std::size_t offset = 0; offset + 7 < hdr.pixels.size(); offset += 8) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            std::uint16_t value = 0;
            std::memcpy(&value, hdr.pixels.data() + offset + channel * 2, sizeof(value));
            maxHdr = std::max(maxHdr, halfToFloat(value));
        }
        std::uint16_t alpha = 0;
        std::memcpy(&alpha, hdr.pixels.data() + offset + 6, sizeof(alpha));
        if (halfToFloat(alpha) < 0.8f) ++hdrShadowedPixels;
    }
    INFO("lighting pass 必须先产生线性 HDR 高光");
    REQUIRE(maxHdr > 1.0f);
    REQUIRE(hdrShadowedPixels > hdr.width * hdr.height / 14);
    REQUIRE(hdrShadowedPixels < hdr.width * hdr.height / 4);

    const RawAttachment bloom = app->readbackBloom();
    REQUIRE(bloom.bytesPerPixel == 8);
    REQUIRE(bloom.width == frame.width);
    REQUIRE(bloom.height == frame.height);
    float maxBloom = 0.0f;
    for (std::size_t offset = 0; offset + 7 < bloom.pixels.size(); offset += 8) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            std::uint16_t value = 0;
            std::memcpy(&value, bloom.pixels.data() + offset + channel * 2, sizeof(value));
            maxBloom = std::max(maxBloom, halfToFloat(value));
        }
    }
    INFO("真实 Cook-Torrance 镜面高光必须通过 threshold 并扩散进 bloom image");
    REQUIRE(maxBloom > 0.05f);

    const GBufferCapture materialMask = app->renderAndCaptureGBuffer();

    std::uint8_t brightestOutput = 0;
    std::size_t cyanDominantPixels = 0;
    for (std::size_t offset = 0; offset + 3 < frame.pixels.size(); offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            brightestOutput = std::max(brightestOutput, frame.pixels[offset + channel]);
        }
        const int r = frame.pixels[offset + 0];
        const int g = frame.pixels[offset + 1];
        const int b = frame.pixels[offset + 2];
        if (!isClearAlbedo(materialMask.albedoMetallic, offset) &&
            g > r + 30 && b > r + 30 && std::max(g, b) > 100) {
            ++cyanDominantPixels;
        }
    }
    REQUIRE(brightestOutput > 100);
    INFO("Sci-Fi Helmet 不能被屏幕空间青色 emissive 覆盖");
    INFO("青色主导像素数: " << cyanDominantPixels);
    REQUIRE(cyanDominantPixels < frame.width * frame.height / 100);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t07 Slang SPIR-V 替换完整 P04 shader 链", "[t07]") {
    ValidationGuard guard;
    auto slangApp = makeApp(Stage::Slang);
    REQUIRE(slangApp->usingSlang());
    REQUIRE(slangApp->geometryPipeline() != VK_NULL_HANDLE);
    REQUIRE(slangApp->lightingPipeline() != VK_NULL_HANDLE);
    REQUIRE(slangApp->iblPipeline() != VK_NULL_HANDLE);
    REQUIRE(slangApp->shadowPipeline() != VK_NULL_HANDLE);
    REQUIRE(slangApp->bloomPipeline() != VK_NULL_HANDLE);
    REQUIRE(slangApp->tonemapPipeline() != VK_NULL_HANDLE);
    const GBufferCapture slangGbuffer = slangApp->renderAndCaptureGBuffer();
    std::size_t slangAlbedoMagenta = 0;
    for (std::size_t offset = 0; offset + 3 < slangGbuffer.albedoMetallic.pixels.size();
         offset += 4) {
        const int r = slangGbuffer.albedoMetallic.pixels[offset + 0];
        const int g = slangGbuffer.albedoMetallic.pixels[offset + 1];
        const int b = slangGbuffer.albedoMetallic.pixels[offset + 2];
        if (r > g + 30 && b > g + 30 && std::max(r, b) > 100) {
            ++slangAlbedoMagenta;
        }
    }
    INFO("Slang G-buffer albedo 洋红主导像素数: " << slangAlbedoMagenta);
    REQUIRE(slangAlbedoMagenta < slangGbuffer.albedoMetallic.width *
                                     slangGbuffer.albedoMetallic.height / 100);
    const auto slangFrame = slangApp->renderAndCaptureFinal();

    const std::vector<float> slangIbl = slangApp->readbackIbl();
    REQUIRE(slangIbl.size() == 22 * 4);
    REQUIRE(slangIbl[0] == Catch::Approx(0.45f));
    REQUIRE(slangIbl[23] == Catch::Approx(1.0f));
    const RawAttachment slangHdr = slangApp->readbackHdr();
    float maxSlangHdr = 0.0f;
    std::size_t nonFiniteSlangHdr = 0;
    std::size_t magentaSlangHdr = 0;
    std::size_t slangHdrShadowedPixels = 0;
    for (std::size_t offset = 0; offset + 7 < slangHdr.pixels.size(); offset += 8) {
        float rgb[3]{};
        for (std::size_t channel = 0; channel < 3; ++channel) {
            std::uint16_t value = 0;
            std::memcpy(&value, slangHdr.pixels.data() + offset + channel * 2, sizeof(value));
            const float decoded = halfToFloat(value);
            rgb[channel] = decoded;
            if (!std::isfinite(decoded)) ++nonFiniteSlangHdr;
            maxSlangHdr = std::max(maxSlangHdr, decoded);
        }
        if (rgb[0] > rgb[1] + 0.2f && rgb[2] > rgb[1] + 0.2f &&
            std::max(rgb[0], rgb[2]) > 0.4f) ++magentaSlangHdr;
        std::uint16_t alpha = 0;
        std::memcpy(&alpha, slangHdr.pixels.data() + offset + 6, sizeof(alpha));
        if (halfToFloat(alpha) < 0.8f) ++slangHdrShadowedPixels;
    }
    INFO("Slang HDR 非有限通道数: " << nonFiniteSlangHdr);
    INFO("Slang HDR 洋红主导像素数: " << magentaSlangHdr);
    REQUIRE(nonFiniteSlangHdr == 0);
    REQUIRE(maxSlangHdr > 1.0f);
    REQUIRE(slangHdrShadowedPixels > slangHdr.width * slangHdr.height / 14);
    REQUIRE(slangHdrShadowedPixels < slangHdr.width * slangHdr.height / 4);

    const RawAttachment slangBloom = slangApp->readbackBloom();
    std::size_t nonFiniteSlangBloom = 0;
    std::size_t magentaSlangBloom = 0;
    for (std::size_t offset = 0; offset + 7 < slangBloom.pixels.size(); offset += 8) {
        float rgb[3]{};
        for (std::size_t channel = 0; channel < 3; ++channel) {
            std::uint16_t value = 0;
            std::memcpy(&value, slangBloom.pixels.data() + offset + channel * 2,
                        sizeof(value));
            rgb[channel] = halfToFloat(value);
            if (!std::isfinite(rgb[channel])) ++nonFiniteSlangBloom;
        }
        if (rgb[0] > rgb[1] + 0.2f && rgb[2] > rgb[1] + 0.2f &&
            std::max(rgb[0], rgb[2]) > 0.4f) ++magentaSlangBloom;
    }
    INFO("Slang bloom 非有限通道数: " << nonFiniteSlangBloom);
    INFO("Slang bloom 洋红主导像素数: " << magentaSlangBloom);
    REQUIRE(nonFiniteSlangBloom == 0);

    REQUIRE(slangFrame.width == 160);
    REQUIRE(slangFrame.height == 120);
    std::uint8_t slangBrightestOutput = 0;
    std::size_t slangCyanDominantPixels = 0;
    std::size_t slangMagentaDominantPixels = 0;
    for (std::size_t offset = 0; offset + 3 < slangFrame.pixels.size(); offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            slangBrightestOutput = std::max(slangBrightestOutput,
                                            slangFrame.pixels[offset + channel]);
        }
        const int r = slangFrame.pixels[offset + 0];
        const int g = slangFrame.pixels[offset + 1];
        const int b = slangFrame.pixels[offset + 2];
        if (!isClearAlbedo(slangGbuffer.albedoMetallic, offset) &&
            g > r + 30 && b > r + 30 && std::max(g, b) > 100) {
            ++slangCyanDominantPixels;
        }
        if (r > g + 30 && b > g + 30 && std::max(r, b) > 100) {
            ++slangMagentaDominantPixels;
        }
    }
    REQUIRE(slangBrightestOutput > 100);
    INFO("Slang 输出同样不能恢复屏幕空间青色 emissive");
    REQUIRE(slangCyanDominantPixels < slangFrame.width * slangFrame.height / 100);
    INFO("Slang bloom 不能因局部数组未定义值产生大块洋红色污染");
    REQUIRE(slangMagentaDominantPixels < slangFrame.width * slangFrame.height / 100);
    REQUIRE(slangFrame.pixels[pixelOffset(slangFrame, slangFrame.width / 2,
                                          slangFrame.height / 2) + 3] == 255);

    slangApp.reset();
    guard.requireClean();
}
