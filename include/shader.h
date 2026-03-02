#pragma once
#include "D3D12/d3d12.h"
#include "utils.h"
#include <dxcapi.h>
#include <string>
#include <iostream>
#include <fstream>
#include <variant>

typedef struct {
    union BC{
        struct{
            D3D12_SHADER_BYTECODE vsByteCode;
            D3D12_SHADER_BYTECODE psByteCode;
        }VSPS;
        D3D12_SHADER_BYTECODE rtxByteCode;
    }ByteCode;
}SimpleShaderByteCode;

// I'd prefer Unions but Since ComPtrs have constructors, can't use unions but have to do with Variants.
class Shader {
private:
    struct RasterBlob{
        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    };
    std::variant<RasterBlob, Microsoft::WRL::ComPtr<IDxcBlob>> shaderBlob;
    static std::string readShader(const std::string& shaderPath);
public:
    void CompileShader(const std::string& vsPath, const std::string& psPath);//AM PASSING REFERENCES CHECK AGAIN
    void CompileShader(const std::string& rtxPath);
    SimpleShaderByteCode getShaderByteCode() const;
};
