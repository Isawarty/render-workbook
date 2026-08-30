[[vk::binding(0, 0)]] [vk::image_format("rgba16f")]
RWTexture2D<float4> hdrImage;
[[vk::binding(1, 0)]] [vk::image_format("rgba16f")]
RWTexture2D<float4> bloomImage;
struct BloomPush { uint2 size; };
[[vk::push_constant]] ConstantBuffer<BloomPush> pushData;

float gaussianWeight(int offset) {
    int distance = abs(offset);
    return distance == 0 ? 0.227027
                         : (distance == 1 ? 0.1945946 : 0.1216216);
}

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 threadId : SV_DispatchThreadID) {
    uint width = pushData.size.x;
    uint height = pushData.size.y;
    int2 pixel = int2(threadId.xy);
    if (pixel.x >= int(width) || pixel.y >= int(height)) return;
    float3 sum = float3(0.0);
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            int2 p = clamp(pixel + int2(x, y), int2(0),
                           int2(int(width) - 1, int(height) - 1));
            float3 c = hdrImage[p].rgb;
            float luminance = dot(c, float3(0.2126, 0.7152, 0.0722));
            c *= smoothstep(0.35, 0.9, luminance);
            sum += c * gaussianWeight(x) * gaussianWeight(y);
        }
    }
    bloomImage[pixel] = float4(sum, 1.0);
}
