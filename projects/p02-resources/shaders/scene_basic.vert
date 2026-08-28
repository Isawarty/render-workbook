#version 450
// t01 用。没有 UBO、没有 push constant、没有纹理 ——
// 顶点坐标就是 NDC，直接透传。目的是先把「顶点数据来自 GPU 内存」这件事跑通。

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPos, 1.0);
    fragColor   = inColor;
}
