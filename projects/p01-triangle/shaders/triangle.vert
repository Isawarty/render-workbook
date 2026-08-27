#version 450

// P1 没有 vertex buffer：顶点直接硬编码在 shader 里，用 gl_VertexIndex 索引。
// 这样能把「画出第一个三角形」所需的概念数量压到最少 —— 顶点缓冲、
// 内存分配、staging 上传全部留到 P2。
//
// 注意 Vulkan 的 NDC 与 OpenGL 不同：Y 轴向下，Z 范围是 [0,1] 而非 [-1,1]。
// 这是从 OpenGL 转过来最容易踩的坑之一。

vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // 顶部（Y 向下，所以负值在上）
    vec2( 0.5,  0.5),   // 右下
    vec2(-0.5,  0.5)    // 左下
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor   = colors[gl_VertexIndex];
}
