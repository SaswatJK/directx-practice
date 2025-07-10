#include "../include/utils.h"
#include "../include/shader.h"
#include "D3D12/d3d12.h"
#include "D3D12/d3dx12_core.h"
#include "D3D12/dxgiformat.h"
#include <combaseapi.h>
#include <intsafe.h>
#include <malloc.h>
#include <wrl/client.h>
#include <wrl/implements.h>

UINT windowWidth = 1000;
UINT windowHeight = 1000;

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

typedef struct{
    float pos[3];
}Vertex;

int main(){
    SDL_Init(SDL_INIT_VIDEO);
    HRESULT hr;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory; //Latest version of factory, can check feature support, preference of GPU, as well as warp works
    Microsoft::WRL::ComPtr<IDXGIFactory2> tempFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&tempFactory));
    if(FAILED(hr)){
        std::cerr<<"Can't create a factory for dxgi";
        return -1;
    }
    tempFactory.As(&dxgiFactory);

    BOOL allowTearing = FALSE;
    dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    //std::cout<<"Skin tearing is: "<<allowTearing;

    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
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
    d3d12Device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    SDL_Window* window = SDL_CreateWindow("RayTracing", windowWidth, windowHeight, 0);
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

    D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; //Type upload is the best for CPU read once and GPU write once Data. This type has CPU access optimized for uploading to the GPU, which is why it has a CPU virtual address, not just a GPU one. Resouurce on this Heap must be created with GENERIC_READ
    uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; //I used WRITE_COMBINE enum but it gave me error, so I have to use PROPERTY_UNKOWN. The write comobine ignores the cache and writes to a buffer, it's better for streaming data to another device like the GPU, but very inefficient if we wanna read for some reason from the same buffer this is pointing to.
    uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; //I used MEMORY_POOL_L0 but it gave me error, so I have to use POOL_UNKOWN. L0 means the system memory (RAM), while L1 is the videocard memory (VRAM). If integrated graphics exists, this will be slower for the GPU but the CPU will be faster than the GPU. However it does still support APUs and Integrated Graphics, while if we use L1, it's only available for systems with their own graphics cards. It cannot be accessed by the CPU at all, so it's only GPU accessed.
    //I think I read somehwere that the reason I got error was due to redundancy? I will check later
    uploadHeapProperties.CreationNodeMask = 1;
    uploadHeapProperties.VisibleNodeMask = 1;

    D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; //Default type is the best for GPU reads and writes, it's not accessible by the CPU.
    defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; //It is actually not readable from the CPU. We say unkown because we are using a non custom type.
    defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; //Actully L1 but since non custom type.
    defaultHeapProperties.CreationNodeMask = 1;
    defaultHeapProperties.VisibleNodeMask = 1;

    Microsoft::WRL::ComPtr<ID3D12Heap1> uploadHeap;
    D3D12_HEAP_DESC uploadHeapDesc;
    uploadHeapDesc.Properties = uploadHeapProperties;
    uploadHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64 KB.
    uploadHeapDesc.SizeInBytes = 4194300; //65536 for buffer alignments.. Now, we start constant buffer and vertex buffer in those, so they take up 65536, then for the texture, we have the 4095904 bytes, but since it's still a buffer, we need to start it from 65536 byte offset. So that + 655536 = some bytes, and we add extra bytes jsut so that it's a multiple of 64KB.
    uploadHeapDesc.Flags = D3D12_HEAP_FLAG_NONE;

    hr = d3d12Device->CreateHeap(&uploadHeapDesc, IID_PPV_ARGS(&uploadHeap));
    if(FAILED(hr)){
        std::cout<<"Upload heap creation has failed!";
        return -1;
    }
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain; //For compatibility reasons as the createswapchainforhwnd method only takes in swapchain1
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    DXGI_SAMPLE_DESC swapSampleDesc = {};
    swapSampleDesc.Count = 1;
    swapChainDesc.Width = windowWidth; //Can make it 0 during the creation of swapchain, that will automatically obtain the width from the hwnd
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //Last time I tried to change it to more, the thing kept crashing
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; //Haven't thought of it
    swapChainDesc.SampleDesc = swapSampleDesc;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
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
    float metallic[4] = { 1.0f, 0.2f, 0.0f, 1.0f };

    Vertex triangle[3] = {
        {{0.0f, 0.5f, -0.5f}},
        {{0.5f, -0.5f, 0.5f}},
        {{-0.5, -0.5f, 0.0f}}
    };

    Vertex quad[4] = {
        {{-1.0f, -1.0f, 0.0f}}, //bottom left
        {{-1.0f, 1.0f, 0.0f}}, //Top left
        {{1.0f, 1.0f, 0.0f}}, //Top right
        {{1.0f, -1.0f, 0.0f}} //Bottom right
    };

    unsigned int quadIndices[6] = {
        0, 1, 2, //top left triangle
        2, 3, 0 //bottom right triangle
    };

    // Committed resource is the easiest way to create resource since we don't need to do heap management ourselves.
    //I wanted to use ID3D12Resource2 but it's so unimportant that these guys didn't even document it properly, there's no links to new features or to the old interface it has inherited from LMFAO
    Microsoft::WRL::ComPtr<ID3D12Resource> uniformBuffer;
    D3D12_RESOURCE_DESC primaryBufferDesc = {};
    DXGI_SAMPLE_DESC primaryBufferSampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    primaryBufferSampleDesc.Count = 1;
    primaryBufferSampleDesc.Quality = 0;
    primaryBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    primaryBufferDesc.Alignment = 0;
    primaryBufferDesc.Width = 256 + sizeof(triangle) + sizeof(quad) + sizeof(quadIndices);
    primaryBufferDesc.Height = 1;
    primaryBufferDesc.DepthOrArraySize = 1;
    primaryBufferDesc.MipLevels = 1;
    primaryBufferDesc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    primaryBufferDesc.SampleDesc = primaryBufferSampleDesc;
    primaryBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    primaryBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    //CreateCommittedResource2 can also be used, it makes it easier to do memory management as it will handle the heapAllocation itself, however that means only one resource per heap, and no custom heap layouts. Now, while I have not used custom heap layouts till now, I think I don't want more abstracted things while learning.
    //CreateCommittedResource3 uses layout rather than states for barriers and stuff, which should give more control for the memory of both GPU, CPU, and the access of either/or memory from either/or physical hardware (host or device), so does CreatePlacedResource2, while CreatePlacedResource1 looks for desc_1 rather than desc for resources.
    UINT64 bufferOffset = 0;
    hr = d3d12Device->CreatePlacedResource(uploadHeap.Get(), bufferOffset, &primaryBufferDesc,
                                           D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Your application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                           nullptr, IID_PPV_ARGS(&uniformBuffer));
    if(FAILED(hr)){
        std::cout<<"Could not place the Vertex Buffer Resource";
        PrintDebugMessages();
        return -1;
    }

    UINT64 constantBufferOffset = 0;
    //Map the data from the host RAM to the GPU VRAM
    //What map does is, it gives a cpu accessible pointer to the GPU memory. Gets a GPU pointer to the specified subresource in the resource, but may not disclose the pointer value to applications. Map also invalidates the CPU cache, when necessary, so that CPU reads to this address reflect any modifications made by the GPU.
    UINT8* mappedData = nullptr;
    uniformBuffer->Map(0,//Index number of the subresource
                      nullptr,//Nullptr as read range means that the entire subreousrce might be read by the CPU, or else we can use a d3d12_range struct that describes the range of memory to access. We use non nullptr for read-range only if we want to update partially a large buffer, or read existing vertex and modify it.
                      reinterpret_cast<void**>(&mappedData));
    //After the map operation, a GPU pointer is given.
    //Why not use GetGPUVirtualAddress() method instead of Map? Because the CPU cannot understand the GPUVirtualAddress.
    memcpy(mappedData, &color, 16);
    memcpy(mappedData + 16, &metallic, 16);
    memcpy(mappedData + 256, &triangle, sizeof(triangle));
    memcpy(mappedData + 256 + sizeof(triangle), &quad, sizeof(quad));
    memcpy(mappedData + 256 + sizeof(triangle) + sizeof(quad), &quadIndices, sizeof(quadIndices));
    uniformBuffer->Unmap(0, nullptr);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> uniformHeap;
    D3D12_DESCRIPTOR_HEAP_DESC uniformHeapDesc = {};
    uniformHeapDesc.NumDescriptors = 3;
    uniformHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    uniformHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_CPU_DESCRIPTOR_HANDLE descHandle = {};
    hr = d3d12Device->CreateDescriptorHeap(&uniformHeapDesc, IID_PPV_ARGS(&uniformHeap));
    if(FAILED(hr)){
        std::cerr<<"Descriptor heap for uniform heap was not made  successfully!";
        return -1;
    }
    //A view is the same as a descriptor, this is a descriptor that goes in a descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = uniformBuffer->GetGPUVirtualAddress(); //Remember that one resource and/or a subresource can have multiple descriptors referencing it
    cbvDesc.SizeInBytes = 256;
    d3d12Device->CreateConstantBufferView(
        &cbvDesc,
        uniformHeap->GetCPUDescriptorHandleForHeapStart() //Describes the CPU descriptor handle that representes the start of the heap that holds the constant buffer view. This is a pointer or handle that the CPU uses to access and write into the descriptor heap memory. When you create or update a descriptor (like a constant buffer view), the CPU needs to write the descriptor data into the heap. So you get a CPU handle to specify exactly where in the heap to write this descriptor. This handle is valid and usable only on the CPU side, allowing you to fill in descriptor information.
        //Simplest way to say it, we are getting the location of the first descriptor of many (if there are more than 1), in the descriptor heap. Right now it's the start of the descriptor heap, now imagine we wanted another descriptor of the same type in the heap, to create the 'view', we would have to increment this handle to the next descriptor 'slot' in the descriptor heap. When we call functions like CreateShaderResourceView or CreateConstantBufferView, we pass a CPU descriptor handle to specify exactly where in the heap the new descriptor should be written. Later, when binding descriptors to the GPU pipeline, we use GPU descriptor handles to tell the GPU where to find these descriptors during execution.
        //It's because the descriptor heap is in the CPU accessible memory while what it's ponting to is in the GPU, which is why descriptors/views use gpuvirtualaddress for pointing to a buffer that is committed to the GPU memory, but we use cpuhandle for the memory for the descriptors themselves.
    );

    D3D12_VERTEX_BUFFER_VIEW triangleVBView = {};
    triangleVBView.SizeInBytes = sizeof(triangle);
    triangleVBView.BufferLocation = uniformBuffer->GetGPUVirtualAddress() + 256;
    triangleVBView.StrideInBytes = sizeof(Vertex);

    D3D12_VERTEX_BUFFER_VIEW quadVBView = {};
    triangleVBView.SizeInBytes = sizeof(quad);
    triangleVBView.BufferLocation = uniformBuffer->GetGPUVirtualAddress() + 256 + sizeof(triangle);
    triangleVBView.StrideInBytes = sizeof(Vertex);

    D3D12_INDEX_BUFFER_VIEW quadIBView = {};
    quadIBView.SizeInBytes = sizeof(quadIndices);
    quadIBView.BufferLocation = uniformBuffer->GetGPUVirtualAddress() + 256 + sizeof(triangle) + sizeof(quad);
    quadIBView.Format = DXGI_FORMAT_R32_UINT;

    //For future reference: We can have the same heap create multiple descriptor tables, which have contiguous slices of the same heap, this is useful if we want to upload resource once in bulk and use them in slice, in this case, descriptor tables are useful. Now we create a descriptor table and get the starting address from the GPU location of the descriptor heap and add offset of where we want to start the slice from, then we use descriptor range struct to define the numbers of descriptor and everything we want to include in that slice/table

    Microsoft::WRL::ComPtr<ID3D12Resource> backbufferRTs[3];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> backBufferRTVHeap;
    D3D12_DESCRIPTOR_HEAP_DESC backBufferRTVHeapDesc = {};
    backBufferRTVHeapDesc.NumDescriptors = 3;
    backBufferRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    backBufferRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = d3d12Device->CreateDescriptorHeap(&backBufferRTVHeapDesc, IID_PPV_ARGS(&backBufferRTVHeap));
    if(FAILED(hr)){
        std::cerr<<"Descriptor heap creation for render textures failed!";
        return -1;
    }
    UINT rtvDescriptorIncrementSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //Returns the size of the handle increment for the given type of descriptor heap, including any necessary padding.
    D3D12_CPU_DESCRIPTOR_HANDLE bbRTVHandle = backBufferRTVHeap->GetCPUDescriptorHandleForHeapStart(); //Handle to the address of the first descriptor in the heap, remember that before 'binding' these things to the pipeline, it will be in the CPU, so we work with the CPU handle.
    for(UINT8 i = 0; i < 2; i++){
        swapChain->GetBuffer(i, IID_PPV_ARGS(&backbufferRTs[i])); //Basically since our render textures have not been described till now, we use this to get the swapchain buffer, with it's properties and everything and we give it to the render texture. Calling GetBuffer returns a fully described resource created and managed by the swap chain.
        d3d12Device->CreateRenderTargetView(
            backbufferRTs[i].Get(), //Pointer to the resourece, and not the Com object.
            nullptr, //We don't need to make a render_target_view_desc, it will inherit the importnat values like size and format and fill in defaults.
            bbRTVHandle);
        bbRTVHandle.ptr = bbRTVHandle.ptr + rtvDescriptorIncrementSize;
    }

    D3D12_RESOURCE_DESC gBufferDesc = {};
    gBufferDesc.SampleDesc.Count = 1;
    gBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    gBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    gBufferDesc.Height = windowHeight;
    gBufferDesc.Width = windowWidth;
    gBufferDesc.DepthOrArraySize = 1;
    gBufferDesc.MipLevels = 1;
    gBufferDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    gBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    gBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    //We create a samper, the thing with samplers is that they only need to be described and we only create a sampler resource view, and a sampler heap.
    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
    samplerHeapDesc.NumDescriptors = 1;
    samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    samplerHeapDesc.NodeMask = 0;
    samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

    d3d12Device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerHeap));

    D3D12_CPU_DESCRIPTOR_HANDLE samplerHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
    d3d12Device->CreateSampler(&samplerDesc, samplerHandle);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> uRootSignature;
    //Descriptor range defines a contiguous sequence of resource descriptors of a specific type within a descriptor table. Like the 'width' of a slice, that is descriptor table, of a data, that is descriptor heap.
    D3D12_DESCRIPTOR_RANGE uniformRange = {};
    uniformRange.NumDescriptors = 1;
    uniformRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    uniformRange.RegisterSpace = 0;
    uniformRange.BaseShaderRegister = 0;
    uniformRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE textureRange = {};
    textureRange.NumDescriptors = 2;
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.RegisterSpace = 0;
    textureRange.BaseShaderRegister = 0;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE srvRanges[] = { uniformRange, textureRange };
    D3D12_ROOT_DESCRIPTOR_TABLE uniformTable = {};
    uniformTable.NumDescriptorRanges = 2;
    uniformTable.pDescriptorRanges = srvRanges;

    D3D12_DESCRIPTOR_RANGE samplerRange = {};
    samplerRange.NumDescriptors = 1;
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.RegisterSpace = 0;
    samplerRange.BaseShaderRegister = 0;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_DESCRIPTOR_TABLE samplerTable = {};
    samplerTable.NumDescriptorRanges = 1;
    samplerTable.pDescriptorRanges = &samplerRange;

    //Root parameters define how resoureces are bound to the pipeline. We right now use descriptor tables, which are just pointers to a fixed range of data in a descriptor heap. One root parameter can onyl contain one descriptor table, so only one slice, which is why more than one may be required.
    D3D12_ROOT_PARAMETER rootParameterUniform = {};
    rootParameterUniform.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameterUniform.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameterUniform.DescriptorTable = uniformTable;

    D3D12_ROOT_PARAMETER rootParameterSampler = {};
    rootParameterSampler.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameterSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameterSampler.DescriptorTable = samplerTable;

    D3D12_ROOT_PARAMETER rootParams[] = { rootParameterUniform, rootParameterSampler };
    D3D12_ROOT_SIGNATURE_DESC uRootSignatureDesc = {};
    uRootSignatureDesc.NumParameters = 2; //Can have more than one descriptor table
    uRootSignatureDesc.pParameters = rootParams; //Pointer to an array of descriptor tables if they are multiple of them.
    uRootSignatureDesc.NumStaticSamplers = 0;
    uRootSignatureDesc.pStaticSamplers = nullptr;
    uRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob; //Used to return data of an arbitrary length
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob; //Same as above
    // The root signature is the definition of an arbitrarily arranged collection of descriptor tables (including their layout), root constants and root descriptors. It basically describes the layout of resource used by the pipeline.

    //'Serializing' a root signature means converting a root signature description from the desc structures, to a GPU readable state. We need to serialize into blobs the root signature, which talks about textures and buffers and where to find them and what not, and shaders, so the GPU can understand it.
    //This description is a CPU-side data structure describing how our shaders will access resources. To create a GPU-usable root signature (ID3D12RootSignature), our must serialize this description into a binary format.
    D3D12SerializeRootSignature(&uRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);

    hr = d3d12Device->CreateRootSignature(
        0, // 0 for a single GPU, or else multipel GPU then have to decide which 'node' the signature needs to be applied to.
        signatureBlob->GetBufferPointer(), //Pointer to the source data for the serialized signature.
        signatureBlob->GetBufferSize(), //Size of the blob
        IID_PPV_ARGS(&uRootSignature));
    if(FAILED(hr)){
        std::cout<<"Root signature creation failed!";
        return -1;
    }
    //Reminder for the future. I dont' need to make a root signature for index or vertex buffers. I only need to copy it into GPU memory, through memcpy and mapping to get the pointer to the GPU memory, and I need to call IASetVertexBuffers, IASetIndexBuffer for it. So just make a d3d12 resource, map cpu resource to it, then call the function! I have to initailize the resource through resource_desc ofcourse, but it's not as crazy as the SRV or CBV ones.

    //Pipeline state objects should be created upfront. They should be created at the start with all the possible rasterisation descriptions and the shader combinations. And use that to compile all the shader before hand. After that, they should be switched between scenes, rather than making a new PSO everytime we need it. This is where the 'precompilation' of shaders that are so common nowadays, comes in. So for better readability and managibility, define the other piepilne state structs first that should be common among all the pipeline states, like the rasterizer's properties. Then the shader's names shoudl reflect what it does and then be used for different pipeline states with their own names. Remember that PSO is the main hickup when it comes to game engines nowadays and the frametime spike that occurs.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> firstpassPSO;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC firstpassPSODesc = {};
    //Will be using the same dxgi_sample_desc that I created for swap chain.
    firstpassPSODesc.SampleDesc = swapSampleDesc;
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
    firstpassPSODesc.BlendState = opaqueBlendDesc;
    D3D12_RASTERIZER_DESC simpleRasterizerDesc = {};
    simpleRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    simpleRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; //Do not draw triangles that are back facing.
    simpleRasterizerDesc.FrontCounterClockwise = FALSE; // Determines if a triangle is front- or back-facing. If this member is TRUE, a triangle will be considered front-facing if its vertices are counter-clockwise on the render target and considered back-facing if they are clockwise. If this parameter is FALSE, the opposite is true.
    simpleRasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;//Remeber that we have a depth bias that we use for shadow mapping?
    simpleRasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    simpleRasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    //Bias = (float)DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope;
    simpleRasterizerDesc.DepthClipEnable = TRUE; //The hardware clips the z value if true.
    simpleRasterizerDesc.MultisampleEnable = FALSE; //True uses MSAA, false uses alphya line anti aliasing.
    simpleRasterizerDesc.AntialiasedLineEnable = TRUE; //Can only be false if MSAA is true.
    simpleRasterizerDesc.ForcedSampleCount = 0; //Sample count that is forced while UAV rendering. UAV is basically unordered access view, meaning the 'view' can be accesssed in any order, read write, write read write, write read write read etc. If we are to do UAV stuff, like reading and writing to the same texture/buffer concurrently, we can set it over 0, this means the rasterizer acts as if multiple samples exist. So, the GPU processes multiple samples per pixel. It's like doing manual super sampling.
    simpleRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF; //Read the GPU Gems algorithm on conservative rasterization. It's absolutely beautiful. Basically making shit fatter for better inteserction tests.
    //D3D12_DEPTH_STENCIL_DESC dsDesc = {}; Not usign thise because we're not doign depth or stencil testing.
    D3D12_INPUT_LAYOUT_DESC firstpassInputLayoutDesc = {};
    D3D12_INPUT_ELEMENT_DESC firstpassInputElementDesc[] = {
    "POSITION", //Semantic Name
    0, //Semantic index
    DXGI_FORMAT_R32G32B32_FLOAT, //format of the input, since we're using 4 byte floats in the CPU, we might as well use that in the GPU.
    0, //Input slot. The IA stage has n input slots, which are designed to accommodate up to n vertex buffers that provide input data. Each vertex buffer must be assigned to a different slot
    0, //Offset in bytes to this element from the start of the vertex.
    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, //A value that identifies the input data class for a single input slot.
    0 //The number of instances to draw using the same per-instance data before advancing in the buffer by one element. If it's per vertex data, then set this value to 0.
    };
    firstpassInputLayoutDesc.pInputElementDescs = firstpassInputElementDesc;
    firstpassInputLayoutDesc.NumElements = 1; //Only vertex right now.
    firstpassPSODesc.DepthStencilState.DepthEnable = FALSE;
    firstpassPSODesc.DepthStencilState.StencilEnable = FALSE;
    firstpassPSODesc.SampleMask = UINT_MAX; //The sample mask for the blend state. Use in Multisampling to selectively enable or disable certain/specific samples.
    firstpassPSODesc.RasterizerState = simpleRasterizerDesc;
    firstpassPSODesc.InputLayout = firstpassInputLayoutDesc;
    //firstpassPSODesc.IBStripCutValue, we are not filling this cause we don't have an index buffer.
    firstpassPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    firstpassPSODesc.NumRenderTargets = 1;
    firstpassPSODesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
    firstpassPSODesc.NodeMask = 0; //Remember just like the 'NodeMask' in the createrootsignature method, this is for multiGPU systems where we are using node masks to choose between GPUs.
    firstpassPSODesc.pRootSignature = uRootSignature.Get();
    Shader triangleShader("../shaders/simple.vert","../shaders/simple.pixel");
    SimpleShaderByteCode triangleByteCode = triangleShader.getShaderByteCode();
    firstpassPSODesc.VS = triangleByteCode.vsByteCode;
    firstpassPSODesc.PS = triangleByteCode.psByteCode;
    d3d12Device->CreateGraphicsPipelineState(&firstpassPSODesc, IID_PPV_ARGS(&firstpassPSO));

    Microsoft::WRL::ComPtr<ID3D12PipelineState> presentpassPSO;
    D3D12_INPUT_LAYOUT_DESC presentpassInputLayoutDesc = {};
    D3D12_INPUT_ELEMENT_DESC presentpassInputElementDesc[] = {
    "POSITION",
    0,
    DXGI_FORMAT_R32G32B32_FLOAT,
    0,
    0,
    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    0
    };
    presentpassInputLayoutDesc.pInputElementDescs = presentpassInputElementDesc;
    presentpassInputLayoutDesc.NumElements = 1;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC presentpassPSODesc = {};
    presentpassPSODesc.SampleDesc = swapSampleDesc;
    presentpassPSODesc.SampleMask = UINT_MAX;
    presentpassPSODesc.BlendState = opaqueBlendDesc;
    presentpassPSODesc.DepthStencilState.DepthEnable = FALSE;
    presentpassPSODesc.DepthStencilState.StencilEnable = FALSE;
    presentpassPSODesc.RasterizerState = simpleRasterizerDesc;
    presentpassPSODesc.InputLayout = presentpassInputLayoutDesc;
    presentpassPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    presentpassPSODesc.NumRenderTargets = 1;
    presentpassPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    presentpassPSODesc.pRootSignature = uRootSignature.Get();
    Shader quadShader("../shaders/quad.vert", "../shaders/quad.pixel");
    SimpleShaderByteCode quadByteCode = quadShader.getShaderByteCode();
    presentpassPSODesc.VS = quadByteCode.vsByteCode;
    presentpassPSODesc.PS = quadByteCode.psByteCode;
    d3d12Device->CreateGraphicsPipelineState(&presentpassPSODesc, IID_PPV_ARGS(&presentpassPSO));

    int textureWidth, textureHeight, nrChannels;
    std::string texturePath = "C:/Users/broia/Downloads/practicetexture.png";
    unsigned char* textureData = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &nrChannels, 0);
    if (textureData == nullptr)
        std::cout<<"Texture data can't be opened in memory!";

    //Let's do textures
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    D3D12_CLEAR_VALUE textureClearValue = {};

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    textureDesc.Height = textureHeight;
    textureDesc.Width = textureWidth;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE; //Since the flag doesn't state to allow depth/stencil or render target, we must set the clear value to nullptr

    D3D12_RESOURCE_ALLOCATION_INFO allocInfo = d3d12Device->GetResourceAllocationInfo(0, 1, &textureDesc);
    UINT64 textureDHSize = allocInfo.SizeInBytes;
    allocInfo = d3d12Device->GetResourceAllocationInfo(0, 1, &gBufferDesc);
    UINT64 gBufferDHSize = allocInfo.SizeInBytes;

    Microsoft::WRL::ComPtr<ID3D12Heap1> defaultHeap;
    D3D12_HEAP_DESC defaultHeapDesc;
    defaultHeapDesc.Properties = defaultHeapProperties;
    defaultHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64 KB.
    defaultHeapDesc.SizeInBytes = textureDHSize + gBufferDHSize; //Just enough with alignment for the uploaded texture and the gbuffer.
    defaultHeapDesc.Flags = D3D12_HEAP_FLAG_NONE;

    hr = d3d12Device->CreateHeap(&defaultHeapDesc, IID_PPV_ARGS(&defaultHeap));
    if(FAILED(hr)){
        std::cout<<"Default heap creation has failed!";
        return -1;
    }

    hr = d3d12Device->CreatePlacedResource(defaultHeap.Get(), 0, &gBufferDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&backbufferRTs[2]));
    if(FAILED(hr)){
        std::cout<<"G buffer placement error!";
        PrintDebugMessages();
        return -1;
    }

    D3D12_TEX2D_RTV gBufferRTV = {};
    gBufferRTV.MipSlice = 0;
    gBufferRTV.PlaneSlice = 0;

    D3D12_RENDER_TARGET_VIEW_DESC gBufferViewDesc = {};
    gBufferViewDesc.Texture2D = gBufferRTV;
    gBufferViewDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    gBufferViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    hr = d3d12Device->CreatePlacedResource(defaultHeap.Get(), gBufferDHSize, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
    if(FAILED(hr)){
        std::cout<<"Texture resource placement error!";
        return -1;
    }
    d3d12Device->CreateRenderTargetView(backbufferRTs[2].Get(), &gBufferViewDesc, bbRTVHandle);

    //Command queues will execute command lists and signal a fence.
    //Command allocator. A memory pool for where a commands will be placed for the GPU to read. Through a frame from the CPU side, each command is allocated through command lists in a command allocator. Commands are recorded to the command list, when a command list opens, it starts recording commands, and when the command list closes, it stops. So if we want multiple command lists in the command allocator, we must stop the recording of one list and then only move to another. So can only use the command allcoator with multiple command lists if they're all closed of course.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> primaryCommandAllocator;
    hr = d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, //Specifies a command buffer that the GPU can execute. A direct command list doesn't inherit any GPU state.
                                             IID_PPV_ARGS(&primaryCommandAllocator));
    //So what does it mean to inherit state etc? Command lists set states, basically imagine we have a list of commands for a specific scene, we would set a PSO, a root signature, a vertex buffer etc for the lists to use. Now if we want to draw different types of things in different ways, call different commands in different sequences, we can make two different types of command lists. Now what we can do is we can say that it's a bundle type of command list, which means if we execute commandlistA, we need to set the 'states' of the GPU, but then when we execute commandlistB, whether it be the same commandd allocator (easier to make sense) or different, the states are 'inherited', so we don't need to set the vertex buffers, the root signature etc, however we would stil need to set the PSO.
    if(FAILED(hr)){
        std::cout<<"Primary Command allocator creation failed!";
        return -1;
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> triangleCommandList; //Everytime a commandlist is reset, it will allocate a block of memory from the commandallocator, it will pile up. We can reset the command allocator, but if we do so, then we shouldn't use the command list and execute it with the previously allocated memory.
    hr = d3d12Device->CreateCommandList(0,//Node mask, GPU(s)
                                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        primaryCommandAllocator.Get(),
                                        firstpassPSO.Get(),
                                        IID_PPV_ARGS(&triangleCommandList));
    if(FAILED(hr)){
        std::cout<<"Triangle Command list creation failed!";
        return -1;
    }

    D3D12_TEXTURE_COPY_LOCATION textureCopyLocation = {};
    textureCopyLocation.pResource = texture.Get();
    textureCopyLocation.SubresourceIndex = 0;
    textureCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    d3d12Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    Microsoft::WRL::ComPtr<ID3D12Resource> intermeddiateBuffer;
    primaryBufferDesc.Width = totalBytes;
    UINT64 textureBufferOffset = 65536;
    hr = d3d12Device->CreatePlacedResource(uploadHeap.Get(), textureBufferOffset, &primaryBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermeddiateBuffer));
    if(FAILED(hr)){
        std::cout<<"Intermediate buffer for the texture upload failed to allocate!";
        return -1;
    }

    mappedData = nullptr;
    intermeddiateBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    unsigned char* destIB = static_cast<unsigned char*>(mappedData);
    unsigned char* sourceT = textureData;
    PrintDebugMessages();
    //The thing is that D3D12 requires padding for texture data.
    //Now, while the GPU doesn't know that this buffer has texture data, when we copy it, we are copying it in the form of the footprint we 'studied' from the textureDesc. So, D3D12, when copying, will copy the data, with the padding and everything necessary regarding the footprint. So we will need to pad the data while we do a memcpy to begin with.
    //For 'padding', we're just leaving the data empty.
    for (UINT row = 0; row < textureHeight; row++){
        memcpy(destIB + (row * footprint.Footprint.RowPitch), sourceT + (row * textureWidth * nrChannels), textureWidth * nrChannels);
    }
    //memcpy(mappedData, textureData, totalBytes);
    intermeddiateBuffer->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION intermediateCopyLocation = {};
    intermediateCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    intermediateCopyLocation.pResource = intermeddiateBuffer.Get();
    intermediateCopyLocation.SubresourceIndex = 0;
    intermediateCopyLocation.PlacedFootprint = footprint;
    //Synchronization. Why it's important. So the GPU and CPU are both working simulatenously, however, we don't want one to overtake the other, we don't want the CPU to overtake and change the PSO before the frame has been completed by the GPU and cause errors, and we don't want the GPU to start making frames before updating uniforms and stuff by the CPU.
    //A fence is one of the most simple synchronization objects. It exists in both the CPU and the GPU. It has an internal integer counter, and can trigger events on either the GPU or the GPU when the counter reached a certain value, that we can specify.
    //This can be done to trigger an event on the CPU when a command allocator's commands have been executed fully, or we can have the GPU wait while the CPu is completing a certain operation first, or we can get the command queues (if multiple exists), on the GPU to wait upon one another.
    Microsoft::WRL::ComPtr<ID3D12Fence1> fence; //ID3D12Fence1 extends ID3D12Fence, and supports the retrieval of the flags used to create the original fence. This new feature is useful primarily for opening shared fences.
    hr = d3d12Device->CreateFence(0, //Initial fence value
                                  D3D12_FENCE_FLAG_NONE, //Flag for if shared or not. We would want shared fence for syncrhonization after multi-threading, or we can also have a flag for cross-adapter fences, we would want multiple fences for multithreading. Different threads in the CPU build different command lists and command allocators, then different fences syncrhonize the GPU as well while doing execution of said allocators.
                                  IID_PPV_ARGS(&fence));
    if(FAILED(hr)){
        std::cout<<"Fence creation failed!";
        return -1;
    }
    // Command list are not free-threaded; that is, multiple threads may not share the same command list and call its methods concurrently. So generally, each thread will get its own command list.
    // Command allocators are not free-threaded; that is, multiple threads may notshare the same command allocator and call its methods concurrently. So generally, each thread will get its own command allocator.
    // The command queue is free-threaded, so multiple threads can access the command queue and call its methods concurrently. In particular, each thread can submit their generated command list to the thread queue concurrently.
    // For performance reasons, the application must specify at initialization time the maximum number of command lists they will record concurrently.
    HANDLE fenceEvent = CreateEvent(nullptr, //Handle cannot be inhereted by child processes.
                                    FALSE, //If this parameter is FALSE, the function creates an auto-reset event object, and the system automatically resets the event state to nonsignaled after a single waiting thread has been released.
                                    FALSE, //If this parameter is TRUE, the initial state of the event object is signaled; otherwise, it is nonsignaled.
                                    nullptr //If lpName is NULL, the event object is created without a name.
                                    );
    if(fenceEvent == nullptr){
        std::cout<<"Fence Event creation failed!";
        return -1;
    }

    UINT frameIndex = 0; // What frame we are in.
    UINT fenceValue = 1;

    D3D12_RESOURCE_BARRIER textureUploadBarrier = {};
    textureUploadBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    textureUploadBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    D3D12_RESOURCE_TRANSITION_BARRIER textureUploadTransitionBarrier = {};
    textureUploadTransitionBarrier.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    textureUploadTransitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureUploadTransitionBarrier.pResource = texture.Get();
    textureUploadTransitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    textureUploadBarrier.Transition = textureUploadTransitionBarrier;
    triangleCommandList->CopyTextureRegion(&textureCopyLocation, 0, 0, 0, &intermediateCopyLocation, nullptr);
    triangleCommandList->ResourceBarrier(1, &textureUploadBarrier);
    triangleCommandList->Close(); //All command lists created by a d3d12 device is created in an open state, so closing it at first helps us do better error management and tracking.
    ID3D12CommandList* cLists[] = { triangleCommandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

    hr = commandQueue->Signal(fence.Get(), fenceValue); //Will tell the GPU after finishing all the currently queued commands, set the fence to the fence value. It will se the GPU fence value to the fence value variable provided through this method. Basically saying hey commandqueue, after you finish, at this address (given by the fence), put the value (given by fencevalue).
    if(FAILED(hr)){
        std::cout<<"Command queue signal failed before the rendering loop!";
        return -1;
    }
    if (fence->GetCompletedValue() < fenceValue) { //If the value in the fence object is less than the 'fence value' variable, then that means that the command queue has not finished yet, as the command queue will set the value in the fence object to be fence value after finishing.
        hr = fence->SetEventOnCompletion(fenceValue, fenceEvent); //Basically will say that if the fence's value is the value provided through the 'fencevalue' variable, then fenceevent should be "signaled". "Signaling" in this context means that the fence object will notify a event handle if that happens.
        if(FAILED(hr)){
            std::cout<<"Failed to set fence event on completion before the rendering loop!";
            return -1;
        }
        WaitForSingleObject(fenceEvent, INFINITE); //Basically saying that the CPU thread should wait an 'INFINITE' amount of time until event has been signaled. The thread will do other things, and the reason we don't just run an infinite loop instead, checking and seeing if the fence value is reached or not, is because the thread will not be free for other tasks + the memory will keep being accessed over and over again, which may not be bad since cache, but it'll waste the other 64-8 bytes in the cache.
    }
    fenceValue++;
    PrintDebugMessages();

    D3D12_TEX2D_SRV texSRV = {};
    texSRV.MipLevels = 1;
    texSRV.MostDetailedMip = 0;
    texSRV.PlaneSlice = 0;
    texSRV.ResourceMinLODClamp = 0.0f;
    D3D12_SHADER_RESOURCE_VIEW_DESC texView = {};
    texView.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    texView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    texView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    texView.Texture2D = texSRV;
    UINT srvDescriptorIncrementSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE textureHandle = uniformHeap->GetCPUDescriptorHandleForHeapStart();
    textureHandle.ptr = textureHandle.ptr + srvDescriptorIncrementSize;
    d3d12Device->CreateShaderResourceView(backbufferRTs[2].Get(), &texView, textureHandle);
    textureHandle.ptr = textureHandle.ptr + srvDescriptorIncrementSize;
    texView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d3d12Device->CreateShaderResourceView(texture.Get(), &texView, textureHandle);

    bool running = true;
    SDL_Event sdlEvent; //We want to log events for like quitting shit.
    if(!window){
        std::cout<<"SDL window creation failed!";
        return -1;
    }

    while(running){
        while (SDL_PollEvent(&sdlEvent))
            if(sdlEvent.type == SDL_EVENT_QUIT){
                running = false;
                break;
            }
        hr = primaryCommandAllocator->Reset();
        if(FAILED(hr)){
            std::cout<<"Command allocator is reset during the start of each frame, and it couldn't reset!";
            return -1;
        }
        hr = triangleCommandList->Reset(primaryCommandAllocator.Get(), firstpassPSO.Get());
        if(FAILED(hr)){
            std::cout<<"Command list is reset during the start of each frame, and it couldn't reset!";
            return -1;
        }
        triangleCommandList->SetGraphicsRootSignature(uRootSignature.Get());
        // RTV heaps that we use for presenting to the screen, and not for rendering to a frame buffer or for re-use by the shader, do not need to be set with SetDescriptorHeaps, they are instead set by OMSetRenderTarget. If I wanted to reuse those render targets, then I should create a frame buffer and while creating the rtv in the RTVHeapDesc, I should have used the flag for ALLOW_RENDER_TARGET. Now I can re-use the same RTVs for both Framebuffer and for the swapchain, btu then the swapchain format would need to be the same as the RTV format, and we will lose lots of information with only 1 byte for each color channel.
        ID3D12DescriptorHeap* dHeaps[] = { uniformHeap.Get(), samplerHeap.Get() };
        triangleCommandList->SetDescriptorHeaps(_countof(dHeaps), dHeaps);
        triangleCommandList->SetGraphicsRootDescriptorTable(0, //Root parameter index, base register + index (b0, b1, and dependingly...) in the shader. If range says 5 descriptors, then it will basically start from that base register range and form the uniform heap pointer, and then allocate 5 consecutive descriptors.
                                                            uniformHeap->GetGPUDescriptorHandleForHeapStart());
        triangleCommandList->SetGraphicsRootDescriptorTable(1, samplerHeap->GetGPUDescriptorHandleForHeapStart());
        //Each 'slot' in the root parameter can hold anyting from one descriptor to descriptor tables. Each descriptor table can have multiple ranges. Where a range describes a contiguous block of descriptors of same type. Set a certain table. So this binds the 0'th table to the descriptorhandlestart.. So however it has defined it.
        D3D12_VIEWPORT viewPort = {}; //Viewport defines where we want to render to in the swapchain or the render target, whatever we are rendering to. So if I have a viewport of 500x500 in a 1000x1000 window, it will only render to the other 500x500
        viewPort.TopLeftX = 0.0f; //Position of X on the left
        viewPort.TopLeftY = 0.0f; //Position of Y on the top
        viewPort.Width = static_cast<float>(windowWidth);
        viewPort.Height = static_cast<float>(windowHeight);
        viewPort.MinDepth = D3D12_MIN_DEPTH;
        viewPort.MaxDepth = D3D12_MAX_DEPTH;
        // Viewport can clip pixels, and so does a rect, but why 2 different types of clipping? Why would I ever use a viewport and also a rect, why ever use a rect? It's because if I change a viewport, the projection would change, so if I don't wanna change the projection, then I would use a scissor clipping through a rect. Imagine rotating a screen, if u wanna adjust stuff, then change viewport, if u just wanna cut stuff, change rect.
        D3D12_RECT rect = {};
        rect.left = 0;
        rect.top = 0;
        rect.right = windowWidth;
        rect.bottom = windowHeight;
        triangleCommandList->RSSetViewports(1, &viewPort);
        triangleCommandList->RSSetScissorRects(1, &rect);
        // Resource barrier is used when we want to use the same resource (say a texture) to write and read both, and in that case, the applicaiton will tell the GPU that this resource is in a write-ready state or read-ready state,and the GPU will wait for the transition if it's trying to read and the resource is in write-ready state. Now, since our pipeline cannot actually read the render texture, we are going to change state from 'present' meaning, to present to the swapchain, to render target, and vice-versa.
        D3D12_RESOURCE_BARRIER bbBarrierRender = {}; // Describes a resource barrier (transition in resource use).
        bbBarrierRender.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; //Transition of a set of subreosuces between different usages. The caller must specify the before and after usages of the subresources.
        bbBarrierRender.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        D3D12_RESOURCE_TRANSITION_BARRIER bbTransition = {}; //Describes the transition of subresources between different usages.
        //bbTransition.pResource = backbufferRTs[frameIndex].Get();
        bbTransition.pResource = backbufferRTs[2].Get(); //First it's the render texture for firstpass
        bbTransition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; //Transition all subresources at the same time when transitioning state of a resource.
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbBarrierRender.Transition = bbTransition;
        triangleCommandList->ResourceBarrier(1, &bbBarrierRender); //A resource barrier is a command you insert into a command list to inform the GPU driver that a resource (like a texture or buffer) is about to be used in a different way than before. This helps the GPU synchronize access to that resource and avoid hazards such as reading while writing or using stale data.
        // To make it easier for me to undrstand it, I decided to name these heaps after what I am using it for, for example rtv heap is changed to backbufferrtv heap, but that should be changed because this rtv heap can be used for other rtvs and not just the back buffer.
        bbRTVHandle = backBufferRTVHeap->GetCPUDescriptorHandleForHeapStart(); //First back buffer RTV.
        bbRTVHandle.ptr += 2 * rtvDescriptorIncrementSize;
        const float clearColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
        triangleCommandList->ClearRenderTargetView(bbRTVHandle, clearColor,
                                                   0, //Number of rects to clear
                                                   nullptr //Rects to clear, basically we can clear only a certain rect of the screen.
                                                   );
        triangleCommandList->OMSetRenderTargets(1, &bbRTVHandle, FALSE, nullptr); //Nullptr and false since no depth/stencil.
        triangleCommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        triangleCommandList->IASetVertexBuffers(0, 1, &triangleVBView);
        triangleCommandList->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER bbBarrierRead = {};
        bbBarrierRead.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        bbBarrierRead.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bbBarrierRead.Transition = bbTransition;
        triangleCommandList->ResourceBarrier(1, &bbBarrierRead);

        triangleCommandList->SetPipelineState(presentpassPSO.Get()); //Since I have 2 pipelinestates now.
        bbRTVHandle = backBufferRTVHeap->GetCPUDescriptorHandleForHeapStart();
        bbRTVHandle.ptr += frameIndex * rtvDescriptorIncrementSize; 
        bbTransition.pResource = backbufferRTs[frameIndex].Get();
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // The swapchain will always initialize the backbuffers into the present sate, and after that, these states will always have been at the present state after rendering anyways.
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbBarrierRender.Transition = bbTransition;

        triangleCommandList->ResourceBarrier(1, &bbBarrierRead);
        D3D12_RESOURCE_BARRIER bbBarrierPresent = {};
        bbBarrierPresent.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        bbBarrierPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbTransition.StateAfter= D3D12_RESOURCE_STATE_PRESENT;
        bbBarrierPresent.Transition = bbTransition;

        triangleCommandList->ClearRenderTargetView(bbRTVHandle, clearColor, 0, nullptr);
        triangleCommandList->IASetVertexBuffers(0, 1, &quadVBView); //Using the same slot as the previous vertex buffer because this is a different pass and both the vertex buffers don't need to be set at the same time.
        triangleCommandList->IASetIndexBuffer(&quadIBView);
        triangleCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
        triangleCommandList->ResourceBarrier(1, &bbBarrierPresent);

        //I set a PSO, I set root signatures, I set descriptor heaps, I binded the descriptor tables, I got my viewport and scissor rect ready, I have also made a resource barrier and resource.
        hr = triangleCommandList->Close();
        if(FAILED(hr)){
            std::cout<<"Command list close during render-loop failed!";
            return -1;
        }
        PrintDebugMessages();
        ID3D12CommandList* cLists[] = { triangleCommandList.Get() };
        commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

        hr = swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        if(FAILED(hr)){
            std::cout<<"Swapchain present failed!";
            return -1;
        }

        hr = commandQueue->Signal(fence.Get(), fenceValue); //Will tell the GPU after finishing all the currently queued commands, set the fence to the fence value. It will se the GPU fence value to the fence value variable provided through this method. Basically saying hey commandqueue, after you finish, at this address (given by the fence), put the value (given by fencevalue).
        if(FAILED(hr)){
            std::cout<<"Command queue signal failed!";
            return -1;
        }
        if (fence->GetCompletedValue() < fenceValue) { //If the value in the fence object is less than the 'fence value' variable, then that means that the command queue has not finished yet, as the command queue will set the value in the fence object to be fence value after finishing.
            hr = fence->SetEventOnCompletion(fenceValue, fenceEvent); //Basically will say that if the fence's value is the value provided through the 'fencevalue' variable, then fenceevent should be "signaled". "Signaling" in this context means that the fence object will notify a event handle if that happens.
            if(FAILED(hr)){
                std::cout<<"Failed to set fence event on completion in the rendering loop!";
                return -1;
            }
            WaitForSingleObject(fenceEvent, INFINITE); //Basically saying that the CPU thread should wait an 'INFINITE' amount of time until event has been signaled. The thread will do other things, and the reason we don't just run an infinite loop instead, checking and seeing if the fence value is reached or not, is because the thread will not be free for other tasks + the memory will keep being accessed over and over again, which may not be bad since cache, but it'll waste the other 64-8 bytes in the cache.
        }
        frameIndex = swapChain->GetCurrentBackBufferIndex();
        fenceValue++;
        PrintDebugMessages();

    }
    //What happens if we exit out of this loop through quitting or something during GPU work? We don't want the GPU to be left doing work, so we wait for the GPU to finish, or else it can cause memory corruption for GPU, Crashes, Driver problems etc.
    //However, since I only poll exits at the start of the loop (before commandqueue is even signalled), and the fence value is upgraded at the end of the loop (after the GPU has already done the work), I don't need to worry about that problem.
    CloseHandle(fenceEvent);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
