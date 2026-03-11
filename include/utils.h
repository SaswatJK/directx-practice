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

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef float f32;

#define KiB(x) ((x) * 1024ULL)
#define MiB(x) ((x) * 1024ULL * 1024ULL)
#define GiB(x) ((x) * 1024ULL * 1024ULL * 1024ULL)

#define INSERT_ARRAY_IN_ARENA(arena, type, num, dataPointer, newPointer) (arena).insertData((dataPointer), (num) * sizeof(type), reinterpret_cast<void**>(&(newPointer)))
#define INITIALIZE_ARRAY_IN_ARENA(arena, type, num, newPointer) (arena).insertData(NULL, (num) * sizeof(type), reinterpret_cast<void**>(&(newPointer)))
#define INSERT_OBJECT_IN_ARENA(arena, type, dataPointer, newPointer) (arena).insertData((dataPointer), sizeof(type), reinterpret_cast<void**>(&(newPointer)))
#define INITIALIZE_OBJECT_IN_ARENA(arena, type, newPointer) (arena).insertData(NULL, sizeof(type), reinterpret_cast<void**>(&(newPointer)))
#define DELETE_DATA_IN_ARENA(arena, type, num) (arena).removeData((num) * sizeof(type))
#define GET_POINTER_IN_ARENA(arena, type, newPointer) (arena).castPointer(reinterpret_cast<void**>(&(newPointer)))
#define PUSH_POINTER_IN_ARENA(arena, type, num) (arena).pushPointer(sizeof(type) * (num))

typedef enum{
    ARENA_NO_SPACE_FOR_COMMIT = 322,
    ARENA_NO_SPACE_FOR_DATA,
    ARENA_CREATION_FAILURE,
    ARENA_COMMIT_FAILURE,
    ARENA_FREE_FAILURE,
    ARENA_CANT_DELETE_RESERVED,
    ARENA_CANT_DELETE_NULL,
    ARENA_OK
}ARENA_ERROR;

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
    PSO_PRESENT,
    PSO_COUNT
}psoInfo;

typedef enum {
    BUFFER_VERTEX = 0, //Will upload all vertex buffers in teh same resource, and the views will differentiate, am not gonnna do much premature optimisations right now.
    BUFFER_INDEX,
    BUFFER_PER_FRAME_CONSTANT,
    BUFFER_CONSTANT,
    BUFFER_BLAS_SCRATCH,
    BUFFER_TLAS_DESC,
    BUFFER_TLAS,
    BUFFER_TLAS_SCRATCH,
    BUFFER_MATRICES_BLAS,
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

typedef struct{
    void* data; //Pointer to a resource.
    UINT size; //Size in bytes, of the resouce.
}PtrSizePair;

typedef struct{
    Vertex* data; //Pointer to a vector/array of vertex array.
    UINT size; //Size in bytes, of the vertex vertex array, for each view.
}VertexSizePair;

typedef union {
    struct {
        PtrSizePair* arr;
        size_t count;
    } PSPArray;
    struct {
        VertexSizePair* arr;
        size_t count;
    } VSPArray;
} DataArray;

typedef struct ArenaStruct{
    ARENA_ERROR reserveArena(u64 sizeInBytes);
    ARENA_ERROR commitArena(u64 sizeInBytes);
    ARENA_ERROR insertData(void* data, u64 sizeInBytes, void** outMemoryPointer);
    ARENA_ERROR removeData(u64 sizeInBytes);
    ARENA_ERROR castPointer(void** outMemoryPointer);
    ARENA_ERROR pushPointer(u32 offsetInBytes);
    ARENA_ERROR removeArena();
private:
    u64 arenaSize; // Size of arena.
    u64 arenaSizeCommitted;
    u64 arenaSizeLeftReserved;
    u64 arenaSizeLeftInCommitted;
    void* arenaBasePointer;
    void* arenaLatestPointer;
}Arena;

//ALl have 8 byte alignment.
typedef struct D3DResourceStruct{
    UINT32 heapOffsets[heapInfo::HEAP_COUNT] = {0};
    UINT32 descriptorInHeapCount[dhInfo::DH_COUNT] = {0};
    UINT32 eachDescriptorCount[viewInfo::VIEW_COUNT] = {0};

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> texture2Ds; //All the textures that will be used at once.
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> BLAS; // All the BLASes.
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geoDescs; // All the geometry (in this case each model is a geometry)'s descs.
    std::vector<D3D12_VERTEX_BUFFER_VIEW> vbViews;
    std::vector<D3D12_INDEX_BUFFER_VIEW> ibViews;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs; // Right now I only have one TLAS.

    Microsoft::WRL::ComPtr<ID3D12Heap> heaps[heapInfo::HEAP_COUNT]; //All the heaps that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12Resource> buffers[bufferInfo::BUFFER_COUNT]; //All the buffer resources that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[dhInfo::DH_COUNT]; //All the descriptor heaps that will be used at once.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[psoInfo::PSO_COUNT];

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature; // I think I can get away with only one root signature because of bindless.
    Microsoft::WRL::ComPtr<ID3D12Resource> shaderBindingTable; // Table for all the shaders for RT.
    Microsoft::WRL::ComPtr<ID3D12StateObject> rayTracingState;

    D3D12_GPU_VIRTUAL_ADDRESS rayGenTableAddress;   //Table address for all the ray gen shaders.
    D3D12_GPU_VIRTUAL_ADDRESS hitGroupTableAddress; // Same for group shaders.
    D3D12_GPU_VIRTUAL_ADDRESS missTableAddress;     // Same for miss shaders.

    UINT shaderRecordSize;
    UINT hitGroupTableSize;
    UINT hitGroupTableStride;
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
