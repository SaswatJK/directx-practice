#include <D3D12/D3dx12.h>
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgiformat.h>
#include <intsafe.h>
#include <iostream>
#include "D3D12/d3d12.h"
#include "D3D12/d3dcommon.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_properties.h"
#include "SDL3/SDL_video.h"
#include "glm/glm.hpp"
#include <ratio>
#include <wrl.h>
#include <wrl/client.h>

// WE CAN USE QueryInterface to convert 1s to 3s. Like SwapChain1 to SwapChain3 and others like it.

//Basic vertex struct for uniformity
struct Vertex{
  float position[3];
  float color[4];
};

Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

void PrintDebugMessages() {
    UINT64 messageCount = infoQueue->GetNumStoredMessages();

    for (UINT64 i = 0; i < messageCount; i++) {
        SIZE_T messageLength = 0;
        infoQueue->GetMessage(i, NULL, &messageLength);

        D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageLength);
        infoQueue->GetMessage(i, message, &messageLength);

        printf("D3D12 Message: %s\n", message->pDescription);
        free(message);
    }

    infoQueue->ClearStoredMessages();
}

int main(){
if (SDL_Init(SDL_INIT_VIDEO) == false) {
    //SDL_Log("SDL_Init failed: %s", SDL_GetError());
    std::cout<<SDL_GetError();
    SDL_Log("Joe amamamamammaamam");
    return 1;
}

// Inherits from all the IDXGIFactories before it. It implements methods for generating DXGI objects.
// Some tasks are graphics API independent, like swapchains and page flipping, so we use DXGI instead of D3D

Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
{
    debugController->EnableDebugLayer();
}

Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter; // A real phyislca device
Microsoft::WRL::ComPtr<ID3D12Device> device; // A device is basiaclly a way to represent a physical piece of adapter (like a GPU, be it discrete or integrated)

for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &hardwareAdapter); ++i) {
  if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)))) {
    break;
    }
  }
  if(!device)
  std::cout<<"The device failed to create for d3d12 \n";

    device->QueryInterface(IID_PPV_ARGS(&infoQueue));
//Checking to see what tier of raytracing my GPU supports
//D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
//device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
//std::cout<<"The tier is: "<<options5.RaytracingTier;

Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue; //Submit commands to the GPU, which are collected and then executed in bulk. We don't want the GPU to wait for the CPU or vice versa while commands are being collected.

D3D12_COMMAND_QUEUE_DESC queueDesc = {}; //Describing what kind of queue we want
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //Allows all operations, whether they are graphics, compute or copy. Despite every command type being allowed, only command lists with type DIRECT are allowed to be submitted.
//The rest of the properties of the struct is 0 for basic simpel command queue

hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

unsigned int width = 800;
unsigned int height = 800;

Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
swapChainDesc.BufferCount = 2; //Swapchain buffers 2 of them
swapChainDesc.Width = width; //Width of the swapchain, if 0, the runtime will obtain the width from the window itself.
swapChainDesc.Height = height; //height of the swapchain
swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //format of the colors for the swapchain
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //used as an output render target
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //Will discard the ontents of the backbuffer after we call the Present method
swapChainDesc.SampleDesc.Count = 1;

Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;

SDL_Window* window = SDL_CreateWindow("SDL3 + DirectX12", width, height, 0);

HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL); //HWND pointer for the window intialized by SDL

if(!hwnd){
  std::cerr<<"The hwnd from sdl is failed:"<<SDL_GetError();
}

//std::cout<<"The hwnd value is:"<<hwnd;
hr = factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain); //We are creating swapchain for that window.

if(FAILED(hr)){
if (hr == DXGI_ERROR_DEVICE_REMOVED) {
    std::cerr << "Device removed. Please check your GPU connection.";
} else if (hr == DXGI_ERROR_INVALID_CALL) {
    std::cerr << "Invalid call. Verify your parameters.";
}
  std::cerr<<"failed to create swap chain";
  return 1;
}

hr = tempSwapChain.As(&swapChain);

//A descriptor heap is a collection of contiguous allocations of descriptors, one allocation for every descriptor. Descriptor heaps contain many object types that are not part of a Pipeline State Object (PSO), such as Shader Resource Views (SRVs), Unordered Access Views (UAVs), Constant Buffer Views (CBVs), and Samplers.

