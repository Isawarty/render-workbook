#version 450

// 这一步故意没有 color attachment：fragment shader 把 compute 结果逐像素复制到
// 另一个 storage buffer。这样严格 L2 能证明 graphics stage 真的消费了 compute 输出。
layout(std430, set = 0, binding = 0) readonly buffer FilteredPixels { vec4 filteredPixels[]; };
layout(std430, set = 0, binding = 1) writeonly buffer ConsumedPixels { vec4 consumedPixels[]; };
layout(push_constant) uniform Push { uint width; uint height; } pc;

void main() {
    uvec2 pos = uvec2(gl_FragCoord.xy);
    if (pos.x >= pc.width || pos.y >= pc.height) return;
    uint i = pos.y * pc.width + pos.x;
    consumedPixels[i] = filteredPixels[i];
}
