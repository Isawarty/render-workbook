#include "EnvProbe.h"

#include "rwb/core/Log.h"

#include <cstdio>
#include <exception>
#include <string>

int main() {
    using namespace p00;

    std::printf("=== render-workbook P0 环境自检 ===\n\n");

    EnvReport report;
    try {
        report = probeEnvironment();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "探测过程抛异常: %s\n", e.what());
        return 2;
    }

    if (!report.error.empty()) {
        std::fprintf(stderr, "%s\n", report.error.c_str());
        return 1;
    }

    std::printf("volk 加载            : OK\n");
    std::printf("instance API 版本    : %s\n", versionToString(report.instanceApiVersion).c_str());
    std::printf("instance 扩展数      : %zu\n", report.instanceExtensions.size());
    std::printf("instance layer 数    : %zu\n", report.instanceLayers.size());
    std::printf("validation layer     : %s\n",
                report.validationLayerAvailable
                    ? "可用"
                    : "不可用  <-- L1 测试需要它, 请安装 Vulkan SDK");
    std::printf("\n物理设备 (%zu 个):\n", report.devices.size());

    for (const DeviceInfo& d : report.devices) {
        std::printf("  - %s\n", d.name.c_str());
        std::printf("      API      : %s\n", versionToString(d.apiVersion).c_str());
        std::printf("      类型     : %s\n", d.isDiscrete ? "独立显卡" : "集成/软件");
        std::printf("      队列能力 : %s%s\n",
                    d.hasGraphics ? "graphics " : "",
                    d.hasCompute  ? "compute"   : "");
    }

#ifdef RWB_SHADER_DIR
    const std::string spv = std::string(RWB_SHADER_DIR) + "/probe.comp.spv";
    try {
        const std::size_t words = probeCompiledShader(spv);
        std::printf("\nshader 编译链路      : OK (probe.comp.spv, %zu words)\n", words);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nshader 编译链路      : 失败 - %s\n", e.what());
        return 3;
    }
#endif

    std::printf("\n环境就绪。下一步: docs/00-roadmap.md -> projects/p01-triangle/README.md\n");
    return 0;
}
