#include "rwb/core/File.h"

#include "rwb/core/Log.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace rwb {

std::vector<std::uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        throw std::runtime_error(format("打不开文件: %s", path.c_str()));
    }
    const std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (size > 0 && !f.read(reinterpret_cast<char*>(data.data()), size)) {
        throw std::runtime_error(format("读文件失败: %s", path.c_str()));
    }
    return data;
}

std::vector<std::uint32_t> readSpirv(const std::string& path) {
    const std::vector<std::uint8_t> bytes = readBinaryFile(path);

    if (bytes.empty()) {
        throw std::runtime_error(format("SPIR-V 是空文件: %s (shader 可能没编译成功)", path.c_str()));
    }
    if (bytes.size() % 4 != 0) {
        throw std::runtime_error(
            format("SPIR-V 大小 %zu 不是 4 的倍数: %s", bytes.size(), path.c_str()));
    }

    std::vector<std::uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());

    constexpr std::uint32_t kSpirvMagic = 0x07230203u;
    if (words[0] != kSpirvMagic) {
        throw std::runtime_error(
            format("SPIR-V magic 不对 (读到 0x%08X, 期望 0x07230203): %s", words[0], path.c_str()));
    }
    return words;
}

} // namespace rwb
