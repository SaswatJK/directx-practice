#include "../include/resource.h"
#include "D3D12/d3d12.h"
#include "utils.h"
#include <combaseapi.h>
#include <intsafe.h>
#include <minwindef.h>
#include <wrl/client.h>

void Heap::createHeap(UINT size, heapInfo heap, const D3DGlobal &d3D, D3DResources &resources){
    UINT heapSize = size;
    D3D12_HEAP_DESC desc;
    switch(heap) {
        case UPLOAD_HEAP:
            desc.Properties = uploadHeapProperties;
            if(heapSize < 65536 * 4) // The 4 bfufers that will be placed in the upload heap
                heapSize = 65536 * 4;
            break;
        case DEFAULT_HEAP:
            desc.Properties = defaultHeapProperties;
            break;
        default:
            std::cerr << "Unknown heap type: " << heap << std::endl;
            return;
    }
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64 KB.
    desc.SizeInBytes = heapSize; //65536 for buffer alignments.. if it's still a buffer, we need to start it from 65536 byte offset. So that + 655536 = some bytes, and we add extra bytes jsut so that it's a multiple of 64KB.
    desc.Flags = D3D12_HEAP_FLAG_NONE;
    HRESULT hr = d3D.device->CreateHeap(&desc, IID_PPV_ARGS(&resources.heaps[heap]));
    if(FAILED(hr)){
        std::cerr<<"Creation of heap of type: "<<heap<<" has failed!";
        return;
    }
}

void Heap::createDescriptorHeap(dhInfo dh, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
    dhDesc.Type = type;
    dhDesc.NumDescriptors = 10;
    dhDesc.Flags = flag;
    dhDesc.NodeMask = 0;
    HRESULT hr = d3D.device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(&resources.descriptorHeaps[dh]));
    if(FAILED(hr)){
        std::cerr<<"Creation of descriptor heap of type: "<<dh<<" has failed!";
        return;
    }
}

void Buffer::initVertexBuffer(const DataArray::VSPArray &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    UINT64 dataSize = 0;
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
    for(UINT i = 0; i < data.count; i++) //Looping through the 'size' member of the struct.
        dataSize += data.arr[i].size; //Adding the size to get the size for the heap.
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
    resources.buffers[VERTEX_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(mappedData)); //Mapping the vertex buffer resource to the GPU memory.
    UINT8* currentPtr = mappedData; //Current pointer in the GPU.
    for(UINT i = 0; i < data.count; i++){ //Going through each pointer to different vertex arrays.
        memcpy(mappedData, data.arr[i].data, data.arr[i].size); //Copying each, which is of unique sizes.
        currentPtr += data.arr[i].size; //Moving the pointer in the GPU to a new freed space.
    }
    resources.buffers[VERTEX_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
    //we craete views here as well yipee.
    D3D12_VERTEX_BUFFER_VIEW vbView = {}; //I have to store these views as well... HMM
    vbView.StrideInBytes = sizeof(Vertex);
    UINT previousOffset = 0;
    resources.vbViews.resize(data.count);
    for(UINT i = 0; i < data.count; i++){
        vbView.SizeInBytes = data.arr[i].size;
        vbView.BufferLocation = resources.buffers[VERTEX_BUFFER]->GetGPUVirtualAddress() + previousOffset;
        previousOffset = data.arr[i].size;
        resources.vbViews.push_back(vbView);
    }
}

void Buffer::initIndexBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    UINT64 dataSize = 0;
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
    for(UINT i = 0; i < data.count; i++)
        dataSize += data.arr[i].size;
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
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.count; i++){
        memcpy(mappedData, data.arr[i].data, data.arr[i].size);
        currentPtr += data.arr[i].size;
    }
    resources.buffers[INDEX_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.Format = DXGI_FORMAT_R32_UINT;
    UINT previousOffset = 0;
    resources.ibViews.resize(data.count);
    for(UINT i = 0; i < data.count; i++){
        ibView.SizeInBytes = data.arr[i].size;
        ibView.BufferLocation = resources.buffers[INDEX_BUFFER]->GetGPUVirtualAddress() + previousOffset;
        previousOffset = data.arr[i].size;
        resources.ibViews.push_back(ibView);
    }
}

