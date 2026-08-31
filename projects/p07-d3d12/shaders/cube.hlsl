cbuffer Transform : register(b0) {
    float angle;
};

Texture2D checkerTexture : register(t0);
SamplerState checkerSampler : register(s0);

struct VertexInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput vsMain(VertexInput input) {
    const float c = cos(angle);
    const float s = sin(angle);
    float3 rotated = float3(
        c * input.position.x + s * input.position.z,
        input.position.y,
        -s * input.position.x + c * input.position.z);
    rotated.z += 3.0;

    VertexOutput output;
    output.position = float4(rotated.x, rotated.y, rotated.z - 0.1, rotated.z);
    output.uv = input.uv;
    return output;
}

float4 psMain(VertexOutput input) : SV_Target0 {
    return checkerTexture.Sample(checkerSampler, input.uv);
}