//render texture descriptior heap
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap; //Remember to make a com object pointer for "easier" memory management for these interfaces
D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
rtvHeapDesc.NumDescriptors = 2; //Double buffering
rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //Just saying this is a render target view(view is the saem thing as descriptor)

hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));//We use the device to create stuff. Just like how we use a factory in dxgi to create stuff.

//Basically rtv Heap knows that there will be 2 descriptors, so it will probably allocate memory for that
//And then we can get the increment size of the rtv descriptor heaps for easy mathematics
UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[2]; //Double buffering

//Basically this now returns the CPU handle that represents the start of the heap.
D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

for(UINT i = 0; i < 2; i++){
  hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])); //Get one of the swap chain's back buffers
  device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle); //basically rtv handle is where the newly created render target view will reside
  rtvHandle.ptr += rtvDescriptorSize; //Put the render target and the descriptors and stuff at one 
  //rtvHandle.Offset(1, rtvDescriptorSize); //For the Cd3dx12 helper library
}

//Device can make command allocator and command list, we uplaod rsoure and do draw calls and finish wrapping up teh ommand list and execute it through command list whle the allocator just prepares the list.
//Command queues will execute command lists and signal fence

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)); //Since the queue is of type list_direct. We are basically allocating the memory required for a command list

if(FAILED(hr)){
  std::cerr<<"Command Alocator createion error";
}
//Everytime a commandlist is reset, it will allocate a block of memory from the commandallocator, it will pile up. We can reset the command allocator, but if we do so, then we shouldn't use teh command list and execute it with the previously allocated memory. Not thread safe obiously, have more than one commandallocator per thread.

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> commandList;
hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)); //Same as before, but we get the commandAllocator's memory through the getter and then we put the memory of the commandList com

if(FAILED(hr)){
  std::cerr<<"Command list createion error";
  return 1;
}

//Need to make a descriptor Range for CBV
D3D12_DESCRIPTOR_RANGE descriptorRange = {};
descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
descriptorRange.NumDescriptors = 1;
descriptorRange.BaseShaderRegister = 0;
descriptorRange.RegisterSpace = 0;
descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

D3D12_ROOT_PARAMETER rootParam = {};
rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
rootParam.DescriptorTable.NumDescriptorRanges = 1;
rootParam.DescriptorTable.pDescriptorRanges = &descriptorRange;

//This is for shaders. Like to be like "Hey the shader can take these resources and use it"
//The root signature defines what resources are bound to teh graphics pipeline. A root signature is configured by teh app and links command lists to the resources the shader require.
Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
//CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
//rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
//Root signatures basically define the binding relationship betwween shader resource and the graphics pieline.
// All the arguments in the helper method is basically the same thing as manually filling these descriptions themselves.
D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
rootSignatureDesc.NumParameters = 1;
rootSignatureDesc.pParameters = &rootParam;
rootSignatureDesc.NumStaticSamplers = 0;
rootSignatureDesc.pStaticSamplers = nullptr;
rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // The app is opting in to using the Input Assembler (requiring an input layout that defines a set of vertex buffer bindings). Omitting this flag can result in one root argument space being saved on some hardware. Omit this flag if the Input Assembler is not required, though the optimization is minor.

//Blob is basically a binary format
Microsoft::WRL::ComPtr<ID3DBlob> signature;
Microsoft::WRL::ComPtr<ID3DBlob> error;

//serializzing means converting root signature descrption structure into a binary blob format that the GPU driver can understand and proces.
hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

// Using our device adapter to create a root signature from the blob
hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
if (FAILED(hr)) {
std::cerr << "Failed to create root signature" << std::endl;
return 1;
}

//The POSITION and COLOR in the shaders are called semantics. You increase the semantic index only if multiple same named semantics exist

const char* vertexShaderSource = R"(
  struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
  };
        
  PSInput VSMain(float3 position : POSITION, float4 color : COLOR) {
    PSInput result;
    result.position = float4(position, 1.0f);
    result.color = color;
    return result;
    }
)";
    
const char* pixelShaderSource = R"(
    cbuffer Vec4Buffer : register (b0){
        float4 myVec4;
        float4 myVec5;
    };

  struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
  };
        
  float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
    }
)";


Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

