[[vk::binding(0, 0)]] RWStructuredBuffer<float4> ibl;

[numthreads(32, 1, 1)]
[shader("compute")]
void main(uint3 threadId : SV_DispatchThreadID) {
    uint i = threadId.x;
    if (i >= 22) return;
    float3 environment = float3(0.45);
    if (i == 0) {
        ibl[i] = float4(environment, 1.0);
    } else if (i <= 5) {
        float roughness = float(i - 1) / 4.0;
        ibl[i] = float4(environment * (1.0 - 0.32 * roughness), roughness);
    } else {
        uint lutIndex = i - 6;
        float nv = (float(lutIndex % 4) + 0.5) / 4.0;
        float roughness = (float(lutIndex / 4) + 0.5) / 4.0;
        float scale = nv * (1.0 - 0.45 * roughness);
        float bias = (1.0 - nv) * (0.04 + 0.2 * roughness);
        ibl[i] = float4(scale, bias, nv, roughness);
    }
}
