#include <catch2/catch_test_macros.hpp>

#include "ImageCompare.h"
#include "ResourceApp.h"
#include "rwb/core/ValidationLog.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace p02;
namespace gt = rwb::test;

namespace {

// L1：每个测试开头清空 validation 记录，结尾断言零 error / 零 warning。
struct ValidationGuard {
    ValidationGuard() { rwb::ValidationLog::instance().reset(); }

    void requireClean() const {
        const auto& log = rwb::ValidationLog::instance();
        INFO(log.summary());
        REQUIRE(log.errorCount() == 0);
        REQUIRE(log.warningCount() == 0);
    }
};

AppConfig testConfig() {
    AppConfig c;
    c.width  = 320;   // 小一点，基准图存起来便宜，跑起来也快
    c.height = 240;
    c.highDpiFramebuffer = false; // Retina 上也保持 320x240 像素，和跨平台基准一致
    c.title  = "render-workbook p02 test";
    return c;
}

std::unique_ptr<ResourceApp> makeApp(Stage stage) {
    auto app = std::make_unique<ResourceApp>(testConfig());
    app->initUpTo(stage);
    return app;
}

gt::Image toImage(const rwb::rhi::CapturedImage& frame) {
    gt::Image img;
    img.width  = frame.width;
    img.height = frame.height;
    img.pixels = frame.pixels;
    return img;
}

// L3：和基准图比。基准图缺失时 SKIP 而不是 FAIL ——
// 「基准还没建立」和「你写错了」是两回事。
// borrowed = 这一条用的是「别的用例的」基准图。那种情况必须走只读比对，
// 否则重新生成基准图的那一跑会把被借的那张覆盖成借用方的画面。
void requireMatchesGolden(ResourceApp& app, const std::string& name,
                          bool filteringSensitive, bool borrowed = false) {
    const gt::Image actual = toImage(app.renderAndCapture());

    const auto tol = gt::toleranceForDevice(app.deviceName(), filteringSensitive);
    const gt::CompareResult result = borrowed
        ? gt::compareToGoldenReadOnly(name, actual, tol)
        : gt::compareToGolden(name, actual, tol);

    INFO("设备: " << app.deviceName());
    INFO(result.message);
    if (result.baselineMissing) {
        SKIP("基准图尚未入库, 这不是你的实现有问题: " + result.message);
    }
    REQUIRE(result.passed);
}

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE("t01 顶点与索引缓冲", "[t01]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Buffers);

    REQUIRE(app->vertexBuffer().valid());
    REQUIRE(app->indexBuffer().valid());

    INFO("buffer 的字节数必须刚好装得下场景数据");
    REQUIRE(app->vertexBuffer().size == sizeof(Vertex) * app->vertices().size());
    REQUIRE(app->indexBuffer().size  == sizeof(std::uint32_t) * app->indices().size());

    INFO("顶点/索引缓冲应当放在 device-local 显存里，不该是常驻映射的 —— "
         "常驻映射说明你跳过了 staging，直接在 host-visible 内存里建了它");
    REQUIRE(app->vertexBuffer().mapped == nullptr);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t01 画面：一个彩色四边形", "[t01][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Buffers);
    requireMatchesGolden(*app, "p02-t01", /*filteringSensitive=*/false);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t02 UBO 与 descriptor set", "[t02]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Uniforms);

    REQUIRE(app->descriptorSetLayout() != VK_NULL_HANDLE);
    REQUIRE(app->descriptorPool()      != VK_NULL_HANDLE);

    INFO("每个在飞帧都要有自己的 UBO 和 descriptor set；"
         "只建一份的话，CPU 写第 N+1 帧时会覆盖 GPU 正在读的第 N 帧数据");
    REQUIRE(app->uniformBuffers().size() == ResourceApp::kFramesInFlight);
    REQUIRE(app->descriptorSets().size() == ResourceApp::kFramesInFlight);

