#include "D3D12/d3d12.h"
#include "utils.h"
#include <wrl/client.h>
#include <cstdint>
#include <vector>

typedef struct{
    void* data;
    UINT size;
}PtrSizePair;

typedef struct{
    PtrSizePair* arr;
    size_t count;
}PSPArray;

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
    void createUploadHeap(UINT size, const D3DGlobal &d3D, D3DResources &resources);
    void createDefaultHeap(UINT size, const D3DGlobal &d3D, D3DResources &resources);
};

namespace Buffer{
    //I am so confused, I wasted so many hours thinking of a data agnostinc way of uplaoding to buffers but I know what I will upload in the vertex and constant buffer tho... I guess it'll be useful for constant and intermmediate bufferse?
    void createVertexBuffer(const std::vector<Vertex> &data, const D3DGlobal &d3D, D3DResources &resources); //width should be the sum of the data.
    void createIndexBuffer(const std::vector<UINT32> &data, const D3DGlobal &d3D, D3DResources &resources);
    void createConstantBuffer(const PSPArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void createIntermeddiateBuffer(const PSPArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void create2DTexture(const PSPArray &data, UINT width, UINT height, DXGI_FORMAT, const D3DGlobal &d3D, D3DResources &resources);
};
