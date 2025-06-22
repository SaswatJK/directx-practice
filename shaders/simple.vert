        //Remember how in glsl we use structs or even variables that we want to share from one shader to another, like vs to fs through an 'out' attributed variable or struct? We don't need to do that.
struct PSInput {
    float4 position : SV_POSITION; //System-value semantic for clip-space position
};

PSInput VSMain(float3 position : POSITION ) { //Can name this whatever we want, however we need to set the entrypoint name whle compiling the shader.
    PSInput result;
    result.position = float4(position, 1.0f);
    return result;
}