    for (const Buffer& b : app->uniformBuffers()) {
        REQUIRE(b.valid());
        INFO("UBO 每帧都要改，应当是常驻映射的 host-visible 内存");
        REQUIRE(b.mapped != nullptr);
        REQUIRE(b.size >= sizeof(CameraUniform));
    }
    for (VkDescriptorSet set : app->descriptorSets()) REQUIRE(set != VK_NULL_HANDLE);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t02 画面：四边形经过相机变换", "[t02][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Uniforms);
    requireMatchesGolden(*app, "p02-t02", /*filteringSensitive=*/false);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t03 push constant 的声明", "[t03]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::PushConstants);

    const std::vector<VkPushConstantRange> ranges = app->pushConstantRangesForTest();
    REQUIRE_FALSE(ranges.empty());

    std::uint32_t total = 0;
    for (const VkPushConstantRange& r : ranges) {
        total = std::max(total, r.offset + r.size);
        INFO("范围声明的阶段里必须包含顶点着色器 —— model 矩阵是在那里用的");
        REQUIRE((r.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) != 0);
    }

    INFO("规范只保证 maxPushConstantsSize >= 128 字节，而且那是所有阶段加起来的总量。"
         "超过 128 的方案在某些设备上直接建不出 pipeline layout");
    REQUIRE(total <= 128);
    REQUIRE(total >= sizeof(ObjectPush));

    INFO("这一阶段的场景是同一份几何画三次，每次一个不同的 model 矩阵");
    REQUIRE(app->draws().size() == 3);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t03 画面：三个各自变换的四边形", "[t03][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::PushConstants);
    requireMatchesGolden(*app, "p02-t03", /*filteringSensitive=*/false);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t04 纹理已上传且 layout 正确", "[t04]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Texture);

    REQUIRE(app->texture().valid());
    REQUIRE(app->texture().width  == ResourceApp::kTextureSize);
    REQUIRE(app->texture().height == ResourceApp::kTextureSize);

    INFO("256x256 的完整 mip 链是 9 级: 256,128,64,32,16,8,4,2,1。"
         "只建 1 级的话 t05 没东西可生成");
    REQUIRE(app->texture().mipLevels == 9);

    app.reset();
    // layout 转换写错了的话，validation 会在这里报出来 —— 这是这道题主要的判据。
    guard.requireClean();
}

TEST_CASE("t04 画面不该变：这一步只是把纹理传上去", "[t04][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Texture);

    INFO("t04 还没有采样器，shader 也还没采样纹理。"
         "如果画面变了，说明你多改了不该改的东西");
    requireMatchesGolden(*app, "p02-t03", /*filteringSensitive=*/false, /*borrowed=*/true);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t05 image view 与 sampler", "[t05]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Sampler);

    REQUIRE(app->texture().view != VK_NULL_HANDLE);
    REQUIRE(app->sampler()      != VK_NULL_HANDLE);

    app.reset();
    // mip 链生成里最常见的错误 —— 最后一级忘了转 layout —— 会在这里被抓住
    guard.requireClean();
}

