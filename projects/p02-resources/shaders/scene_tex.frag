#version 450
// combined image sampler: 一个 descriptor 同时携带 image view 和 sampler。
// Vulkan 也允许把两者拆开（SAMPLED_IMAGE + SAMPLER），组合式更省事，
// 拆开式在「同一张图用多种采样方式」时更省显存。t05 的任务书会讲。
layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) in  vec3 fragColor;
layout(location = 1) in  vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragUv) * vec4(fragColor, 1.0);
}
