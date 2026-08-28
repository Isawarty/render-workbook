#version 450

layout(std430, set=0, binding=0) readonly buffer Positions { vec4 positions[]; };
layout(std430, set=0, binding=1) writeonly buffer Consumed { vec4 consumed[]; };

void main() {
    uint i = uint(gl_VertexIndex);
    consumed[i] = positions[i];
    gl_Position = positions[i];
    gl_PointSize = 1.0;
}
