#version 450
layout(set = 0, binding = 0) uniform sampler2D hdrImage;
layout(set = 0, binding = 1) uniform sampler2D bloomImage;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;
vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec3 hdr = texture(hdrImage, inUv).rgb + texture(bloomImage, inUv).rgb * 0.75;
    outColor = vec4(pow(aces(hdr), vec3(1.0 / 2.2)), 1.0);
}
