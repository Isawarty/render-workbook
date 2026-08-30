#version 450
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput gAlbedoMetallic;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput gNormalRoughness;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput gEmissiveAo;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput gDepth;
layout(std430, set = 0, binding = 4) readonly buffer IblData { vec4 values[]; } ibl;
layout(set = 0, binding = 5) uniform sampler2DShadow shadowMap;
layout(std140, set = 0, binding = 6) uniform LightingFrame {
    mat4 inverseViewProjection;
    mat4 lightViewProjection;
    vec4 cameraPosition;
    vec4 lightDirection;
} frameData;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;
const float PI = 3.14159265359;
float distributionGgx(vec3 n, vec3 h, float r) {
    float a2 = r * r; a2 *= a2;
    float nh = max(dot(n, h), 0.0);
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}
float geometrySchlick(float nv, float r) {
    float k = (r + 1.0); k = k * k / 8.0;
    return nv / max(nv * (1.0 - k) + k, 0.0001);
}
vec3 fresnelSchlick(float hv, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - hv, 5.0);
}
vec3 skyColor(vec2 uv) {
    vec3 horizon = vec3(0.28, 0.36, 0.46);
    vec3 zenith = vec3(0.035, 0.075, 0.14);
    vec3 ground = vec3(0.025, 0.03, 0.035);
    return uv.y < 0.58 ? mix(zenith, horizon, smoothstep(0.0, 0.58, uv.y))
                       : mix(horizon, ground, smoothstep(0.58, 1.0, uv.y));
}
vec3 worldPosition(vec2 uv, float depth) {
    vec4 world = frameData.inverseViewProjection * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return world.xyz / world.w;
}
float pcfShadow(vec3 world) {
    vec4 lightClip = frameData.lightViewProjection * vec4(world, 1.0);
    if (lightClip.w <= 0.0) return 1.0;
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 uv = lightNdc.xy * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 ||
        lightNdc.z <= 0.0 || lightNdc.z >= 1.0) return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            visibility += texture(shadowMap,
                vec3(uv + vec2(x, y) * texel, lightNdc.z - 0.0025));
    return visibility / 9.0;
}
void main() {
    vec4 am = subpassLoad(gAlbedoMetallic);
    vec4 nr = subpassLoad(gNormalRoughness);
    vec4 ea = subpassLoad(gEmissiveAo);
    float depth = subpassLoad(gDepth).r;
    if (depth >= 0.999) { outColor = vec4(skyColor(inUv), 1.0); return; }
    vec3 world = worldPosition(inUv, depth);
    vec3 n = normalize(nr.xyz);
    vec3 v = normalize(frameData.cameraPosition.xyz - world);
    vec3 l = normalize(frameData.lightDirection.xyz), h = normalize(v + l);
    float nv = max(dot(n, v), 0.0), nl = max(dot(n, l), 0.0);
    vec3 f0 = mix(vec3(0.04), am.rgb, am.a);
    vec3 f = fresnelSchlick(max(dot(h, v), 0.0), f0);
    float d = distributionGgx(n, h, max(nr.a, 0.04));
    float g = geometrySchlick(nv, nr.a) * geometrySchlick(nl, nr.a);
    vec3 specular = d * g * f / max(4.0 * nv * nl, 0.0001);
    vec3 kd = (1.0 - f) * (1.0 - am.a);
    // Neutral, high-intensity key light: genuine Cook-Torrance highlights can
    // cross the bloom threshold without tinting the whole material.
    vec3 direct = (kd * am.rgb / PI + specular) * vec3(20.0) * nl;
    float visibility = pcfShadow(world);
    direct *= visibility;
    uint prefilterIndex = 1u + uint(clamp(nr.a * 4.0, 0.0, 4.0));
    uint lutX = uint(clamp(nv * 4.0, 0.0, 3.0));
    uint lutY = uint(clamp(nr.a * 4.0, 0.0, 3.0));
    vec2 brdf = ibl.values[6u + lutY * 4u + lutX].rg;
    vec3 ambient = ibl.values[0].rgb * am.rgb * (1.0 - am.a);
    ambient += ibl.values[prefilterIndex].rgb * (f0 * brdf.x + brdf.y);
    outColor = vec4(direct + ambient * ea.a + ea.rgb, visibility);
}
