#include "../include/shader.h"
#include "D3D12/d3dcommon.h"
#include <winerror.h>

std::string Shader::readShader(const std::string& shaderPath){
    std::ifstream file(shaderPath);
    if (!file.is_open()) {
        std::cout << "Shader file " << shaderPath << " cannot be opened!" << std::endl;
    }
    return std::string (
        std::istreambuf_iterator<char>(file),  //points to start of string buffer
        std::istreambuf_iterator<char>()	   //points to end of string buffer
    );
}

Shader::Shader(const std::string& vsPath, const std::string& psPath){
    std::string tempVS = readShader(vsPath);
    std::string tempPS = readShader(psPath);
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr;
    hr = D3DCompile(tempVS.c_str(), //source of the shader
                    tempVS.length(),
                    nullptr, //Use this parameter for strings that specify error messages, if not used then set to null.
                    nullptr, //Array of shader macros! YEP!!
                    nullptr, //Pointer to an id3dinclude for handling include fies. Set to NULL if no #include in the shader.
                    "VSMain", //main functin entry point for shader.
                    "vs_5_1", //Compiler target
                    D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES,
                    0,
                    &vsBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Vertex Shader";
        return;
    }
    hr = D3DCompile(tempPS.c_str(), //source of the shader
                    tempPS.length(),
                    nullptr, //Use this parameter for strings that specify error messages, if not used then set to null.
                    nullptr, //Array of shader macros! YEP!!
                    nullptr, //Pointer to an id3dinclude for handling include fies. Set to NULL if no #include in the shader.
                    "PSMain", //main functin entry point for shader.
                    "ps_5_1", //Compiler target
                    D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES,
                    0,
                    &psBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Pixel Shader";
        return;
    }
}

SimpleShaderByteCode Shader::getShaderByteCode() const{
    SimpleShaderByteCode simpleByteCode;
    simpleByteCode.vsByteCode.pShaderBytecode = vsBlob->GetBufferPointer();
    simpleByteCode.vsByteCode.BytecodeLength= vsBlob->GetBufferSize();
    simpleByteCode.psByteCode.pShaderBytecode = psBlob->GetBufferPointer();
    simpleByteCode.psByteCode.BytecodeLength= psBlob->GetBufferSize();
    return simpleByteCode;
}
