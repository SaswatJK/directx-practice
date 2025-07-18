#pragma once
#include "D3D12/d3d12.h"
#include "utils.h"
#include "shader.h"
#include <wrl/client.h>
#include <cstdint>
#include <vector>

typedef struct{
    void* data; //Pointer to a resource.
    UINT size; //Size in bytes, of the resouce.
}PtrSizePair;

typedef struct{
    Vertex* data; //Pointer to a vector of vertex array.
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

namespace Heap{
    void createHeap(UINT size, heapInfo heap, const D3DGlobal &d3D, D3DResources &resources);
    void createDescriptorHeap(dhInfo dh, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, const D3DGlobal &d3D, D3DResources &resources);
};

namespace Resource{
    //I am so confused, I wasted so many hours thinking of a data agnostinc way of uplaoding to buffers but I know what I will upload in the vertex and constant buffer tho... I guess it'll be useful for constant and intermmediate bufferse?
    void initVertexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources); //width should be the sum of the data.
    void initIndexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void initConstantBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void createGPUTexture(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources); //For like RTVs for frame buffers.
    void createBackBuffers(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources); //For like the 2 back buffers.
    void init2DTexture(void* data, UINT width, UINT height, UINT nrChannels, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources); //Copy stuff from intermeddiate buffer/upload heap to default heap. The question is, should I use createpalcedresource for the intermeddiate case or createcommittedreosurce
    void createSimpleSampler(const D3DGlobal &d3D, D3DResources &resources);
};

namespace RootSignature{
    void createBindlessRootSignature(const D3DGlobal &d3D, D3DResources &resources);
}

namespace PipelineState{
    void createGraphicsPSO(psoInfo info, const Shader &shader, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources);
}
