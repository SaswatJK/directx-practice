struct PSInput {
    float4 position : SV_POSITION; //System-value semantic for clip-space position
    float2 texcoord : TEXCOORD0;
};

PSInput VSMain(float3 position : POSITION ) { //Can name this whatever we want, however we need to set the entrypoint name whle compiling the shader.
    PSInput result;
    result.position = float4(position, 1.0f);
    result.texcoord = position.xy * 0.5f + 0.5f;
    result.texcoord.y = 1.0f - result.texcoord.y; //Forgot that uv is flipped in directx.
    return result;
}
