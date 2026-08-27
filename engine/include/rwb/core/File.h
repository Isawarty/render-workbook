#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rwb {

// 读整个文件为字节。找不到文件时抛 std::runtime_error（附路径）。
std::vector<std::uint8_t> readBinaryFile(const std::string& path);

// 读 SPIR-V。除了读字节，还会校验：
//   * 大小是 4 的倍数
//   * magic number == 0x07230203
// 这两个检查能在 P1-t05 帮你迅速区分「shader 没编译出来」和「pipeline 建错了」。
std::vector<std::uint32_t> readSpirv(const std::string& path);

} // namespace rwb
