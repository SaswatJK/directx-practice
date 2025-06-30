#pragma once
#include "D3D12/d3d12.h"
#include "utils.h"
#include <string>
#include <iostream>
#include <fstream>

typedef struct {
    D3D12_SHADER_BYTECODE vsByteCode;
    D3D12_SHADER_BYTECODE psByteCode;
}SimpleShaderByteCode;

class Shader {
private:
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    static std::string readShader(const std::string& shaderPath);
public:
    Shader(const std::string& vsPath, const std::string& psPath);//AM PASSING REFERENCES CHECK AGAIN
    SimpleShaderByteCode getShaderByteCode();
};
