#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace p00 {

struct DeviceInfo {
    std::string   name;
    std::uint32_t apiVersion    = 0;
    std::uint32_t driverVersion = 0;
    std::uint32_t vendorId      = 0;
    bool          isDiscrete    = false;
    bool          hasCompute    = false;
    bool          hasGraphics   = false;
};

struct EnvReport {
    bool                    volkOk            = false;
    bool                    instanceOk        = false;
    std::uint32_t           instanceApiVersion = 0;
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> instanceLayers;
    bool                    validationLayerAvailable = false;
    std::vector<DeviceInfo> devices;
    std::string             error;   // 非空表示探测中途失败
};

// 走一遍最小 Vulkan 初始化，把环境情况报告出来。
//
// P0 不是挖空题：它的唯一职责是让你在三台机器上都确认
// 「工具链装对了、Vulkan 跑得起来、shader 编得出来」。
EnvReport probeEnvironment();

// 把 SPIR-V 读进来并做基本校验，用来确认构建期的 glslang 真的产出了东西。
// 返回 SPIR-V 的 word 数；失败时抛异常。
std::size_t probeCompiledShader(const std::string& spvPath);

std::string versionToString(std::uint32_t version);

} // namespace p00