void Buffer::initConstantBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources){
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
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {}; //We are assuming right now that we only have one constant buffer view..
    cbvDesc.BufferLocation = resources.buffers[CONSTANT_BUFFER]->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = 256;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[SRV_CBV_UAV_DH]->GetCPUDescriptorHandleForHeapStart();
    UINT descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    handle.ptr += descriptorIncrementSize;
    d3D.device->CreateConstantBufferView(&cbvDesc,
                                         handle//Describes the CPU descriptor handle that representes the start of the heap that holds the constant buffer view. This is a pointer or handle that the CPU uses to access and write into the descriptor heap memory. When you create or update a descriptor (like a constant buffer view), the CPU needs to write the descriptor data into the heap. So you get a CPU handle to specify exactly where in the heap to write this descriptor. This handle is valid and usable only on the CPU side, allowing you to fill in descriptor information.
                                         //Simplest way to say it, we are getting the location of the first descriptor of many (if there are more than 1), in the descriptor heap. Right now it's the start of the descriptor heap, now imagine we wanted another descriptor of the same type in the heap, to create the 'view', we would have to increment this handle to the next descriptor 'slot' in the descriptor heap. When we call functions like CreateShaderResourceView or CreateConstantBufferView, we pass a CPU descriptor handle to specify exactly where in the heap the new descriptor should be written. Later, when binding descriptors to the GPU pipeline, we use GPU descriptor handles to tell the GPU where to find these descriptors during execution.
                                         //It's because the descriptor heap is in the CPU accessible memory while what it's ponting to is in the GPU, which is why descriptors/views use gpuvirtualaddress for pointing to a buffer that is committed to the GPU memory, but we use cpuhandle for the memory for the descriptors themselves.
                                         );
    resources.descriptorInHeapCount[SRV_CBV_UAV_DH]++;
}

void Buffer::initIntermeddiateBuffer(const DataArray::PSPArray &data, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    UINT dataSize = 0;
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense.
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
    for(UINT i = 0; i < data.count; i++)
        dataSize += data.arr[i].size;
    desc.Width = dataSize;
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[INTERMEDDIATE_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for intermeddiate buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[INTERMEDDIATE_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(mappedData));
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.count; i++){
        memcpy(mappedData, data.arr[i].data, data.arr[i].size);
        currentPtr += data.arr[i].size;
    }
    UINT rmdr = 1;
    resources.buffers[INTERMEDDIATE_BUFFER]->Unmap(0, nullptr);
    if (dataSize > 65536){
        rmdr = dataSize / 65536;
        rmdr++;
    }
    resources.heapOffsets[UPLOAD_HEAP] += rmdr * 65536;
}

