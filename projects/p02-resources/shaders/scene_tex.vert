#version 450
// t05 起用。在 push 版本上多传一个 uv 给片元着色器。

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
layout(location = 1) out vec2 fragUv;

void main() {
    gl_Position = cam.proj * cam.view * obj.model * vec4(inPos, 1.0);
    fragColor   = inColor;
    fragUv      = inUv;
}
