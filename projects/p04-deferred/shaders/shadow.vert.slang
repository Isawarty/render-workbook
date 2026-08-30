struct ShadowPush { float4x4 mvp; };
[[vk::push_constant]] ConstantBuffer<ShadowPush> pushData;
struct VertexInput { [[vk::location(0)]] float3 position : POSITION; };
[shader("vertex")]
float4 main(VertexInput input) : SV_Position {
    return mul(pushData.mvp, float4(input.position, 1.0));
}
