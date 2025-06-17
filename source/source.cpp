#include <D3D12/D3dx12.h>
#include <combaseapi.h>
#include <cstring>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgiformat.h>
#include <dxgicommon.h>
#include <ratio>
#include <wrl.h>
#include <iostream>
#include <SDL3/SDL.h>
#include <windows.h>
#include <intsafe.h>
#include <wrl/client.h>
#include "D3D12/d3d12.h"
#include "D3D12/d3dcommon.h"
#include "D3D12/dxgicommon.h"
#include "D3D12/dxgiformat.h"

UINT width = 1000;
UINT height = 1000;

typedef struct{
    float pos[3];
}Vertex;

int main(){
    SDL_Init(SDL_INIT_VIDEO);
    HRESULT hr;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory; //Latest version of factory, can check feature support, preference of GPU, as well as warp works
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if(FAILED(hr)){
        std::cerr<<"Can't create a factory for dxgi";
        return -1;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter; //Physical hardware device
    Microsoft::WRL::ComPtr<ID3D12Device10> d3d12Device; //This allows for newer methods like CreateComittedResource3, can learn about it in docs, or in this same code underneath at the first use of CreateComittedResource
    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, &dxgiAdapter); ++i) {
        if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&d3d12Device))))
            break;
    }

    if(!d3d12Device){
        std::cerr<<"A direct3d 12 compatible device was not able to be made";
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("RayTracing", width, height, 0);
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    //Since creating a SwapChain requires a commandQueue, we need to make a queue first.
    //Submit commands to the GPU, which are collected and then executed in bulk. We don't want the GPU to wait for the CPU or vice versa while commands are being collected.
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //Allows all operations, whether they are graphics, compute or copy. Despite every command type being allowed, only command lists with type DIRECT are allowed to be submitted.
    hr = d3d12Device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
    if(FAILED(hr)){
        std::cerr<<"Can't create command Queue using the device";
        return -1;
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain; //For compatibility reasons as the createswapchainforhwnd method only takes in swapchain1
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    DXGI_SAMPLE_DESC swapSampleDesc = {};
    swapSampleDesc.Count = 1;
    swapChainDesc.Width = width; //Can make it 0 during the creation of swapchain, that will automatically obtain the width from the hwnd
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //Last time I tried to change it to more, the thing kept crashing
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; //Haven't thought of it
    swapChainDesc.SampleDesc = swapSampleDesc;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr, //full sreen desc for fullscreen, and not a windowed swapchain
        nullptr, //If restrict some content
        &tempSwapChain //A pointer to a variable that receives a pointer to the IDXGISwapChain interface for the swap chain that CreateSwapChain creates, that's why I don't need to use the get method for the ComPtr

    );
    if(FAILED(hr)){
        std::cerr<<"SwapChain creation failed!";
        return -1;
    }

    hr = tempSwapChain.As(&swapChain);
    if(FAILED(hr)){
        std::cerr<<"TempSwapChain to SwapChain conversion failed!";
    }

    //First let's make the Uniforms

    float color[4] = { 1.0f, 0.5f, 0.5f, 1.0f };
    float metallic[4] = { 0.3f, 0.2f, 0.0f, 1.0f };

    //I wanted to use ID3D12Resource2 but it's so unimportant that these guys didn't even document it properly, there's no links to new features or to the old interface it has inherited from LMFAO
    Microsoft::WRL::ComPtr<ID3D12Resource> uniformBuffer;
    D3D12_RESOURCE_DESC uniformBufferDesc = {};
    DXGI_SAMPLE_DESC uniformBufferSampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    uniformBufferSampleDesc.Count = 1;
    uniformBufferSampleDesc.Quality = 0;
    uniformBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    uniformBufferDesc.Alignment = 0;
    uniformBufferDesc.Width = 256;
    uniformBufferDesc.Height = 1;
    uniformBufferDesc.DepthOrArraySize = 1;
    uniformBufferDesc.MipLevels = 1;
    uniformBufferDesc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    uniformBufferDesc.SampleDesc = uniformBufferSampleDesc;
    uniformBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uniformBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> uniformHeap;
    D3D12_DESCRIPTOR_HEAP_DESC uniformHeapDesc = {};
    uniformHeapDesc.NumDescriptors = 1;
    uniformHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    uniformHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_CPU_DESCRIPTOR_HANDLE descHandle = {};
    hr = d3d12Device->CreateDescriptorHeap(&uniformHeapDesc, IID_PPV_ARGS(&uniformHeap));
    if(FAILED(hr)){
        std::cerr<<"Descriptor heap for uniform heap was not made  successfully!";
        return -1;
    }

    D3D12_HEAP_PROPERTIES uniformHeapProperties = {};
    uniformHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; //Type upload is the best for CPU read once and GPU write once Data. This type has CPU access optimized for uploading to the GPU, which is why it has a CPU virtual address, not just a GPU one. Resouurce on this Heap must be created with GENERIC_READ
    uniformHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; //I used WRITE_COMBINE enum but it gave me error, so I have to use PROPERTY_UNKOWN. The write comobine ignores the cache and writes to a buffer, it's better for streaming data to another device like the GPU, but very inefficient if we wanna read for some reason from the same buffer this is pointing to.
    uniformHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; //I used MEMORY_POOL_L0 but it gave me error, so I have to use POOL_UNKOWN. L0 means the system memory (RAM), while L1 is the videocard memory (VRAM). If integrated graphics exists, this will be slower for the GPU but the CPU will be faster than the GPU. However it does still support APUs and Integrated Graphics, while if we use L1, it's only available for systems with their own graphics cards. It cannot be accessed by the CPU at all, so it's only GPU accessed.
    //I think I read somehwere that the reason I got error was due to redundancy? I will check later
    uniformHeapProperties.CreationNodeMask = 1;
    uniformHeapProperties.VisibleNodeMask = 1;

    //CreateComittedResource2 can also be used, it makes it easier to do memory management as it will handle the heapAllocation itself, however that means only one resource per heap, and no custom heap layouts. Now, while I have not used custom heap layouts till now, I think I don't want more abstracted things while learning.
    //CreateCommittedResourece3 uses layout rather than states for barriers and stuff, which should give more control for the memory of both GPU, CPU, and the access of either/or memory from either/or physical hardware (host or device)
    hr = d3d12Device->CreateCommittedResource(
        &uniformHeapProperties,
        D3D12_HEAP_FLAG_NONE, //Can also use the ALLOW_ONLY_BUFFERS flag for better debug in the future,
        &uniformBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Your application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
        nullptr, //For a texture or something or render target, can put a clear value here, for efficient clearing basically, we can use other ways to do it too,
        IID_PPV_ARGS(&uniformBuffer));
    if(FAILED(hr)){
        std::cerr<<"Uniform resource comittment failrue!";
        return -1;
    }

    //Map the data from the host RAM to the GPU VRAM
    UINT8* mappedData = nullptr;
    uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    memcpy(mappedData, &color, 16);
    memcpy(mappedData + 16, &metallic, 16);
    uniformBuffer->Unmap(0, nullptr);

    //A view is the same as a descriptor, this is a descriptor that goes in a descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = uniformBuffer->GetGPUVirtualAddress(); //Remember that one resource and/or a subresource can have multiple descriptors referencing it
    cbvDesc.SizeInBytes = 256;
    d3d12Device->CreateConstantBufferView(
        &cbvDesc,
        uniformHeap->GetCPUDescriptorHandleForHeapStart() //Describes the CPU descriptor handle that representes the start of the heap that holds the constant buffer view. This is a pointer or handle that the CPU uses to access and write into the descriptor heap memory. When you create or update a descriptor (like a constant buffer view), the CPU needs to write the descriptor data into the heap. So you get a CPU handle to specify exactly where in the heap to write this descriptor. This handle is valid and usable only on the CPU side, allowing you to fill in descriptor information.
        //Simplest way to say it, we are getting the location of the first descriptor of many (if there are more than 1), in the descriptor heap. Right now it's the start of the descriptor heap, now imagine we wanted another descriptor of the same type in the heap, to create the 'view', we would have to increment this handle to the next descriptor 'slot' in the descriptor heap. When we call functions like CreateShaderResourceView or CreateConstantBufferView, we pass a CPU descriptor handle to specify exactly where in the heap the new descriptor should be written. Later, when binding descriptors to the GPU pipeline, we use GPU descriptor handles to tell the GPU where to find these descriptors during execution.
    );

    //For future reference: We can have the same heap create multiple descriptor tables, which have contiguous slices of the same heap, this is useful if we want to upload resource once in bulk and use them in slice, in this case, descriptor tables are useful. Now we create a descriptor table and get the starting address from the GPU location of the descriptor heap and add offset of where we want to start the slice from, then we use descriptor range struct to define the numbers of descriptor and everything we want to include in that slice/table

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextures[2];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC renderHeapDesc = {};
    renderHeapDesc.NumDescriptors = 2;
    renderHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    renderHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = d3d12Device->CreateDescriptorHeap(&renderHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if(FAILED(hr)){
        std::cerr<<"Descriptor heap creation for render textures failed!";
        return -1;
    }

    UINT rtvDescriptorIncrementSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //Returns the size of the handle increment for the given type of descriptor heap, including any necessary padding.

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart(); //Handle to the address of the first descriptor in the heap, remember that before 'binding' these things to the pipeline, it will be in the CPU, so we work with the CPU handle.
    for(UINT8 i = 0; i < 2; i++){
        swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTextures[i])); //Basically since our render textures have not been described till now, we use this to get the swapchain buffer, with it's properties and everything and we give it to the render texture. Calling GetBuffer returns a fully described resource created and managed by the swap chain.
        d3d12Device->CreateRenderTargetView(
            renderTextures[i].Get(), //Pointer to the resourece, and not the Com object.
            nullptr, //We don't need to make a render_target_view_desc, it will inherit the importnat values like size and format and fill in defaults.
            rtvHandle);
        rtvHandle.ptr = rtvHandle.ptr + rtvDescriptorIncrementSize;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    //Descriptor range defines a contiguous sequence of resource descriptors of a specific type within a descriptor table. Like the 'width' of a slice, that is descriptor table, of a data, that is descriptor heap.
    D3D12_DESCRIPTOR_RANGE uniformRange = {};
    uniformRange.NumDescriptors = 1;
    uniformRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    uniformRange.RegisterSpace = 0;
    uniformRange.BaseShaderRegister = 0;
    uniformRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_DESCRIPTOR_TABLE uniformTable = {};
    uniformTable.NumDescriptorRanges = 1;
    uniformTable.pDescriptorRanges = &uniformRange;

    //Root parameters define how resoureces are bound to the pipeline. We right now use descriptor tables, which are just pointers to a fixed range of data in a descriptor heap. One root parameter can onyl contain one descriptor table, so only one slice, which is why more than one may be required.
    D3D12_ROOT_PARAMETER rootParameterUniform = {};
    rootParameterUniform.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameterUniform.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameterUniform.DescriptorTable = uniformTable;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameterUniform;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob; //Used to return data of an arbitrary length
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob; //Same as above
    // The root signature is the definition of an arbitrarily arranged collection of descriptor tables (including their layout), root constants and root descriptors. It basically describes the layout of resource used by the pipeline.

    //'Serializing' a root signature means converting a root signature description from the desc structures, to a GPU readable state. We need to serialize into blobs the root signature, which talks about textures and buffers and where to find them and what not, and shaders, so the GPU can understand it.
    //This description is a CPU-side data structure describing how our shaders will access resources. To create a GPU-usable root signature (ID3D12RootSignature), our must serialize this description into a binary format.
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);

    hr = d3d12Device->CreateRootSignature(
        0, // 0 for a single GPU, or else multipel GPU then have to decide which 'node' the signature needs to be applied to.
        signatureBlob->GetBufferPointer(), //Pointer to the source data for the serialized signature.
        signatureBlob->GetBufferSize(), //Size of the blob
        IID_PPV_ARGS(&rootSignature));
    if(FAILED(hr)){
        std::cout<<"Root signature creation failed!";
        return -1;
    }
    //Reminder for the future. I dont' need to make a root signature for index or vertex buffers. I only need to copy it into GPU memory, through memcpy, and I need to call IASetVertexBuffers, IASetIndexBuffer for it. So just make a d3d12 resource, map cpu resource to it, then call the function! I have to initailize the resource through resource_desc ofcourse, but it's not as crazy as the SRV or CBV ones.

    //Pipeline state objects should be created upfront. They should be created at the start with all the possible rasterisation descriptions and the shader combinations. And use that to compile all the shader before hand. After that, they should be switched between scenes, rather than making a new PSO everytime we need it. This is where the 'precompilation' of shaders that are so common nowadays, comes in. So for better readability and managibility, define the other piepilne state structs first that should be common among all the pipeline states, like the rasterizer's properties. Then the shader's names shoudl reflect what it does and then be used for different pipeline states with their own names. Remember that PSO is the main hickup when it comes to game engines nowadays and the frametime spike that occurs.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> simplePSO;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC simplePSODesc = {};
    //We will not be filling in StreamOutput or STREAM_OUTPUT_DESC because it's an output description for vertex shader output to a buffer, which is different from a FBO, where the fragment/pixel shader writes to a buffer.
    //D3D12_STREAM_OUTPUT_DESC streamOutputDesc = {}; I won't have a stream output, so I don't need to fill this
    D3D12_BLEND_DESC opaqueBlendDesc = {}; //Blending is done between background, and 'foreground' objects.
    opaqueBlendDesc.AlphaToCoverageEnable = FALSE; //https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f and it's link to https://www.humus.name/index.php?page=3D&ID=61 explains it.
    opaqueBlendDesc.IndependentBlendEnable = FALSE;// If true, each render target can have it's own unique blend state. There's 8 remember that.
    opaqueBlendDesc.RenderTarget[0] = {}; //This is an array of render target blend description structures. Since I said indpendent blend is false, I only need to configure one.
    opaqueBlendDesc.RenderTarget[0].BlendEnable = FALSE;
    opaqueBlendDesc.RenderTarget[0].LogicOpEnable = FALSE; //Cannot have both LogicOpEnable and BlendEnable to be true.
    opaqueBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE; //Specifies the value to perform blending on the pixel shader output, performs before blending with the render target value. Blend factor that is used on the 'object' that will be blended to the render target (background).
    opaqueBlendDesc.RenderTarget[0].DestBlend =  D3D12_BLEND_ZERO; //Specifies the value to perform blending on the current RGB value on the render target. Factor that is used on the 'background'.
    // The 1 for source and 0 for destination makes this happen: FinalColor=(SrcColor×1)+(DestColor×0)=SrcColor (so opaque)
    opaqueBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    opaqueBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE; //Same thing as before, but this is specifically about the alpha's factor.
    opaqueBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    opaqueBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    opaqueBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP; //Operation to do while we writing to the render target.
    opaqueBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //Identifies which components of each pixel of a render target are writeable during blending. This allows for some whacky shaders if we only blend certain color channels. We can also just combine them iirc.
    D3D12_RASTERIZER_DESC simpleRasterizerDesc = {};
    simpleRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    simpleRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; //Do not draw triangles that are front facing.
    simpleRasterizerDesc.FrontCounterClockwise = FALSE; // Determines if a triangle is front- or back-facing. If this member is TRUE, a triangle will be considered front-facing if its vertices are counter-clockwise on the render target and considered back-facing if they are clockwise. If this parameter is FALSE, the opposite is true.
    simpleRasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;//Remeber that we have a depth bias that we use for shadow mapping?
    simpleRasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    simpleRasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    //Bias = (float)DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope;
    simpleRasterizerDesc.DepthClipEnable = TRUE; //The hardware clips the z value if true.
    simpleRasterizerDesc.MultisampleEnable = FALSE; //True uses MSAA, false uses alphya line anti aliasing.
    simpleRasterizerDesc.AntialiasedLineEnable = FALSE; //Can only be false if MSAA is true.
    simpleRasterizerDesc.ForcedSampleCount = 0; //Sample count that is forced while UAV rendering. UAV is basically unordered access view, meaning the 'view' can be accesssed in any order, read write, write read write, write read write read etc. If we are to do UAV stuff, like reading and writing to the same texture/buffer concurrently, we can set it over 0, this means the rasterizer acts as if multiple samples exist. So, the GPU processes multiple samples per pixel. It's like doing manual super sampling.
    simpleRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF; //Read the GPU Gems algorithm on conservative rasterization. It's absolutely beautiful. Basically making shit fatter for better inteserction tests.
    //D3D12_DEPTH_STENCIL_DESC dsDesc = {}; Not usign thise because we're not doign depth or stencil testing.
    D3D12_INPUT_LAYOUT_DESC simpleInputLayoutDesc = {};
    D3D12_INPUT_ELEMENT_DESC simpleInputElementDesc[] = {
    "POSITION", //Semantic Name
    0, //Semantic index
    DXGI_FORMAT_R32G32B32_FLOAT, //format of the input, since we're using 4 byte floats in the CPU, we might as well use that in the GPU.
    0, //Input slot. The IA stage has n input slots, which are designed to accommodate up to n vertex buffers that provide input data. Each vertex buffer must be assigned to a different slot
    0, //Offset in bytes to this element from the start of the vertex.
    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, //A value that identifies the input data class for a single input slot.
    0 //The number of instances to draw using the same per-instance data before advancing in the buffer by one element. If it's per vertex data, then set this value to 0.
    };
    simpleInputLayoutDesc.pInputElementDescs = simpleInputElementDesc;
    simpleInputLayoutDesc.NumElements = 1; //Only vertex right now.
    simplePSODesc.SampleMask = UINT_MAX; //The sample mask for the blend state. Use in Multisampling to selectively enable or disable certain/specific samples.
    simplePSODesc.RasterizerState = simpleRasterizerDesc;
    simplePSODesc.InputLayout = simpleInputLayoutDesc;
    //simplePSODesc.IBStripCutValue, we are not filling this cause we don't have an index buffer.
    simplePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    simplePSODesc.NumRenderTargets = 1;
    simplePSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    simplePSODesc.NodeMask = 0; //Remember just like the 'NodeMask' in the createrootsignature method, this is for multiGPU systems where we are using node masks to choose between GPUs.
    simplePSODesc.pRootSignature = rootSignature.Get();

    const char* simpleVSSource = R"(
        //Remember how in glsl we use structs or even variables that we want to share from one shader to another, like vs to fs through an 'out' attributed variable or struct? We don't need to do that.
        struct PSInput {
            float4 position : SV_POSITION; //System-value semantic for clip-space position
        };

        PSInput VSMain(float3 position : POSITION ) { //Can name this whatever we want, however we need to set the entrypoint name whle compiling the shader.
        PSInput result;
        result.position = float4(position, 0.0f);
        return result;
        }
    )";
    const char* simplePSSource = R"(
        cbuffer simpleUB : register(b0){
            float4 color;
            float4 metallic;
        };
        float4 PSMain() : SV_TARGET {
            float4 finalColor = color;
            return finalColor;
        }
    )";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    hr = D3DCompile(simpleVSSource, //source of the shader
                    strlen(simpleVSSource),
                    nullptr, //Use this parameter for strings that specify error messages, if not used then set to null.
                    nullptr, //Array of shader macros! YEP!!
                    nullptr, //Pointer to an id3dinclude for handling include fies. Set to NULL if no #include in the shader.
                    "VSMain", //main functin entry point for shader.
                    "vs_5_0", //Compiler target
                    0,
                    0,
                    &vsBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Vertex Shader";
        return -1;
    }
    hr = D3DCompile(simplePSSource, //source of the shader
                    strlen(simplePSSource),
                    nullptr, //Use this parameter for strings that specify error messages, if not used then set to null.
                    nullptr, //Array of shader macros! YEP!!
                    nullptr, //Pointer to an id3dinclude for handling include fies. Set to NULL if no #include in the shader.
                    "PSMain", //main functin entry point for shader.
                    "ps_5_0", //Compiler target
                    0,
                    0,
                    &psBlob,
                    &errorBlob);
    if(FAILED(hr)){
        std::cout<<"Failed to compile Pixel Shader";
        return -1;
    }
    D3D12_SHADER_BYTECODE vsByteCode;
    vsByteCode.pShaderBytecode = vsBlob->GetBufferPointer(); //Bytecode's pointer to a memory.
    vsByteCode.BytecodeLength = vsBlob->GetBufferSize();
    D3D12_SHADER_BYTECODE psByteCode;
    psByteCode.pShaderBytecode = psBlob->GetBufferPointer(); //Bytecode's pointer to a memory.
    psByteCode.BytecodeLength = psBlob->GetBufferSize();

    simplePSODesc.VS = vsByteCode;
    simplePSODesc.PS = psByteCode;

    //Will be using the same dxgi_sample_desc that I created for swap chain.
    d3d12Device->CreateGraphicsPipelineState(&simplePSODesc, IID_PPV_ARGS(&simplePSO));

    Vertex triangle[3] = {
        {{-0.5, -0.5, 0}},
        {{0, 0.5, 0}},
        {{0.5, 0.5, 0}}
    };


    return 0;
}