//It's crazy that direct3d provides macros and includes and entrypoints customisation itself.
hr = D3DCompile(vertexShaderSource, strlen(vertexShaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &error);
if (FAILED(hr)) {
  std::cerr << "Vertex shader compilation failed!"<<std::endl;
  return 1;
}

hr = D3DCompile(pixelShaderSource, strlen(pixelShaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &error);
if (FAILED(hr)) {
  std::cerr << "Pixel shader compilation failed!"<<std::endl;
  return 1;
}

//Now we are creating the Pipeline
//Semantic name, semantic index, format of input, input slot , offset, input slot class, instance data steprate
D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
  {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
};

//pieplien state object
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };//input layout description is a struct that contains an  array of element descrption and the number of the array members
psoDesc.pRootSignature = rootSignature.Get();
//psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
//psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());

D3D12_SHADER_BYTECODE vsShaderByteCode = {};
vsShaderByteCode.pShaderBytecode = vertexShader->GetBufferPointer();
vsShaderByteCode.BytecodeLength = vertexShader->GetBufferSize();
D3D12_SHADER_BYTECODE psShaderByteCode = {};
psShaderByteCode.pShaderBytecode = pixelShader->GetBufferPointer();
psShaderByteCode.BytecodeLength = pixelShader->GetBufferSize();

psoDesc.VS = vsShaderByteCode;
psoDesc.PS = psShaderByteCode;

//psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
//psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

D3D12_RASTERIZER_DESC rasterizerDesc = {};
rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; //either wireframe or solid, so fill or no fill
rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; //Do not draw thriangeles that are back facing
rasterizerDesc.FrontCounterClockwise = FALSE; //Is front Counter clockwise or clockwise vertices
rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS; //Basically prevents shadow acne itself, so we don't need to do any crazy mathematics in our shader
rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
rasterizerDesc.DepthClipEnable = TRUE;
rasterizerDesc.MultisampleEnable = FALSE; //quadrilateral or aa on msaa. if quad, true
rasterizerDesc.AntialiasedLineEnable = TRUE; //if multisample enable is false, use this
rasterizerDesc.ForcedSampleCount = 0; //Sample count that is forced while UAV rendering. UAV is basically unordered access view, meaning the 'view' can be accesssed in any order, read write, rwrite read write, write read write read etc. If we are to do UAV stuff, like reading and writing to the same texture/buffer concurrently, we can set it over 0, this means the rasterizer acts as if multiple samples exist. So, the GPU processes multiple samples per pixel. It's like doing manual super sampling.
rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF; //Read the GPU Gems algorithm on conservative rasterization. It's absolutely beautiful. Basically making shit fatter for better inteserction tests.
psoDesc.RasterizerState = rasterizerDesc;

//Describes the way blending is done
D3D12_BLEND_DESC blendDesc = {};
blendDesc.AlphaToCoverageEnable = FALSE; // https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f and it's link to https://www.humus.name/index.php?page=3D&ID=61 explains it.
blendDesc.IndependentBlendEnable = FALSE; // If true, each render target can have it's own unique blend state. There's 8 remember that.
blendDesc.RenderTarget[0] = {}; //This is an array of render target blend description structures. Since I said indpendent blend is false, I only need to configure one.
blendDesc.RenderTarget[0].BlendEnable = FALSE; //No blending
blendDesc.RenderTarget[0].LogicOpEnable = FALSE; //Enable or disable a logical operation, can't be both blend able and this one true
blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;  //Specifies the operation to perform on the value that the pixel shader outputs, performs before blending with the render target value
blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO; //Specifies the opreation to perform on the current RGB value in the render target.
blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD; //Defines how to comibine the srcblend and destblend opreations
blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE; //specifies the operation to perform on the alpha value that hte pixelshader outputs.
blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO; //Same thing as srcblendalpha but the value in the rander target
blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD; //Defines how to comibine the srcblendalpha and destblendalpha
blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP; //Specifies the logical operation to configure for the render target
blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //combination of values that are combined by usign a bitwise or operation. The resulting value specifies a write maask.
psoDesc.BlendState = blendDesc;

psoDesc.DepthStencilState.DepthEnable = FALSE;
psoDesc.DepthStencilState.StencilEnable = FALSE;
psoDesc.SampleMask = UINT_MAX;
psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
psoDesc.NumRenderTargets = 1;
psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
psoDesc.SampleDesc.Count = 1;

// Represents the state of all currently set shaders as well as certain fixed function state objects.
Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
if (FAILED(hr)) {
  std::cerr << "Failed to create pipeline again!" << std::endl;
  return 1;
}

Vertex triangleVertices[] = {
	{ { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },    // Top (red)
        { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },   // Bottom right (green)
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }   // Bottom left (blue)
};

const UINT vertexBufferSize = sizeof(triangleVertices);

Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;

//CD3DX12_HEAP_PROPERTIES upHeapProps(D3D12_HEAP_TYPE_UPLOAD);

D3D12_HEAP_PROPERTIES upHeapProps = {};
upHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD; //Type upload is the best for CPU read once and GPU write once Data. This type has CPU access optimized for uploading to the GPU. Resouurce on this Heap must be created with GENERIC_READ
upHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; //I used WRITE_COMBINE enum but it gave me error, so I have to use PROPERTY_UNKOWN. The write comobine ignores the cache and writes to a buffer, it's better for streaming data to another device like the GPU, but very inefficient if we wanna read for some reason from the same buffer this is pointing to.
upHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; //I used MEMORY_POOL_L0 but it gave me error, so I have to use POOL_UNKOWN. L0 means the system memory (RAM), while L1 is the videocard memory (VRAM). If integrated graphics exists, this will be slower for the GPU but the CPU will be faster than the GPU. However it does still support APUs and Integrated Graphics, while if we use L1, it's only available for systems with their own graphics cards. It cannot be accessed by the CPU at all, so it's only GPU accessed.
upHeapProps.CreationNodeMask = 1;
upHeapProps.VisibleNodeMask = 1;

//So from this, we can see that we have to create multiple bufers. This kind of reminds me of CUDA programming and doing device and host memcpys and everything, very low level and explicit.
//Basically we will make an upload buffer and a default buffer where the GPU will do everything itself, and the CPU can't access, so unaviablabnel page property. Then we upload initial data to CPU then it will copy those things to the GPU and the GPU will solely work with it.

//CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
D3D12_RESOURCE_DESC bufferDesc = {};
DXGI_SAMPLE_DESC sampleDesc = {}; //Multi sampling parameters
sampleDesc.Count = 1;
sampleDesc.Quality = 0;

bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;//Resource is a buffer and not a texture
bufferDesc.Alignment = 0; //Default alignment
bufferDesc.Width = vertexBufferSize;
bufferDesc.Height = 1; //1D
bufferDesc.DepthOrArraySize = 1; //1D
bufferDesc.MipLevels = 1;
bufferDesc.Format = DXGI_FORMAT_UNKNOWN; //Because buffers are typeless
bufferDesc.SampleDesc = sampleDesc;
bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // Row Major is inefficient for extensive usage according to the document but buffers are created with D3D12_TEXTURE_LAYOUT_ROW_MAJOR, because row-major texture data can be located in them without creating a texture object. This is commonly used for uploading or reading back texture data, especially for discrete/NUMA adapters.
bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

hr = device->CreateCommittedResource(
        &upHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));
    if (FAILED(hr)) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return 1;
    }

