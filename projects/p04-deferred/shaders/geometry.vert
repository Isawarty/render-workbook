#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUv;
layout(location = 4) in float inMaterialKind;
layout(push_constant) uniform GeometryPush { mat4 mvp; mat4 model; } pushData;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outTangent;
layout(location = 2) out vec2 outUv;
layout(location = 3) out float outMaterialKind;
void main() {
    mat3 normalMatrix = mat3(pushData.model);
    outNormal = normalize(normalMatrix * inNormal);
    outTangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);
    outUv = inUv;
    outMaterialKind = inMaterialKind;
    gl_Position = pushData.mvp * vec4(inPosition, 1.0);
}