void Buffer::init2DTexture(void* data, UINT width, UINT height, UINT nrChannels, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources){
    //Thinking if I should upload all textures at once and just create different views?? I think so cause I will hav eto do it regardless.
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {}; //Multisampling, mandatory to fill even for resources that don't make sense
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;
    desc.SampleDesc = sampleDesc;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE; //Since the flag doesn't state to allow depth/stencil or render target, we must set the clear value to nullptr.
    D3D12_RESOURCE_ALLOCATION_INFO info = d3D.device->GetResourceAllocationInfo(0, 1, &desc);
    UINT64 sizeForHeap = info.SizeInBytes;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    resources.texture2Ds.push_back(resource);
    HRESULT hr = d3D.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.texture2Ds.back()));
    if(FAILED(hr)){
        std::cerr<<"Texture: "<<resources.texture2Ds.size()<<" upload failed!";
        return;
    }
    D3D12_TEXTURE_COPY_LOCATION copyDestLocation = {};
    copyDestLocation.pResource = resources.texture2Ds.back().Get();
    copyDestLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDestLocation.SubresourceIndex = 0; //If there's 3D array then the slices.

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint; //The footprint of a sub-resource, so everything about it.
    UINT rows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    //Gets a resource layout that can be copied. Helps the app fill-in D3D12_PLACED_SUBRESOURCE_FOOTPRINT and D3D12_SUBRESOURCE_FOOTPRINT when suballocating space in upload heaps.
    d3D.device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowSizeInBytes, &totalBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> tempUploadBuffer;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Buffer resource, and not a texture resource
    desc.Alignment = 0;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; //Buffers are basically typeless
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Width = totalBytes;
    hr = d3D.device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ //Reason this and not copy_source state is because all buffers in the upload heap have to be in the generic read state during creation.
    , nullptr, IID_PPV_ARGS(&tempUploadBuffer));
    if(FAILED(hr)){
        std::cerr<<"The temporary upload buffer for texture "<<resources.texture2Ds.size()<<" createion failed!";
        return;
    }
    UINT8* mappedData = nullptr;
    tempUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    unsigned char* destIB = static_cast<unsigned char*>(mappedData);
    unsigned char* sourceT = static_cast<unsigned char*>(data);
    //The thing is that D3D12 requires padding for texture data.
    //Now, while the GPU doesn't know that this buffer has texture data, when we copy it, we are copying it in the form of the footprint we 'studied' from the textureDesc. So, D3D12, when copying, will copy the data, with the padding and everything necessary regarding the footprint. So we will need to pad the data while we do a memcpy to begin with.
    //For 'padding', we're just leaving the data empty in the GPU side.
    for (UINT row = 0; row < height; row++){
        memcpy(destIB + (row * footprint.Footprint.RowPitch), sourceT + (row * width * nrChannels), width * nrChannels);
    }
    //memcpy(mappedData, textureData, totalBytes);
    tempUploadBuffer->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION copySrcLocation = {};
    copySrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrcLocation.pResource = tempUploadBuffer.Get();
    copySrcLocation.PlacedFootprint = footprint;
    copySrcLocation.SubresourceIndex = 0;

    UINT fenceValue = 1;

    D3D12_RESOURCE_BARRIER copyToPSResourceBarrier = {}; //For changing from copy state to pixel shader resource state.
    copyToPSResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    copyToPSResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    D3D12_RESOURCE_TRANSITION_BARRIER copyToPSTransitionBarrier = {};
    copyToPSTransitionBarrier.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    copyToPSTransitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    copyToPSTransitionBarrier.pResource = resources.texture2Ds.back().Get();
    copyToPSTransitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    copyToPSResourceBarrier.Transition = copyToPSTransitionBarrier;
    d3D.commandLists[RENDER]->CopyTextureRegion(&copyDestLocation, 0, 0, 0, &copySrcLocation, nullptr);
    d3D.commandLists[RENDER]->ResourceBarrier(1, &copyToPSResourceBarrier);
    d3D.commandLists[RENDER]->Close();
    hr = d3D.commandQueue->Signal(d3D.fence.Get(), fenceValue); //Will tell the GPU after finishing all the currently queued commands, set the fence to the fence value. It will se the GPU fence value to the fence value variable provided through this method. Basically saying hey commandqueue, after you finish, at this address (given by the fence), put the value (given by fencevalue).
    if(FAILED(hr)){
        std::cerr<<"Command queue signal failed before the rendering loop!";
        return;
    }
    if (d3D.fence->GetCompletedValue() < fenceValue) { //If the value in the fence object is less than the 'fence value' variable, then that means that the command queue has not finished yet, as the command queue will set the value in the fence object to be fence value after finishing.
        hr = d3D.fence->SetEventOnCompletion(fenceValue, d3D.fenceEvent); //Basically will say that if the fence's value is the value provided through the 'fencevalue' variable, then fenceevent should be "signaled". "Signaling" in this context means that the fence object will notify a event handle if that happens.
        if(FAILED(hr)){
            std::cerr<<"Failed to set fence event on completion before the rendering loop!";
            return;
        }
        WaitForSingleObject(d3D.fenceEvent, INFINITE); //Basically saying that the CPU thread should wait an 'INFINITE' amount of time until event has been signaled. The thread will do other things, and the reason we don't just run an infinite loop instead, checking and seeing if the fence value is reached or not, is because the thread will not be free for other tasks + the memory will keep being accessed over and over again, which may not be bad since cache, but it'll waste the other 64-8 bytes in the cache. "Until you're signaled do this, which is wait in this case".
    }
    hr = d3D.commandAllocators[PRIMARY]->Reset();
    if(FAILED(hr)){
            std::cerr<<"Command allocator is reset during copying texture resource, and it couldn't reset!";
            return;
        }
    hr = d3D.commandLists[RENDER]->Reset(d3D.commandAllocators[PRIMARY].Get(), nullptr);
    if(FAILED(hr)){
        std::cerr<<"Command List is reset during copying texture resource, and it couldn't reset!";
        return;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC texView = {};
    texView.Format = format;
    texView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    texView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    D3D12_TEX2D_SRV texSRV = {};
    texSRV.MipLevels = 1;
    texSRV.MostDetailedMip = 0;
    texSRV.PlaneSlice = 0;
    texSRV.ResourceMinLODClamp = 0.0f;
    texView.Texture2D = texSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[SRV_CBV_UAV_DH]->GetCPUDescriptorHandleForHeapStart();
    UINT descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    handle.ptr += descriptorIncrementSize;
    d3D.device->CreateShaderResourceView(resources.texture2Ds.back().Get(), &texView, handle);
}