TEST_CASE("t05 画面：铺了棋盘格的地面", "[t05][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Sampler);
    // 这张图的样子高度依赖纹理过滤实现，跨 GPU 逐像素永远对不齐。
    // 真实 GPU 上退化为结构比对，严格判分交给 CI 的 lavapipe。
    requireMatchesGolden(*app, "p02-t05", /*filteringSensitive=*/true);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t06 深度缓冲", "[t06]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Depth);

    REQUIRE(app->depthImage().valid());
    REQUIRE(app->depthImage().view != VK_NULL_HANDLE);

    INFO("深度图必须和颜色附件同尺寸，否则 framebuffer 建不出来");
    REQUIRE(app->depthImage().width  == app->swapchain().extent().width);
    REQUIRE(app->depthImage().height == app->swapchain().extent().height);

    const VkFormat f = app->depthImage().format;
    INFO("挑出来的格式必须真的是深度格式");
    REQUIRE((f == VK_FORMAT_D32_SFLOAT || f == VK_FORMAT_D32_SFLOAT_S8_UINT ||
             f == VK_FORMAT_D24_UNORM_S8_UINT));

    INFO("这一阶段的场景是「地面 + 立方体」两次 draw");
    REQUIRE(app->draws().size() == 2);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t06 画面：立方体的前后关系正确", "[t06][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Depth);
    requireMatchesGolden(*app, "p02-t06", /*filteringSensitive=*/true);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t07 glTF 加载", "[t07]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Model);

    const std::vector<MeshDraw>& draws = app->draws();

    INFO("资产里有三个 node，各带一个 mesh，所以在「地面 + 立方体」之外还要多三次 draw");
    REQUIRE(draws.size() == 5);

    // 后三次 draw 来自 glTF。它们的顶点追加在硬编码几何之后，
    // 所以 vertexOffset 必须非零 —— 忘了它的话模型会画出立方体的顶点。
    const MeshDraw& pyramid = draws[2];
    const MeshDraw& prism   = draws[3];
    const MeshDraw& octa    = draws[4];

    INFO("多 mesh 合并进同一对 buffer 时，第二个之后的 draw 必须带 vertexOffset");
    REQUIRE(pyramid.vertexOffset > 0);
    REQUIRE(prism.vertexOffset   > pyramid.vertexOffset);
    REQUIRE(octa.vertexOffset    > prism.vertexOffset);
    REQUIRE(prism.firstIndex     > pyramid.firstIndex);
    REQUIRE(octa.firstIndex      > prism.firstIndex);

    REQUIRE(pyramid.indexCount > 0);
    REQUIRE(prism.indexCount   > 0);
    REQUIRE(octa.indexCount    > 0);

    // 一个 mesh 占用的顶点数 = 下一个 mesh 的 vertexOffset 减去自己的。
    // 最后一个一直数到末尾。
    const auto vertexCountOf = [&](std::size_t k) -> std::size_t {
        const std::int32_t next = (k + 1 < draws.size())
                                      ? draws[k + 1].vertexOffset
                                      : static_cast<std::int32_t>(app->vertices().size());
        return static_cast<std::size_t>(next - draws[k].vertexOffset);
    };

    for (std::size_t k = 2; k < draws.size(); ++k) {
        const MeshDraw&   d      = draws[k];
        const std::size_t vcount = vertexCountOf(k);
        REQUIRE(vcount > 0);

        std::vector<bool> referenced(vcount, false);
        for (std::uint32_t i = d.firstIndex; i < d.firstIndex + d.indexCount; ++i) {
            const std::uint32_t local = app->indices()[i];
            INFO("索引值必须落在它自己那段顶点范围内");
            REQUIRE(local < vcount);
            referenced[local] = true;
        }

        // 资产刻意让三个 mesh 用了三种索引 componentType（u8 / u32 / u16）。
        // 只按一种去读的话，读出来的索引值仍然「看起来合法」（多半偏小、大量重复 0），
        // 单纯的落界检查抓不到。但每个 mesh 的每个顶点本来都至少被引用一次，
        // 一旦有顶点没人引用，就说明这段索引根本没解对。
        INFO("mesh #" << k << " 有顶点没被任何索引引用 —— "
             "多半是索引的 componentType 读错了（资产里 u8 / u16 / u32 三种都有）");
        REQUIRE(std::all_of(referenced.begin(), referenced.end(),
                            [](bool used) { return used; }));
    }

    // --- byteStride ---------------------------------------------------------
    // Pyramid 的 POSITION / TEXCOORD_0 / COLOR_0 交错在同一个 bufferView 里
    // （byteStride = 32）。按 sizeof(vec3) 连续读的加载器会把 uv 和颜色的字节
    // 当成坐标读出来 —— 而 uv 和颜色都在 [0,1]，所以顶点不会飞到天上去，
    // 只会挤成一团。局部 y 范围是能抓住它的最直接的量。
    {
        float minY = 1e9f, maxY = -1e9f;
        for (std::size_t i = 0; i < vertexCountOf(2); ++i) {
            const float y = app->vertices()[static_cast<std::size_t>(pyramid.vertexOffset) + i].pos.y;
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
        INFO("Pyramid 的局部 y 范围应当是 [-0.20, +0.60]，实测 ["
             << minY << ", " << maxY << "] —— 对不上就是没处理 bufferView 的 byteStride");
        REQUIRE(std::abs(minY - (-0.20f)) < 1e-4f);
        REQUIRE(std::abs(maxY - (+0.60f)) < 1e-4f);
    }

    // --- node 的 matrix 形式 -------------------------------------------------
    // Pyramid 的 node 写的是 4x4 matrix（列主序），不是 TRS。
    // loadModel 在它之上又乘了 translate(0, -0.55, 0)，所以最终的平移列是
    // (-1.12, 0.05 - 0.55, 0.05)。只认 TRS 的加载器会把它当成单位矩阵，
    // 模型缩回原点；顺手转置一下的话 x/z 会跑到别处。
    INFO("Pyramid 的 node 用的是 matrix 形式，只处理 TRS 的加载器会让它缩回原点");
    REQUIRE(std::abs(pyramid.model[3][0] - (-1.12f)) < 1e-3f);
    REQUIRE(std::abs(pyramid.model[3][1] - (-0.50f)) < 1e-3f);
    REQUIRE(std::abs(pyramid.model[3][2] - (+0.05f)) < 1e-3f);

    // --- node 的 rotation ---------------------------------------------------
    // Prism 的 node 带一个绕斜轴 52 度的旋转（资产由 assets/make_shapes_gltf.py 生成）。
    {
        // 完全忽略 rotation 的加载器给出的是纯缩放矩阵，左上 3x3 非对角元素全是 0。
        float offDiagonal = 0.0f;
        for (int c = 0; c < 3; ++c) {
            for (int r = 0; r < 3; ++r) {
                if (r != c) offDiagonal = std::max(offDiagonal, std::abs(prism.model[c][r]));
            }
        }
        INFO("Prism 的 model 矩阵左上 3x3 是纯对角的 —— node 的 rotation 被整个忽略了");
        REQUIRE(offDiagonal > 0.1f);

        // 转了、但转错角度，上面那条抓不到 —— 四元数分量顺序搞反正是这一类：
        // (x,y,z,w) 按 (w,x,y,z) 喂进去，分量置换保持模长为 1，
        // 得到的仍是合法的单位四元数，非对角元素照样非零，只是转到了别的角度。
        //
        // 这里查旋转角本身。左上 3x3 是 scale * R，列长即 scale，除掉之后
        // trace(R) = 1 + 2cos(theta)。资产里的角度是 52 度；把分量顺序写反
        // 会得到 2*acos(0.1099) = 167 度，差得一眼就分得开。
        //
        // 为什么不靠 golden 抓：这个场景在真实 GPU 上走的是结构比对（16x16 块比
        // 平均色），实测这个错误只让 2.00% 的块超阈 —— 正好卡在容忍线上，判为通过。
        // 严格逐像素只在 CI 的 lavapipe 上跑，本机做题时抓不到。
        const glm::mat3 scaled(prism.model);
        const float     scale = glm::length(scaled[0]);
        REQUIRE(scale > 1e-3f);

        const glm::mat3 rotation = scaled / scale;
        const float     trace    = rotation[0][0] + rotation[1][1] + rotation[2][2];
        const float     degrees  =
            glm::degrees(std::acos(glm::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f)));

        INFO("Prism 的旋转角实测 " << degrees << " 度，资产里是 52 度。"
             "转了但角度不对 —— 先检查四元数的分量顺序："
             "glTF 的 rotation 是 (x, y, z, w)，glm::quat 的构造函数是 (w, x, y, z)");
        REQUIRE(std::abs(degrees - 52.0f) < 1.0f);
    }

    app.reset();
    guard.requireClean();
}

