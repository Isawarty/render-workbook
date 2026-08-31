[[vk::binding(0, 0)]] RWStructuredBuffer<float> outputBuffer : register(u0);
[[vk::binding(1, 0)]] StructuredBuffer<float> inputX : register(t0);
[[vk::binding(2, 0)]] StructuredBuffer<float> inputY : register(t1);

struct Params {
    float a;
    uint count;
};
[[vk::binding(3, 0)]] ConstantBuffer<Params> params : register(b0);

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    const uint index = dispatchThreadId.x;
    if (index < params.count) {
        outputBuffer[index] = params.a * inputX[index] + inputY[index];
    }
}
