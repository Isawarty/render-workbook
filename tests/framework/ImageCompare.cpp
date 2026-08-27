#include "ImageCompare.h"

#include "NoDialogs.h"

#include "rwb/core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace fs = std::filesystem;

namespace rwb::test {
namespace {

// 引用一次 NoDialogs 里的符号，防止链接器把整个 TU 丢掉（静态库的经典陷阱）。
const int g_crashDialogsDisabled = installCrashDialogSuppression();

std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

std::string goldenDir() { (void)g_crashDialogsDisabled; return envOr("RWB_GOLDEN_DIR", "tests/golden"); }
std::string outputDir() { return envOr("RWB_OUTPUT_DIR", "tests-output"); }

Image loadPng(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        throw std::runtime_error(format("读 PNG 失败: %s (%s)", path.c_str(), stbi_failure_reason()));
    }
    Image img;
    img.width  = static_cast<std::uint32_t>(w);
    img.height = static_cast<std::uint32_t>(h);
    img.pixels.assign(data, data + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(data);
    return img;
}

void savePng(const std::string& path, const Image& image) {
    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }
    const int stride = static_cast<int>(image.width) * 4;
    if (!stbi_write_png(path.c_str(), static_cast<int>(image.width),
                        static_cast<int>(image.height), 4, image.pixels.data(), stride)) {
        throw std::runtime_error(format("写 PNG 失败: %s", path.c_str()));
    }
}

CompareResult compareImages(const Image& actual, const Image& expected, CompareTolerance tol) {
    CompareResult r;
    r.totalPixels = actual.width * actual.height;

    if (actual.width != expected.width || actual.height != expected.height) {
        r.message = format("尺寸不一致: 实际 %ux%u, 基准 %ux%u",
                           actual.width, actual.height, expected.width, expected.height);
        return r;
    }

    const std::size_t n = actual.pixels.size();
    for (std::size_t i = 0; i + 3 < n; i += 4) {
        int worstForPixel = 0;
        for (int c = 0; c < 4; ++c) {
            const int d = std::abs(static_cast<int>(actual.pixels[i + c]) -
                                   static_cast<int>(expected.pixels[i + c]));
            worstForPixel = std::max(worstForPixel, d);
        }
        r.worstDelta = std::max(r.worstDelta, worstForPixel);
        if (worstForPixel > tol.maxChannelDelta) {
            ++r.failingPixels;
        }
    }

    const double ratio = r.totalPixels ? static_cast<double>(r.failingPixels) / r.totalPixels : 1.0;
    r.passed  = (ratio <= tol.maxFailingPixelR);
    r.message = format("超差像素 %u/%u (%.4f%%), 最大通道差 %d; 阈值: delta<=%d 且占比<=%.4f%%",
                       r.failingPixels, r.totalPixels, ratio * 100.0,
                       r.worstDelta, tol.maxChannelDelta, tol.maxFailingPixelR * 100.0);
    return r;
}

bool isSoftwareRenderer(const std::string& deviceName) {
    const std::string n = toLower(deviceName);
    return n.find("llvmpipe")    != std::string::npos ||
           n.find("lavapipe")    != std::string::npos ||
           n.find("swiftshader") != std::string::npos ||
           n.find("warp")        != std::string::npos ||
           n.find("software")    != std::string::npos;
}

CompareTolerance toleranceForDevice(const std::string& deviceName) {
    return isSoftwareRenderer(deviceName) ? CompareTolerance::strict()
                                          : CompareTolerance::lenient();
}

CompareResult compareToGolden(const std::string& name, const Image& actual, CompareTolerance tol) {
    const fs::path golden = fs::path(goldenDir()) / (name + ".png");
    const fs::path outDir = fs::path(outputDir());

    const char* update = std::getenv("RWB_UPDATE_GOLDEN");
    if (update && std::string(update) == "1") {
        savePng(golden.string(), actual);
        CompareResult r;
        r.passed  = true;
        r.message = format("已更新基准图: %s", golden.string().c_str());
        return r;
    }

    if (!fs::exists(golden)) {
        const fs::path dump = outDir / (name + ".actual.png");
        savePng(dump.string(), actual);
        CompareResult r;
        r.baselineMissing = true;
        r.message = format(
            "基准图不存在: %s\n"
            "  实际结果已保存到: %s\n"
            "  基准图应由 CI 的 lavapipe job 用 RWB_UPDATE_GOLDEN=1 生成后入库;\n"
            "  不要用本机 GPU 出的图当基准 —— 跨 GPU 不一致。",
            golden.string().c_str(), dump.string().c_str());
        return r;
    }

    const Image expected = loadPng(golden.string());
    CompareResult r = compareImages(actual, expected, tol);

    if (!r.passed) {
        const fs::path actualPath = outDir / (name + ".actual.png");
        const fs::path diffPath   = outDir / (name + ".diff.png");
        savePng(actualPath.string(), actual);

        if (actual.width == expected.width && actual.height == expected.height) {
            Image diff = actual;
            for (std::size_t i = 0; i + 3 < diff.pixels.size(); i += 4) {
                for (int c = 0; c < 3; ++c) {
                    const int d = std::abs(static_cast<int>(actual.pixels[i + c]) -
                                           static_cast<int>(expected.pixels[i + c]));
                    diff.pixels[i + c] = static_cast<std::uint8_t>(std::min(255, d * 16));
                }
                diff.pixels[i + 3] = 255;
            }
            savePng(diffPath.string(), diff);
            r.message += format("\n  实际: %s\n  差异(x16 增强): %s",
                                actualPath.string().c_str(), diffPath.string().c_str());
        } else {
            r.message += format("\n  实际: %s", actualPath.string().c_str());
        }
    }
    return r;
}

} // namespace rwb::test