float colorValue[4] = {1.0f, 0.0f, 0.0f, 1.0f};
float colorValue2[4] = {0.0f, 0.0f, 1.0f, 1.0f};

//MAking a Uniform buffer for a vector
D3D12_RESOURCE_DESC uBufferDesc = {};
DXGI_SAMPLE_DESC uSampleDesc = {}; //Multi sampling parameters
uSampleDesc.Count = 1;
uSampleDesc.Quality = 0;

uBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;//Resource is a buffer and not a texture
uBufferDesc.Alignment = 0; //Default alignment
uBufferDesc.Width = 256;
uBufferDesc.Height = 1; //1D
uBufferDesc.DepthOrArraySize = 1; //1D
uBufferDesc.MipLevels = 1;
uBufferDesc.Format = DXGI_FORMAT_UNKNOWN; //Because buffers are typeless
uBufferDesc.SampleDesc = uSampleDesc;
uBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // Row Major is inefficient for extensive usage according to the document but buffers are created with D3D12_TEXTURE_LAYOUT_ROW_MAJOR, because row-major texture data can be located in them without creating a texture object. This is commonly used for uploading or reading back texture data, especially for discrete/NUMA adapters.
uBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

Microsoft::WRL::ComPtr<ID3D12Resource> uniformBuffer;
hr = device->CreateCommittedResource(&upHeapProps, D3D12_HEAP_FLAG_NONE, &uBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uniformBuffer));

