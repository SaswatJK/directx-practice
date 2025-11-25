struct PSInput {
    float4 position : SV_POSITION; //System-value semantic for clip-space position
    float2 texcoord : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float4 normal : NORMAL) { //Can name this whatever we want, however we need to set the entrypoint name whle compiling the shader.
    PSInput result;
    result.position = position;
    result.texcoord = position.xy * 0.5f + 0.5f;
    result.texcoord.y = 1.0f - result.texcoord.y; //Forgot that uv is flipped in directx.
    return result;
}
