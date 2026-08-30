[[vk::binding(0, 0)]] [[vk::input_attachment_index(0)]] SubpassInput<float4> gAlbedoMetallic;
[[vk::binding(1, 0)]] [[vk::input_attachment_index(1)]] SubpassInput<float4> gNormalRoughness;
[[vk::binding(2, 0)]] [[vk::input_attachment_index(2)]] SubpassInput<float4> gEmissiveAo;
[[vk::binding(3, 0)]] [[vk::input_attachment_index(3)]] SubpassInput<float4> gDepth;
[[vk::binding(4, 0)]] StructuredBuffer<float4> ibl;
[[vk::binding(5, 0)]] Sampler2DShadow shadowMap;

struct LightingFrame {
    float4x4 inverseViewProjection;
    float4x4 lightViewProjection;
    float4 cameraPosition;
    float4 lightDirection;
};
[[vk::binding(6, 0)]] ConstantBuffer<LightingFrame> frameData;

static const float PI = 3.14159265359;

float distributionGgx(float3 n, float3 h, float r) {
    float a2 = r * r;
    a2 *= a2;
    float nh = max(dot(n, h), 0.0);
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

float geometrySchlick(float nv, float r) {
    float k = r + 1.0;
    k = k * k / 8.0;
    return nv / max(nv * (1.0 - k) + k, 0.0001);
}

float3 fresnelSchlick(float hv, float3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - hv, 5.0);
}

float3 skyColor(float2 uv) {
    float3 horizon = float3(0.28, 0.36, 0.46);
    float3 zenith = float3(0.035, 0.075, 0.14);
    float3 ground = float3(0.025, 0.03, 0.035);
    return uv.y < 0.58 ? lerp(zenith, horizon, smoothstep(0.0, 0.58, uv.y))
                       : lerp(horizon, ground, smoothstep(0.58, 1.0, uv.y));
}

float3 worldPosition(float2 uv, float depth) {
    float4 world = mul(frameData.inverseViewProjection,
                       float4(uv * 2.0 - 1.0, depth, 1.0));
    return world.xyz / world.w;
}

float pcfShadow(float3 world) {
    float4 lightClip = mul(frameData.lightViewProjection, float4(world, 1.0));
    if (lightClip.w <= 0.0) return 1.0;
    float3 lightNdc = lightClip.xyz / lightClip.w;
    float2 uv = lightNdc.xy * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 ||
        lightNdc.z <= 0.0 || lightNdc.z >= 1.0) return 1.0;
    uint width, height;
    shadowMap.GetDimensions(width, height);
    float2 texel = 1.0 / float2(width, height);
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            visibility += shadowMap.SampleCmp(
                uv + float2(x, y) * texel, lightNdc.z - 0.0025);
        }
    }
    return visibility / 9.0;
}

struct FragmentInput {
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 main(FragmentInput input) : SV_Target0 {
    float4 am = gAlbedoMetallic.SubpassLoad();
    float4 nr = gNormalRoughness.SubpassLoad();
    float4 ea = gEmissiveAo.SubpassLoad();
    float depth = gDepth.SubpassLoad().r;
    if (depth >= 0.999) return float4(skyColor(input.uv), 1.0);
    float3 world = worldPosition(input.uv, depth);
    float3 n = normalize(nr.xyz);
    float3 v = normalize(frameData.cameraPosition.xyz - world);
    float3 l = normalize(frameData.lightDirection.xyz);
    float3 h = normalize(v + l);
    float nv = max(dot(n, v), 0.0);
    float nl = max(dot(n, l), 0.0);
    float3 f0 = lerp(float3(0.04), am.rgb, am.a);
    float3 f = fresnelSchlick(max(dot(h, v), 0.0), f0);
    float d = distributionGgx(n, h, max(nr.a, 0.04));
    float g = geometrySchlick(nv, nr.a) * geometrySchlick(nl, nr.a);
    float3 specular = d * g * f / max(4.0 * nv * nl, 0.0001);
    float3 kd = (1.0 - f) * (1.0 - am.a);
    // Neutral, high-intensity key light: genuine Cook-Torrance highlights can
    // cross the bloom threshold without tinting the whole material.
    float3 direct = (kd * am.rgb / PI + specular) * float3(20.0) * nl;
    float visibility = pcfShadow(world);
    direct *= visibility;
    uint prefilterIndex = 1 + uint(clamp(nr.a * 4.0, 0.0, 4.0));
    uint lutX = uint(clamp(nv * 4.0, 0.0, 3.0));
    uint lutY = uint(clamp(nr.a * 4.0, 0.0, 3.0));
    float2 brdf = ibl[6 + lutY * 4 + lutX].rg;
    float3 ambient = ibl[0].rgb * am.rgb * (1.0 - am.a);
    ambient += ibl[prefilterIndex].rgb * (f0 * brdf.x + brdf.y);
    return float4(direct + ambient * ea.a + ea.rgb, visibility);
}
