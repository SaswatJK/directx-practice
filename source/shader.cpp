#include "../include/shader.h"
#include "D3D12/d3dcommon.h"
#include <wrl/client.h>
#include <dxcapi.h>

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

void Shader::CompileShader(const std::string& vsPath, const std::string& psPath){
    std::string tempVS = readShader(vsPath);
    std::string tempPS = readShader(psPath);
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr;
    shaderBlob.emplace<RasterBlob>();
    RasterBlob& raster = std::get<RasterBlob>(shaderBlob);
    hr = D3DCompile(tempVS.c_str(), //source of the shader
                    tempVS.length(),
                    nullptr, //Use this parameter for strings that specify error messages, if not used then set to null.
                    nullptr, //Array of shader macros! YEP!!
                    nullptr, //Pointer to an id3dinclude for handling include fies. Set to NULL if no #include in the shader.
                    "VSMain", //main functin entry point for shader.
                    "vs_5_1", //Compiler target
                    D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES,
                    0,
                    &raster.vsBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Vertex Shader";
        if (errorBlob) {
            // Print the actual shader compilation error
            std::cerr << "Failed to copmile Vertex Shader:\n" 
                      << (char*)errorBlob->GetBufferPointer() << std::endl;
            errorBlob->Release();
        } else {
            std::cerr << "Shader compilation failed with HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
        }
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
                    &raster.psBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Pixel Shader";
        return;
    }
}


void Shader::CompileShader(const std::string& rtxPath) {
    Microsoft::WRL::ComPtr<IDxcUtils> utils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
    shaderBlob.emplace<Microsoft::WRL::ComPtr<IDxcBlob>>();
    Microsoft::WRL::ComPtr<IDxcBlob>& rtBlob = std::get<Microsoft::WRL::ComPtr<IDxcBlob>>(shaderBlob);
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if (FAILED(hr)) { std::cerr << "DxcUtils failed: 0x" << std::hex << hr << "\n"; return; }

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr)) { std::cerr << "DxcCompiler failed: 0x" << std::hex << hr << "\n"; return; }

    std::cerr << "DXC instances created OK\n";

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    hr = utils->LoadFile(std::wstring(rtxPath.begin(), rtxPath.end()).c_str(), nullptr, &sourceBlob);
    if (FAILED(hr) || !sourceBlob) { std::cerr << "LoadFile failed: " << rtxPath << "\n"; return; }

    std::cerr << "File loaded OK\n";

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_ACP;

    LPCWSTR args[] = { L"-T", L"lib_6_3", L"-HV", L"2021" };

    Microsoft::WRL::ComPtr<IDxcResult> result;
    hr = compiler->Compile(&sourceBuffer, args, _countof(args), nullptr, IID_PPV_ARGS(&result));
    if (FAILED(hr) || !result) { std::cerr << "Compile call failed\n"; return; }

    std::cerr << "Compile call OK\n";

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0)
        std::cerr << "Shader errors:\n" << errors->GetStringPointer() << "\n";

    Microsoft::WRL::ComPtr<IDxcBlob> compiledBlob;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledBlob), nullptr);
    if (FAILED(hr) || !compiledBlob) { std::cerr << "No output blob\n"; return; }

    std::cerr << "Compilation succeeded\n";
    rtBlob = compiledBlob;
}

SimpleShaderByteCode Shader::getShaderByteCode() const{
    SimpleShaderByteCode simpleByteCode;
    switch (shaderBlob.index()){
        case 0: {
            auto& raster = std::get<RasterBlob>(shaderBlob);
            simpleByteCode.ByteCode.VSPS.vsByteCode.pShaderBytecode = raster.vsBlob->GetBufferPointer();
            simpleByteCode.ByteCode.VSPS.vsByteCode.BytecodeLength= raster.vsBlob->GetBufferSize();
            simpleByteCode.ByteCode.VSPS.psByteCode.pShaderBytecode = raster.psBlob->GetBufferPointer();
            simpleByteCode.ByteCode.VSPS.psByteCode.BytecodeLength= raster.psBlob->GetBufferSize();
            break;
        }
        case 1: {
            auto& rsBlob = std::get<Microsoft::WRL::ComPtr<IDxcBlob>>(shaderBlob);
            simpleByteCode.ByteCode.rtxByteCode.pShaderBytecode = rsBlob->GetBufferPointer();
            simpleByteCode.ByteCode.rtxByteCode.BytecodeLength = rsBlob->GetBufferSize();
            simpleByteCode.ByteCode.VSPS.psByteCode.pShaderBytecode = nullptr;
            break;
        }
    }
    return simpleByteCode;
}