UINT8* mappedData = nullptr;
uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
memcpy(mappedData, &colorValue, 16);
memcpy(mappedData+16, &colorValue2, 16);
uniformBuffer->Unmap(0, nullptr);
//Descriptor heap for the CBV (constant buffer View)
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> uniformHeap;
D3D12_DESCRIPTOR_HEAP_DESC uHeapDesc = {};
uHeapDesc.NumDescriptors = 1;
uHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
uHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
hr = device->CreateDescriptorHeap(&uHeapDesc, IID_PPV_ARGS(&uniformHeap));
if(FAILED(hr)){
    std::cout<<"Descriptor Heap for CSV not made";
    return -1;
}

// Describe the CBV itself
D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
cbvDesc.BufferLocation = uniformBuffer->GetGPUVirtualAddress();
cbvDesc.SizeInBytes = 256;

device->CreateConstantBufferView(&cbvDesc, uniformHeap->GetCPUDescriptorHandleForHeapStart());

//Now need to bind to the root signature
// Copy vertices to the buffer
UINT8* pVertexDataBegin;
//CD3DX12_RANGE readRange(0, 0);
D3D12_RANGE readRange = {};
readRange.Begin = 0;
readRange.End = 0;
//This indicates the region the CPU might read, and the coordinates are subresource-relative. A null pointer indicates the entire subresource might be read by the CPU. It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin.

// Gets a CPU pointer to the specified subresource in the resource, but may not disclose the pointer value to applications. Map also invalidates the CPU cache, when necessary, so that CPU reads to this address reflect any modifications made by the GPU.
hr = vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));//We have described the resource vertexBuffer everything, it's size, where to store it, how to be used, but now we will allocate memory space and get pointers ready and everything
if (FAILED(hr)) {
std::cerr << "Failed to map vertex buffer" << std::endl;
return 1;
}
memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices)); //We copied the vertex data to the mapped area finanlly.
vertexBuffer->Unmap(0, nullptr);

//This is like doign the VAO thing in OpenGL
D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();//geting the gpu's pointer to the vertex buffer
vertexBufferView.StrideInBytes = sizeof(Vertex);
vertexBufferView.SizeInBytes = vertexBufferSize;

// Create synchronization Objects
//In order to await completion of command buffers in the command queue, we will need a synchronization structure known as a fence. A fence has an internal integer counter, and can trigger events on either the CPU or the GPU when the counter reaches a certain value. This can be done to trigger an event on the CPU when a command buffer is done executing, have the GPU wait while the CPU is completing a certain operation first, or to get two command queues on the GPU to wait upon one another.
Microsoft::WRL::ComPtr<ID3D12Fence> fence;

hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr)) {
    std::cerr <<"Failed to create fence" << std::endl;
    return 1;
}

HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
if (fenceEvent == nullptr) {
  std::cerr << "Failed to create fence event" <<  std::endl;
  return 1;
}

// Main loop
UINT frameIndex = 0;
UINT64 fenceValue = 1;

commandList->Close(); //We close right now, or we could've closed as soon as we created the list because every command list created by d3d12 is creaetd in an open state.

bool running = true;
SDL_Event event;

if (!window) {
SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
return 1;
}

while (running) {
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      running = false;
      break;
    }
  }

