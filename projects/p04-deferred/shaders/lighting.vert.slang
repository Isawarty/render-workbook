struct VertexOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(uint vertexId : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)
    };
    VertexOutput output;
    float2 p = positions[vertexId];
    output.uv = p * 0.5 + 0.5;
    output.position = float4(p, 0.0, 1.0);
    return output;
}
