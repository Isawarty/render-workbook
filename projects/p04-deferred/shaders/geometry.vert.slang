struct GeometryPush { float4x4 mvp; float4x4 model; };
[[vk::push_constant]] ConstantBuffer<GeometryPush> pushData;
struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float4 tangent : TANGENT;
    [[vk::location(3)]] float2 uv : TEXCOORD0;
    [[vk::location(4)]] float materialKind : TEXCOORD1;
};
struct VertexOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float4 tangent : TANGENT;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float materialKind : TEXCOORD1;
};
[shader("vertex")]
VertexOutput main(VertexInput input) {
    VertexOutput output;
    float3x3 normalMatrix = (float3x3)pushData.model;
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.tangent = float4(normalize(mul(normalMatrix, input.tangent.xyz)), input.tangent.w);
    output.uv = input.uv;
    output.materialKind = input.materialKind;
    output.position = mul(pushData.mvp, float4(input.position, 1.0));
    return output;
}
