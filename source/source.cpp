#include <D3D12/D3dx12.h>
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgiformat.h>
#include <dxgicommon.h>
#include <wrl.h>
#include <iostream>
#include <SDL3/SDL.h>
#include <windows.h>
#include <intsafe.h>
#include "D3D12/d3d12.h"
#include "D3D12/d3dcommon.h"
#include "D3D12/dxgicommon.h"

UINT width = 1000;
UINT height = 1000;

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

    //A vie is the same as a descriptor, this is a descriptor that goes in a descriptor heap
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

    D3D12_DESCRIPTOR_RANGE uniformRange = {};
    uniformRange.NumDescriptors = 1;
    uniformRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    uniformRange.RegisterSpace = 0;
    uniformRange.BaseShaderRegister = 0;
    uniformRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_DESCRIPTOR_TABLE uniformTable = {};
    uniformTable.NumDescriptorRanges = 1;
    uniformTable.pDescriptorRanges = &uniformRange;
    D3D12_ROOT_PARAMETER rootParameterUniform = {};
    rootParameterUniform.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameterUniform.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameterUniform.DescriptorTable = uniformTable;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = ;
    rootSignatureDesc.pParameters = ;
    rootSignatureDesc.NumStaticSamplers = ;
    rootSignatureDesc.pStaticSamplers = ;
    rootSignatureDesc.Flags = ;
    // The root signature is the definition of an arbitrarily arranged collection of descriptor tables (including their layout), root constants and root descriptors.
    D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
    return 0;
}
