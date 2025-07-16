#pragma once
#include "D3D12/d3d12.h"
#include "utils.h"
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

typedef union{
    typedef struct{
    PtrSizePair* arr;
    size_t count;
    }PSPArray;
    typedef struct{
    VertexSizePair* arr;
    size_t count;
    }VSPArray;
}DataArray;

D3D12_HEAP_PROPERTIES uploadHeapProperties = {
    D3D12_HEAP_TYPE_UPLOAD, //Type upload is the best for CPU read once and GPU write once Data. This type has CPU access optimized for uploading to the GPU, which is why it has a CPU virtual address, not just a GPU one. Resouurce on this Heap must be created with GENERIC_READ
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN, //I used WRITE_COMBINE enum but it gave me error, so I have to use PROPERTY_UNKOWN. The write comobine ignores the cache and writes to a buffer, it's better for streaming data to another device like the GPU, but very inefficient if we wanna read for some reason from the same buffer this is pointing to.
    D3D12_MEMORY_POOL_UNKNOWN, //I used MEMORY_POOL_L0 but it gave me error, so I have to use POOL_UNKOWN. L0 means the system memory (RAM), while L1 is the videocard memory (VRAM). If integrated graphics exists, this will be slower for the GPU but the CPU will be faster than the GPU. However it does still support APUs and Integrated Graphics, while if we use L1, it's only available for systems with their own graphics cards. It cannot be accessed by the CPU at all, so it's only GPU accessed.
    //I got error due to redundancy in defining, since the type that I have defined already defines the other members.
    1, //Creation node mask, which GPU is created on.
    1 //Visible node mask, which GPU is visible on.
};

D3D12_HEAP_PROPERTIES defaultHeapProperties = {
    D3D12_HEAP_TYPE_DEFAULT, //Default type is the best for GPU reads and writes, it's not accessible by the CPU.
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN, //It is actually not readable from the CPU. We say unkown because we are using a non custom type.
    D3D12_MEMORY_POOL_UNKNOWN, //Actully L1 but since non custom type.
    1,
    1
};

namespace Heap{
    void createHeap(UINT size, heapInfo heap, const D3DGlobal &d3D, D3DResources &resources);
    void createDescriptorHeap(dhInfo dh, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, const D3DGlobal &d3D, D3DResources &resources);
};

namespace Buffer{
    //I am so confused, I wasted so many hours thinking of a data agnostinc way of uplaoding to buffers but I know what I will upload in the vertex and constant buffer tho... I guess it'll be useful for constant and intermmediate bufferse?
    void initVertexBuffer(const DataArray::VSPArray &data, const D3DGlobal &d3D, D3DResources &resources); //width should be the sum of the data.
    void initIndexBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void initConstantBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void initIntermeddiateBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources); //Remember to get the offset of this buffer from the heap before calling this function, for better view control.
    void create2DTexture(void* data, UINT width, UINT height, DXGI_FORMAT, const D3DGlobal &d3D, D3DResources &resources); //For like RTVs for frame buffers.
    void init2DTexture(void* data, UINT width, UINT height, UINT nrChannels, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources); //Copy stuff from intermeddiate buffer/upload heap to default heap. The question is, should I use createpalcedresource for the intermeddiate case or createcommittedreosurce
};
