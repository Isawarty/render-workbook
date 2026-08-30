#version 450
layout(location = 0) in vec3 inPosition;
layout(push_constant) uniform ShadowPush { mat4 mvp; } pushData;
void main() {
    gl_Position = pushData.mvp * vec4(inPosition, 1.0);
}
