#version 450
// t02 用。相机矩阵来自 uniform buffer，每帧更新一次。
// 注意 set/binding 必须和 C++ 侧的 VkDescriptorSetLayoutBinding 完全对上。

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} cam;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = cam.proj * cam.view * vec4(inPos, 1.0);
    fragColor   = inColor;
}