//Uniforms are called constant buffers
hr = commandAllocator->Reset(); //Resetting the allocator every time
        if (FAILED(hr)) {
            std::cerr << "Failed to reset command allocator" << std::endl;
            break;
        }
        
        hr = commandList->Reset(commandAllocator.Get(), pipelineState.Get()); //Resting the list everytime before using a non reset list with a reset allocator (not a good idea)
        if (FAILED(hr)) {
            std::cerr << "Failed to reset command list" << std::endl;
            break;
        }
        
        // Set necessary state
    commandList->SetGraphicsRootSignature(rootSignature.Get()); //Making stuff ready for the shaders to understand
	ID3D12DescriptorHeap* dHeaps[] = { uniformHeap.Get() };
    commandList->SetDescriptorHeaps(1, dHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, uniformHeap->GetGPUDescriptorHandleForHeapStart());
    //As can be seen, very easy to change the root signature for shaders
	//CD3DX12_VIEWPORT viewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	D3D12_VIEWPORT viewPort = {};
	viewPort.TopLeftX = 0.0f;
	viewPort.TopLeftY = 0.0f;
	viewPort.Width = static_cast<float>(width);
	viewPort.Height = static_cast<float>(height);
	viewPort.MinDepth = D3D12_MIN_DEPTH;
	viewPort.MaxDepth = D3D12_MAX_DEPTH;
    commandList->RSSetViewports(1, &viewPort);
	CD3DX12_RECT rect = CD3DX12_RECT(0, 0, width, height);
    /*
	D3D12_RECT rect = {};
    rect.top = 0;
	rect.left = 0;
	rect.right = width;
	rect.bottom = height;*/
	commandList->RSSetScissorRects(1, &rect);
        
        // Indicate the back buffer will be used as render target
        //CD3DX12_RESOURCE_BARRIER renderTarget =  CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	// Resource barrier is used when we want to use the same resource (say a texture) to write and read both, and in that case, the applicaiton will tell the GPU that this resource is in a write-ready state or read-ready state,and the GPU will wait for the transition if it's trying to read and the resource is in write-ready state.
	D3D12_RESOURCE_BARRIER renderTarget = {};
	D3D12_RESOURCE_TRANSITION_BARRIER barrier = {};
	barrier.pResource = renderTargets[frameIndex].Get();
	barrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; //Transition all subresources in a resourece at the same time.
	barrier.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; //Resource is used as a render target.
	renderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; //Transition of a set of subresources between different usages. The caller must specify the before and after usages of the subresources.
	renderTarget.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	renderTarget.Transition = barrier;
	commandList->ResourceBarrier(1, &renderTarget);

        // Get the current RTV handle
        //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
        //rtvHandle = {};
	rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart(); //Start at the begining of the heap.
	rtvHandle.ptr += frameIndex * rtvDescriptorSize; //Point to the rtv currently (which back buffer's RTV)
        // Clear the render target
        const float clearColor[] = { 1.0f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr); //nullptr meaning no depth and stencil dsecriptor

        // Draw the triangle
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->DrawInstanced(3, 1, 0, 0);

        // Indicate the back buffer will now be used to present
	//CD3DX12_RESOURCE_BARRIER presentTarget = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	D3D12_RESOURCE_BARRIER presentTarget = {};
	barrier.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	presentTarget.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	presentTarget.Transition = barrier;
        commandList->ResourceBarrier(1, &presentTarget);

        // Close the command list
        hr = commandList->Close();
        if (FAILED(hr)) {
            std::cerr << "Failed to close command list" << std::endl;
            break;
        }

        // Execute the command list
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Present the frame
        hr = swapChain->Present(1, 0);
        if (FAILED(hr)) {
            std::cerr << "Present failed" << std::endl;
            break;
        }

        // Schedule a signal command
        hr = commandQueue->Signal(fence.Get(), fenceValue); //Will tell the GPU, after finishing all currentyly queued commands, set the fence to fencevalue, meaning after finishing it, it will set the GPU fence value to the fence value variable in the CPU
        if (FAILED(hr)) {
            std::cerr << "Failed to signal fence" << std::endl;
            break;
        }

        // Wait for the frame to complete
        if (fence->GetCompletedValue() < fenceValue) { //getcompletevalue is the value of the last fence if nothing is completed, and the new fence if things have been compelted
            hr = fence->SetEventOnCompletion(fenceValue, fenceEvent); // tells the fence: "When you reach this value, wake up this Windows event"
            if (FAILED(hr)) {
                std::cerr << "Failed to set fence event" << std::endl;
                break;
            }
            WaitForSingleObject(fenceEvent, INFINITE);
        }

        // Prepare for next frame
        frameIndex = swapChain->GetCurrentBackBufferIndex();
        fenceValue++;
        PrintDebugMessages();
    }

//What happens if we exit out of this loop through quitting or something during GPU work? We don't want the GPU to be left doing work, so we wait for the GPU to finish, or else it can cause memory corruption for GPU, Crashes, Driver problems etc.
    if(fence->GetCompletedValue() < fenceValue - 1) { //Imagine before signalling or anything, fence value = 1 on the cpu, the value in the fence is 0, in this case, if we don't do -1, it will keep doing this, because we haven't asked the fence to signal at all. This is for that one thing where we say that the fence should signal the fence value, then we quit the game loop, however the commandqueue is still being executed.
        hr = fence->SetEventOnCompletion(fenceValue - 1, fenceEvent);
        if(FAILED(hr)){
            std::cout<<"Failed to set fence event on completion outside the rendering loop!";
            return -1;
        }
        WaitForSingleObject(fenceEvent, INFINITE);
    }
CloseHandle(fenceEvent);
//SDL_Delay(3000);  // Show the window for 3 seconds
SDL_DestroyWindow(window);
SDL_Quit();
return 0;
}
