#pragma once

#include "rwb/rhi/Vk.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rwb::rhi {

// SPIR-V 加载与 VkShaderModule 创建。
//
// 你在 P1-t05 写过 createShaderModule；这里是它的 RAII 版。
// 「怎么从 GLSL 编到 SPIR-V」在构建期就做完了（cmake/CompileShaders.cmake 调 glslangValidator），
// 运行期只负责把 .spv 喂给驱动。

class ShaderModule {
public:
    ShaderModule() = default;
    ShaderModule(VkDevice device, const std::vector<std::uint32_t>& spirv);

    // 从 .spv 文件加载。路径通常是 RWB_SHADER_DIR "/xxx.vert.spv"。
    static ShaderModule fromFile(VkDevice device, const std::string& path);

    ~ShaderModule();

    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;
    ShaderModule(const ShaderModule&)            = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    VkShaderModule handle() const { return m_module; }
    explicit operator bool() const { return m_module != VK_NULL_HANDLE; }

    // 拼一个 pipeline stage create info。entry 默认是 "main"。
    VkPipelineShaderStageCreateInfo stageInfo(VkShaderStageFlagBits stage,
                                              const char* entry = "main") const;

private:
    void reset() noexcept;

    VkDevice       m_device = VK_NULL_HANDLE;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

} // namespace rwb::rhi
