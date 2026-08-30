[[vk::binding(0, 0)]] Sampler2D hdrImage;
[[vk::binding(1, 0)]] Sampler2D bloomImage;

float acesChannel(float x) {
    return clamp((x * (2.51 * x + 0.03)) /
                 (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

struct FragmentInput {
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 main(FragmentInput input) : SV_Target0 {
    float3 hdr = hdrImage.Sample(input.uv).rgb +
                 bloomImage.Sample(input.uv).rgb * 0.75;
    float3 mapped = float3(acesChannel(hdr.r), acesChannel(hdr.g),
                           acesChannel(hdr.b));
    return float4(pow(mapped.r, 1.0 / 2.2),
                  pow(mapped.g, 1.0 / 2.2),
                  pow(mapped.b, 1.0 / 2.2), 1.0);
}
