#include "../include/resource.h"
#include "D3D12/d3d12.h"
#include "utils.h"
#include <intsafe.h>

void Heap::createUploadHeap(UINT size, const D3DGlobal &d3D, D3DResources &resources){
    UINT heapSize = size;
    if(size < 65536 * 4) //The 4 bfufers that will be places in the upload heap.
        size = 65536 * 4;
    D3D12_HEAP_DESC desc;
    desc.Properties = uploadHeapProperties;
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64 KB.
    desc.SizeInBytes = size; //65536 for buffer alignments.. if it's still a buffer, we need to start it from 65536 byte offset. So that + 655536 = some bytes, and we add extra bytes jsut so that it's a multiple of 64KB.
    desc.Flags = D3D12_HEAP_FLAG_NONE;

    HRESULT hr = d3D.device->CreateHeap(&desc, IID_PPV_ARGS(&resources.heaps[UPLOAD_HEAP]));
    if(FAILED(hr)){
        std::cerr<<"Upload heap creation has failed!";
        return;
    }
}

void Heap::createDefaultHeap(UINT size, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_HEAP_DESC desc;
    desc.Properties = defaultHeapProperties;
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64 KB.
    desc.SizeInBytes = size; //Just enough with alignment for the uploaded texture and the gbuffer, and we uise the default heap size that we get from the resource allocation info.
    desc.Flags = D3D12_HEAP_FLAG_NONE;
    HRESULT hr = d3D.device->CreateHeap(&desc, IID_PPV_ARGS(&resources.heaps[DEFAULT_HEAP]));
    if(FAILED(hr)){
        std::cerr<<"Default heap creation has failed!";
        return;
    }
}

void Buffer::createVertexBuffer(const std::vector<Vertex> &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    desc.Alignment = 0;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    UINT64 dataSize = data.size() * sizeof(Vertex);
    desc.Width = dataSize;
    //Committed resource is the easiest way to create resource since we don't need to do heap management ourselves.
    //I wanted to use ID3D12Resource2 but it's so unimportant that these guys didn't even document it properly, there's no links to new features or to the old interface it has inherited from LMFAO. CreateCommittedResource3 uses layout rather than states for barriers and stuff, which should give more control for the memory of both GPU, CPU, and the access of either/or memory from either/or physical hardware (host or device), so does CreatePlacedResource2, while CreatePlacedResource1 looks for desc_1 rather than desc for resources.
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[VERTEX_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for vertex buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[VERTEX_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(mappedData));
    memcpy(mappedData, data.data(), dataSize);
    resources.buffers[VERTEX_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
}

void Buffer::createIndexBuffer(const std::vector<UINT32> &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    desc.Alignment = 0;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    UINT64 dataSize = data.size() * sizeof(UINT32);
    desc.Width = dataSize;
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[INDEX_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for index buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[INDEX_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(mappedData));
    memcpy(mappedData, data.data(), dataSize);
    resources.buffers[INDEX_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
}

void Buffer::createConstantBuffer(const PSPArray &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    UINT dataSize = 256; //I'm not goind to do a bunch of crazy stuff..
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    desc.Alignment = 0;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Width = dataSize;
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[CONSTANT_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for constant buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[CONSTANT_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(mappedData));
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.count; i++){
        memcpy(mappedData, data.arr[i].data, data.arr[i].size);
        currentPtr += data.arr[i].size;
    }
    resources.buffers[CONSTANT_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
}


