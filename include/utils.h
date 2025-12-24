#pragma once
#include <D3D12/D3dx12.h>
#include <combaseapi.h>
#include <cstring>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgiformat.h>
#include <dxgicommon.h>
#include <stdlib.h>
#include <iostream>
#include <SDL3/SDL.h>
#include <windows.h>
#include <cstdint>
#include "D3D12/d3d12.h"
#include "D3D12/d3d12sdklayers.h"
#include "D3D12/d3dcommon.h"
#include "D3D12/dxgicommon.h"
#include "D3D12/dxgiformat.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_video.h"
#include "glm/ext/vector_float4.hpp"
#include "stb_image.h"
#include <glm/common.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <wrl/client.h>

typedef enum {
    PRIMARY = 0,
    ALLOCATOR_COUNT
}cmdAllocator;

typedef enum {
    RENDER = 0,
    LIST_COUNT
}cmdList;

typedef enum {
    HEAP_UPLOAD = 0,
    HEAP_DEFAULT,
    HEAP_COUNT
}heapInfo;

typedef enum {
    PSO_RENDER = 0,
    PSO_RAYTRACING,
    PSO_PRESENT,
    PSO_COUNT
}psoInfo;

typedef enum {
    BUFFER_VERTEX = 0, //Will upload all vertex buffers in teh same resource, and the views will differentiate, am not gonnna do much premature optimisations right now.
    BUFFER_INDEX,
    BUFFER_PER_FRAME_CONSTANT,
    BUFFER_PER_MODEL_CONSTANT,
    BUFFER_COUNT
}bufferInfo;

typedef enum {
    DH_SRV_CBV_UAV = 0,
    DH_RTV,
    DH_SAMPLER,
    DH_DSV,
    DH_IMGUI_SRV,
    DH_COUNT
}dhInfo;

typedef enum {
    VIEW_SRV = 0,
    VIEW_CBV,
    VIEW_RTV,
    VIEW_UAV,
    VIEW_SAMPLER,
    VIEW_DSV,
    VIEW_COUNT
}viewInfo;

typedef enum {
    TEX_TYPE_DEPTH = 0,
    TEX_TYPE_RGBA,
    TEX_TYPE_COUNT
}textureTypeInfo;

typedef enum {
    BACKBUFFER0 = 0,
    BACKBUFFER1,
    GBUFFER,
    //SHADOWBUFFER, //Can add Later.
    RTV_COUNT
}rtvInfo;

typedef struct{
    glm::vec4 position;
    glm::vec4 color;
    glm::vec4 normal;
}Vertex;


//ALl have 8 byte alignment.
typedef struct D3DResourceStruct{
    Microsoft::WRL::ComPtr<ID3D12Heap> heaps[heapInfo::HEAP_COUNT]; //All the heaps that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12Resource> buffers[bufferInfo::BUFFER_COUNT]; //All the buffer resources that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[dhInfo::DH_COUNT]; //All the descriptor heaps that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[psoInfo::PSO_COUNT];
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> texture2Ds; //All the textures that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature; // I think I can get away with only one root signature because of bindless.
    std::vector<D3D12_VERTEX_BUFFER_VIEW> vbViews;
    std::vector<D3D12_INDEX_BUFFER_VIEW> ibViews;
    UINT32 heapOffsets[heapInfo::HEAP_COUNT] = {0};
    UINT32 descriptorInHeapCount[dhInfo::DH_COUNT] = {0};
    UINT32 eachDescriptorCount[viewInfo::VIEW_COUNT] = {0};
}D3DResources;

typedef struct D3DGlobalStruct{ //prefix of x meaning it's dxgi
    Microsoft::WRL::ComPtr<IDXGIFactory7> xFactory; //Latest version of factory, can check feature support, preference of GPU, as well as warp works
    //Microsoft::WRL::ComPtr<IDXGIFactory2> tempFactory; //I dont' need to put it here.
    Microsoft::WRL::ComPtr<IDXGIAdapter1> xAdapter; //Physical hardware device.
    Microsoft::WRL::ComPtr<ID3D12Device10> device; //This allows for newer methods like CreateComittedResource3, can learn about it in docs, or in this same code underneath at the first use of CreateComittedResource.
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> xSwapChain;
    //Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain; //For compatibility reasons as the createswapchainforhwnd method only takes in swapchain1.
    // Command list are not free-threaded; that is, multiple threads may not share the same command list and call its methods concurrently. So generally, each thread will get its own command list.
    // Command allocators are not free-threaded; that is, multiple threads may notshare the same command allocator and call its methods concurrently. So generally, each thread will get its own command allocator.
    // The command queue is free-threaded, so multiple threads can access the command queue and call its methods concurrently. In particular, each thread can submit their generated command list to the thread queue concurrently.
    // For performance reasons, the application must specify at initialization time the maximum number of command lists they will record concurrently.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[cmdAllocator::ALLOCATOR_COUNT];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> commandLists[cmdList::LIST_COUNT];
    Microsoft::WRL::ComPtr<ID3D12Fence1> fence;
    HANDLE fenceEvent;
}D3DGlobal;
