//Remember how in glsl we use structs or even variables that we want to share from one shader to another, like vs to fs through an 'out' attributed variable or struct? We don't need to do that.

struct MyConstants{
    float4 color;
    float4 metallic;
    float4 viewMatR0;    // View matrix row 0
    float4 viewMatR1;    // View matrix row 1
    float4 viewMatR2;    // View matrix row 2
    float4 viewMatR3;    // View matrix row 3
    float4 projMatR0;    // Projection matrix row 0
    float4 projMatR1;    // Projection matrix row 1
    float4 projMatR2;    // Projection matrix row 2
    float4 projMatR3;    // Projection matrix row 3
    float4 modelMatR0;
    float4 modelMatR1;
    float4 modelMatR2;
    float4 modelMatR3;
};

ConstantBuffer <MyConstants> cBuffers[] : register(b0, space0);

struct PSInput {
    float4 position : SV_POSITION; //System-value semantic for clip-space position
    float4 normal : NORMAL0;
    float4 worldPos :POSITION1;
    float2 texcoord : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float4 normal : NORMAL) { //Can name this whatever we want, however we need to set the entrypoint name whle compiling the shader.
    PSInput result;
    float4x4 modelMatrix = float4x4(cBuffers[0].modelMatR0, cBuffers[0].modelMatR1, cBuffers[0].modelMatR2, cBuffers[0].modelMatR3);
    float4x4 viewMatrix = float4x4(cBuffers[0].viewMatR0, cBuffers[0].viewMatR1, cBuffers[0].viewMatR2, cBuffers[0].viewMatR3);
    float4x4 projMatrix = float4x4(cBuffers[0].projMatR0, cBuffers[0].projMatR1, cBuffers[0].projMatR2, cBuffers[0].projMatR3);
/*
	float4 worldPos = mul(modelMatrix, position);
	float4 viewPos = mul(viewMatrix, worldPos);
	float4 clipPos = mul(projMatrix, viewPos);
*/
	float4 worldPos = mul(position, modelMatrix);
	float4 viewPos = mul(worldPos, viewMatrix);
	float4 clipPos = mul(viewPos, projMatrix);

	result.position = clipPos;

    result.texcoord = position.xy * 0.5f + 0.5f;
    result.normal = normal;
    result.worldPos = worldPos;
    result.texcoord.y = 1.0f - result.texcoord.y; //Forgot that uv is flipped in directx.
    return result;
}