TEST_CASE("t07 画面：模型出现在场景里", "[t07][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Model);
    requireMatchesGolden(*app, "p02-t07", /*filteringSensitive=*/true);
    app.reset();
    guard.requireClean();
}

TEST_CASE("t08 多重采样", "[t08]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Msaa);

    const VkSampleCountFlagBits samples = app->sampleCount();
    INFO("选出来的采样数必须是 2 的幂，且不超过本课封顶的 4x");
    REQUIRE((samples == VK_SAMPLE_COUNT_1_BIT || samples == VK_SAMPLE_COUNT_2_BIT ||
             samples == VK_SAMPLE_COUNT_4_BIT));

    // 硬件支持的必要条件：颜色和深度都要支持这个采样数。
    const VkPhysicalDeviceLimits& limits = app->context().properties().limits;
    const VkSampleCountFlags supported =
        limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    INFO("只查颜色附件的采样能力是经典错误 —— 有的硬件颜色支持 8x 而深度只到 4x");
    REQUIRE((static_cast<VkSampleCountFlags>(samples) & supported) != 0);

    if (samples == VK_SAMPLE_COUNT_1_BIT) {
        WARN("这台设备只支持单采样（软件渲染器上很常见），"
             "多重采样的那条路径这次没被真正跑到");
    } else {
        INFO("开了 MSAA 就必须另开一张多采样的离屏颜色附件 —— "
             "swapchain 的图像永远是单采样的");
        REQUIRE(app->colorTarget().valid());
        REQUIRE(app->colorTarget().view != VK_NULL_HANDLE);
    }

    app.reset();
    guard.requireClean();
}

TEST_CASE("t08 MSAA 只该改变边缘，不该改变构图", "[t08][golden]") {
    ValidationGuard guard;

    // 和 t07 的场景完全相同，区别只有多重采样。
    // 所以这里刻意「不」建自己的基准图 —— 结构上它必须和 t07 一致，
    // 而逐像素的边缘差异正是 MSAA 该有的效果。
    auto app = makeApp(Stage::Msaa);
    const gt::Image msaa = toImage(app->renderAndCapture());
    const VkSampleCountFlagBits samples = app->sampleCount();
    const std::string device = app->deviceName();
    app.reset();

    const gt::CompareResult structural =
        gt::compareToGoldenReadOnly("p02-t07", msaa, gt::CompareTolerance::structural());
    INFO("设备: " << device);
    INFO(structural.message);
    if (structural.baselineMissing) {
        SKIP("基准图尚未入库: " + structural.message);
    }
    REQUIRE(structural.passed);

    if (samples != VK_SAMPLE_COUNT_1_BIT) {
        // 真开了 MSAA 的话，边缘像素必须和单采样版本不同 ——
        // 否则说明 resolve 附件没接对，画面其实还是单采样的那张。
        auto plain = makeApp(Stage::Model);
        const gt::Image noMsaa = toImage(plain->renderAndCapture());
        plain.reset();

        const gt::CompareResult exact =
            gt::compareImages(msaa, noMsaa, gt::CompareTolerance::strict());
        INFO("和单采样版本逐像素比: " << exact.message);
        INFO("完全一致说明 resolve 没生效 —— 检查 subpass 的 pResolveAttachments "
             "和 framebuffer 里附件的顺序");
        REQUIRE_FALSE(exact.passed);
    }

    guard.requireClean();
}
