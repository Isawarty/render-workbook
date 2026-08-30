[[vk::binding(0, 0)]] Sampler2D baseColorTexture;
[[vk::binding(1, 0)]] Sampler2D metallicRoughnessTexture;
[[vk::binding(2, 0)]] Sampler2D normalTexture;
[[vk::binding(3, 0)]] Sampler2D occlusionTexture;

struct FragmentInput {
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float4 tangent : TANGENT;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float materialKind : TEXCOORD1;
};

struct FragmentOutput {
    float4 albedoMetallic : SV_Target0;
    float4 normalRoughness : SV_Target1;
    float4 emissiveAo : SV_Target2;
};

[shader("fragment")]
FragmentOutput main(FragmentInput input) {
    if (input.materialKind > 0.5) {
        FragmentOutput ground;
        ground.albedoMetallic = float4(0.18, 0.22, 0.24, 0.0);
        ground.normalRoughness = float4(normalize(input.normal), 0.82);
        ground.emissiveAo = float4(0.0, 0.0, 0.0, 1.0);
        return ground;
    }
    float3 n = normalize(input.normal);
    float3 t = normalize(input.tangent.xyz - n * dot(n, input.tangent.xyz));
    float3 b = cross(n, t) * input.tangent.w;
    float3 tangentNormal = normalTexture.Sample(input.uv).xyz * 2.0 - 1.0;
    float3 worldNormal = normalize(mul(float3x3(t, b, n), tangentNormal));
    float3 baseColor = baseColorTexture.Sample(input.uv).rgb;
    float3 mr = metallicRoughnessTexture.Sample(input.uv).rgb;
    float ao = occlusionTexture.Sample(input.uv).r;
    FragmentOutput output;
    output.albedoMetallic = float4(baseColor, mr.b);
    output.normalRoughness = float4(worldNormal, clamp(mr.g, 0.04, 1.0));
    // SciFiHelmet has no emissive material. Keep this MRT channel at zero
    // instead of manufacturing screen-space glow from its UVs/base color.
    output.emissiveAo = float4(0.0, 0.0, 0.0, ao);
    return output;
}
