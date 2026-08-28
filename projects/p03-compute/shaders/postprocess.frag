#version 450
// TODO(p03-t05): 逐像素把 filtered 写入 consumed，证明 graphics stage 真消费结果。
layout(std430,set=0,binding=0) readonly buffer FilteredPixels { vec4 filteredPixels[]; };
layout(std430,set=0,binding=1) writeonly buffer ConsumedPixels { vec4 consumedPixels[]; };
layout(push_constant) uniform Push { uint width; uint height; } pc;
void main() { }
