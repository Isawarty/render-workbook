#version 450
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inTangent;
layout(location = 2) in vec2 inUv;
layout(location = 3) in float inMaterialKind;
layout(set = 0, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 0, binding = 1) uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 2) uniform sampler2D normalTexture;
layout(set = 0, binding = 3) uniform sampler2D occlusionTexture;
layout(location = 0) out vec4 outAlbedoMetallic;
layout(location = 1) out vec4 outNormalRoughness;
layout(location = 2) out vec4 outEmissiveAo;
void main() {
    if (inMaterialKind > 0.5) {
        outAlbedoMetallic = vec4(0.18, 0.22, 0.24, 0.0);
        outNormalRoughness = vec4(normalize(inNormal), 0.82);
        outEmissiveAo = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 n = normalize(inNormal);
    vec3 t = normalize(inTangent.xyz - n * dot(n, inTangent.xyz));
    vec3 b = cross(n, t) * inTangent.w;
    vec3 tangentNormal = texture(normalTexture, inUv).xyz * 2.0 - 1.0;
    vec3 worldNormal = normalize(mat3(t, b, n) * tangentNormal);
    vec3 baseColor = texture(baseColorTexture, inUv).rgb;
    vec3 mr = texture(metallicRoughnessTexture, inUv).rgb;
    float ao = texture(occlusionTexture, inUv).r;
    outAlbedoMetallic = vec4(baseColor, mr.b);
    outNormalRoughness = vec4(worldNormal, clamp(mr.g, 0.04, 1.0));
    // SciFiHelmet has no emissive material. Keep this MRT channel at zero
    // instead of manufacturing screen-space glow from its UVs/base color.
    outEmissiveAo = vec4(0.0, 0.0, 0.0, ao);
}
