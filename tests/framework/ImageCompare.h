#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rwb::test {

// 8-bit RGBA 图像
struct Image {
    std::uint32_t             width  = 0;
    std::uint32_t             height = 0;
    std::vector<std::uint8_t> pixels; // width * height * 4

    bool empty() const { return width == 0 || height == 0 || pixels.empty(); }
};

Image loadPng(const std::string& path);
void  savePng(const std::string& path, const Image& image);

// 比较容差。
//
// 为什么不能全平台都要求逐像素相等：
//   NVIDIA、Apple Silicon(MoltenVK/Metal)、lavapipe 三者的光栅化规则、
//   浮点舍入、纹理过滤实现都不同，同一份正确代码在三边出的图必然有微小差异。
//
// 所以本仓库的判分策略是：
//   * 基准图由 CI 的 lavapipe（纯 CPU，确定性）生成并入库
//   * CI 上用 strict()：几乎逐像素
//   * 本机 GPU 用 lenient()：只作冒烟，防止你把画面写崩却没人发现
struct CompareTolerance {
    int    maxChannelDelta  = 2;      // 单通道最大允许差值 (0-255)
    double maxFailingPixelR = 0.001;  // 允许超过上面阈值的像素占比

    static CompareTolerance strict()  { return CompareTolerance{1, 0.0}; }
    static CompareTolerance lenient() { return CompareTolerance{8, 0.02}; }
};

struct CompareResult {
    bool          passed        = false;
    // 基准图尚未建立。这不等于「你写错了」，测试应当 SKIP 而不是 FAIL。
    // 基准图由 CI 的 lavapipe job 生成后入库。
    bool          baselineMissing = false;
    std::uint32_t failingPixels = 0;
    std::uint32_t totalPixels   = 0;
    int           worstDelta    = 0;
    std::string   message;
};

CompareResult compareImages(const Image& actual, const Image& expected,
                            CompareTolerance tol = CompareTolerance{});

// 与 tests/golden/<name>.png 比对。
//
// 行为：
//   * 基准图不存在           -> 把 actual 落盘, 返回失败并提示如何入库
//   * RWB_UPDATE_GOLDEN=1 -> 覆盖基准图并通过（只应在 CI/lavapipe 上用）
//   * 失败                   -> 落盘 <name>.actual.png 与 <name>.diff.png 便于肉眼定位
CompareResult compareToGolden(const std::string& name, const Image& actual,
                              CompareTolerance tol = CompareTolerance{});

// 环境相关目录（由 CTest 通过环境变量注入，见 cmake/Testing.cmake）
std::string goldenDir();
std::string outputDir();

// 当前是否跑在软件渲染 (lavapipe/WARP) 上 —— 决定用 strict 还是 lenient
bool isSoftwareRenderer(const std::string& deviceName);
CompareTolerance toleranceForDevice(const std::string& deviceName);

} // namespace rwb::test
