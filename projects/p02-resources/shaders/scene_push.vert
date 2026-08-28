#version 450
// t03 用。相机还在 UBO 里（每帧一次），但每个物体的 model 矩阵改走 push constant
// （每次 draw call 一次）。这正是 t03 要你对比的两条路径。

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
} obj;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = cam.proj * cam.view * obj.model * vec4(inPos, 1.0);
    fragColor   = inColor;
}
