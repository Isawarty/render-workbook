#include <catch2/catch_test_macros.hpp>

#include "EnvProbe.h"
#include "ImageCompare.h"

#include <volk.h>

#include <string>

// P0 的测试回答一个问题：这台机器能不能开始做这门课？
// 它不判你写得对不对（还没让你写任何东西），只判环境。

TEST_CASE("volk 能加载 Vulkan loader", "[vulkan]") {
    const p00::EnvReport report = p00::probeEnvironment();
    INFO(report.error);
    REQUIRE(report.volkOk);
}

TEST_CASE("能创建 VkInstance", "[vulkan]") {
    const p00::EnvReport report = p00::probeEnvironment();
    INFO(report.error);
    REQUIRE(report.instanceOk);
}

TEST_CASE("至少有一个支持 graphics + compute 的物理设备", "[vulkan]") {
    const p00::EnvReport report = p00::probeEnvironment();
    REQUIRE(report.instanceOk);
    REQUIRE_FALSE(report.devices.empty());

    bool found = false;
    for (const p00::DeviceInfo& d : report.devices) {
        INFO("设备: " << d.name << "  API " << p00::versionToString(d.apiVersion));
        if (d.hasGraphics && d.hasCompute) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("物理设备至少支持 Vulkan 1.2", "[vulkan]") {
    // 本课程全程按 Vulkan 1.2 写（MoltenVK 对 1.3 的支持仍不完整）。
    const p00::EnvReport report = p00::probeEnvironment();
    REQUIRE(report.instanceOk);
    REQUIRE_FALSE(report.devices.empty());

    bool found = false;
    for (const p00::DeviceInfo& d : report.devices) {
        if (d.apiVersion >= VK_API_VERSION_1_2) found = true;
    }
    INFO("没有任何设备支持 Vulkan 1.2 —— 请更新显卡驱动");
    REQUIRE(found);
}

TEST_CASE("validation layer 可用", "[vulkan]") {
    // L1 测试整层都建立在 validation layer 上。
    // 它来自 Vulkan SDK，不是驱动自带的。
    const p00::EnvReport report = p00::probeEnvironment();
    REQUIRE(report.instanceOk);
    INFO("找不到 VK_LAYER_KHRONOS_validation。\n"
         "Windows/macOS: 安装 LunarG Vulkan SDK\n"
         "Linux: apt install vulkan-validationlayers");
    REQUIRE(report.validationLayerAvailable);
}

TEST_CASE("构建期 GLSL -> SPIR-V 链路可用", "[shaders]") {
    // 本仓库不依赖 SDK 里的 glslc，而是用 FetchContent 构建出的 glslang。
    // 这条测试确认它真的产出了合法 SPIR-V。
    const std::string spv = std::string(RWB_SHADER_DIR) + "/probe.comp.spv";
    INFO("期望的 SPIR-V 路径: " << spv);

    std::size_t words = 0;
    REQUIRE_NOTHROW(words = p00::probeCompiledShader(spv));
    REQUIRE(words > 16);   // 合法的 SPIR-V 至少有 5 word 头 + 若干指令
}

TEST_CASE("测试框架的目录注入正常", "[shaders]") {
    // CTest 应当通过环境变量注入基准图目录和输出目录（见 cmake/Testing.cmake）
    INFO("golden dir: " << rwb::test::goldenDir());
    INFO("output dir: " << rwb::test::outputDir());
    REQUIRE_FALSE(rwb::test::goldenDir().empty());
    REQUIRE_FALSE(rwb::test::outputDir().empty());
}
