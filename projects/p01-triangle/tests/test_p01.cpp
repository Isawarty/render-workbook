#include <catch2/catch_test_macros.hpp>

#include "ImageCompare.h"
#include "TriangleApp.h"
#include "rwb/core/ValidationLog.h"

#include <memory>

using namespace p01;
namespace gt = rwb::test;

namespace {

// 每个测试开头清空 validation 记录，结尾断言零报错 —— 这就是 L1。
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
    c.width  = 320;    // 小一点，golden 图存起来便宜，跑起来也快
    c.height = 240;
    c.highDpiFramebuffer = false; // Retina 上也保持 320x240 像素，和跨平台基准一致
    c.title  = "render-workbook p01 test";
    return c;
}

std::unique_ptr<TriangleApp> makeApp(Stage stage) {
    auto app = std::make_unique<TriangleApp>(testConfig());
    app->initUpTo(stage);
    return app;
}

} // namespace

TEST_CASE("t01 instance 与 debug messenger", "[t01]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Instance);

    REQUIRE(app->instance() != VK_NULL_HANDLE);
    INFO("开启了 validation 就必须建出 debug messenger，否则 L1 测试收不到任何消息");
    REQUIRE(app->debugMessenger() != VK_NULL_HANDLE);

    app.reset();          // 触发 cleanup，销毁期的 validation 报错也要算进去
    guard.requireClean();
}

TEST_CASE("t02 物理设备与逻辑设备", "[t02]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Device);

    REQUIRE(app->physicalDevice() != VK_NULL_HANDLE);
    REQUIRE(app->device()         != VK_NULL_HANDLE);

    INFO("选中的设备: " << app->physicalDeviceName());
    REQUIRE(app->queueFamilies().complete());

    app.reset();
    guard.requireClean();
}

TEST_CASE("t03 swapchain 与 image view", "[t03]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Swapchain);

    REQUIRE(app->swapchain() != VK_NULL_HANDLE);
    REQUIRE_FALSE(app->swapchainImages().empty());

    INFO("image view 数量必须和 swapchain 图像数一一对应");
    REQUIRE(app->swapchainImageViews().size() == app->swapchainImages().size());

    for (VkImageView v : app->swapchainImageViews()) REQUIRE(v != VK_NULL_HANDLE);

    REQUIRE(app->swapchainExtent().width  > 0);
    REQUIRE(app->swapchainExtent().height > 0);
    REQUIRE(app->swapchainFormat() != VK_FORMAT_UNDEFINED);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t04 render pass 与 framebuffer", "[t04]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::RenderPass);

    REQUIRE(app->renderPass() != VK_NULL_HANDLE);

    INFO("每张 swapchain 图像都要有自己的 framebuffer，但共享同一个 render pass");
    REQUIRE(app->framebuffers().size() == app->swapchainImageViews().size());
    for (VkFramebuffer fb : app->framebuffers()) REQUIRE(fb != VK_NULL_HANDLE);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t05 graphics pipeline", "[t05]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Pipeline);

    REQUIRE(app->pipeline() != VK_NULL_HANDLE);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t06 command pool 与 command buffer", "[t06]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Commands);

    REQUIRE(app->commandPool() != VK_NULL_HANDLE);
    REQUIRE(app->commandBuffers().size() == app->framesInFlight());
    for (VkCommandBuffer cb : app->commandBuffers()) REQUIRE(cb != VK_NULL_HANDLE);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t07 同步与呈现：能连续画若干帧且 validation 干净", "[t07]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Sync);

    INFO("这一条最容易挂在 fence 上：如果 acquire 之前就 vkResetFences，"
         "遇到 OUT_OF_DATE 提前返回时会留下永不 signal 的 fence，下一帧死锁");
    REQUIRE_NOTHROW(app->run(5));

    app.reset();
    guard.requireClean();
}

TEST_CASE("t07 渲染结果与基准图一致", "[t07][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Sync);

    app->run(2);
    const TriangleApp::CapturedFrame frame = app->renderAndCapture();

    REQUIRE(frame.width  == app->swapchainExtent().width);
    REQUIRE(frame.height == app->swapchainExtent().height);

    gt::Image actual;
    actual.width  = frame.width;
    actual.height = frame.height;
    actual.pixels = frame.pixels;

    // 容差按设备自动选：软件渲染(lavapipe/WARP) 用严格阈值，本机 GPU 用宽松阈值。
    // 基准图只应由 CI 的 lavapipe 生成 —— 跨 GPU 的像素永远对不齐。
    const auto tol = gt::toleranceForDevice(app->physicalDeviceName());
    const gt::CompareResult result = gt::compareToGolden("p01-triangle", actual, tol);

    INFO("设备: " << app->physicalDeviceName());
    INFO(result.message);
    if (result.baselineMissing) {
        SKIP("基准图尚未入库, 这不是你的实现有问题: " + result.message);
    }
    REQUIRE(result.passed);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t08 swapchain 能被重建", "[t08]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::Resize);

    const std::uint32_t before = app->swapchainGeneration();
    app->run(2);

    app->requestResize();
    REQUIRE_NOTHROW(app->run(3));

    INFO("requestResize 之后必须真的走过一次重建路径；"
         "generation 没涨说明 drawFrame 里没有检查 m_framebufferResized");
    REQUIRE(app->swapchainGeneration() > before);

    INFO("重建之后还要能继续正常渲染");
    REQUIRE_NOTHROW(app->run(3));

    app.reset();
    guard.requireClean();
}

TEST_CASE("t09 多帧并行的资源数量正确", "[t09]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::FramesInFlight);

    REQUIRE(app->framesInFlight() == TriangleApp::kMaxFramesInFlight);

    INFO("每个在飞的帧都要有自己的 command buffer，否则 CPU 会覆写 GPU 正在读的那份");
    REQUIRE(app->commandBuffers().size() == TriangleApp::kMaxFramesInFlight);

    app.reset();
    guard.requireClean();
}

TEST_CASE("t09 多帧并行下画面仍然正确", "[t09][golden]") {
    ValidationGuard guard;
    auto app = makeApp(Stage::FramesInFlight);

    // 跑得比 framesInFlight 多好几轮，逼出「帧槽位复用」相关的同步错误
    app->run(10);
    const TriangleApp::CapturedFrame frame = app->renderAndCapture();

    gt::Image actual;
    actual.width  = frame.width;
    actual.height = frame.height;
    actual.pixels = frame.pixels;

    const auto tol = gt::toleranceForDevice(app->physicalDeviceName());
    const gt::CompareResult result = gt::compareToGolden("p01-triangle", actual, tol);

    INFO("多帧并行不应改变画面，只应改变吞吐。画面变了说明同步写错了");
    INFO(result.message);
    if (result.baselineMissing) {
        SKIP("基准图尚未入库, 这不是你的实现有问题: " + result.message);
    }
    REQUIRE(result.passed);

    app.reset();
    guard.requireClean();
}
