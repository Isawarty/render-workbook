#version 450

// location 必须和 vertex shader 的 out 对上，光栅化会自动做重心插值。
layout(location = 0) in  vec3 fragColor;

// 这个 location 对应 render pass 里 subpass.pColorAttachments 的下标。
layout(location = 0) out vec4 outColor;

void main() {
    // swapchain 用的是 _SRGB 格式，硬件会在写入时替我们做 gamma 编码，
    // 所以这里输出的应当是线性空间的值。
    outColor = vec4(fragColor, 1.0);
}
