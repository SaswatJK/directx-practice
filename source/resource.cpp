#include "../include/resource.h"
#include "D3D12/d3d12.h"
#include "D3D12/dxgicommon.h"
#include "utils.h"
#include <climits>
#include <combaseapi.h>
#include <intsafe.h>
#include <minwindef.h>
#include <wrl/client.h>

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

UINT getVSPDataSize(const DataArray &data){
    UINT dataSize = 0;
    for(UINT i = 0; i < data.VSPArray.count; i++) //Looping through the 'size' member of the struct.
        dataSize += data.VSPArray.arr[i].size; //Adding the size to get the size for the heap.
    return dataSize;
}

UINT getPSPDataSize(const DataArray &data){
    UINT dataSize = 0;
    for(UINT i = 0; i < data.PSPArray.count; i++)
        dataSize += data.PSPArray.arr[i].size;
    return dataSize;
}

void Heap::createHeap(UINT size, heapInfo heap, const D3DGlobal &d3D, D3DResources &resources){
    UINT heapSize = size;
    UINT heaps = heapSize/65536 + 1;
    D3D12_HEAP_DESC desc;
    switch(heap) {
        case UPLOAD_HEAP:
            desc.Properties = uploadHeapProperties;
            if(heapSize < 65536 * 4) // The 4 bfufers that will be placed in the upload heap
                heapSize = 65536 * 4;
            else
                heapSize = 65536 * heaps;
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
    dhDesc.NumDescriptors = 100;
    dhDesc.Flags = flag;
    dhDesc.NodeMask = 0;
    HRESULT hr = d3D.device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(&resources.descriptorHeaps[dh]));
    if(FAILED(hr)){
        std::cerr<<"Creation of descriptor heap of type: "<<dh<<" has failed!";
        return;
    }
}

void Resource::initVertexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources){
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
    dataSize = getVSPDataSize(data);
    desc.Width = dataSize;
    //Committed resource is the easiest way to create resource since we don't need to do heap management ourselves.
    //I wanted to use ID3D12Resource2 but it's so unimportant that these guys didn't even document it properly, there's no links to new features or to the old interface it has inherited from LMFAO. CreateCommittedResource3 uses layout rather than states for barriers and stuff, which should give more control for the memory of both GPU, CPU, and the access of either/or memory from either/or physical hardware (host or device), so does CreatePlacedResource2, while CreatePlacedResource1 looks for desc_1 rather than desc for resources.
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_COMMON, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[VERTEX_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for vertex buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[VERTEX_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)); //Mapping the vertex buffer resource to the GPU memory.
    UINT8* currentPtr = mappedData; //Current pointer in the GPU.
    for(UINT i = 0; i < data.VSPArray.count; i++){ //Going through each pointer to different vertex arrays.
        memcpy(currentPtr, data.VSPArray.arr[i].data, data.VSPArray.arr[i].size); //Copying each, which is of unique sizes.
        currentPtr += data.VSPArray.arr[i].size; //Moving the pointer in the GPU to a new freed space.
    }
    resources.buffers[VERTEX_BUFFER]->Unmap(0, nullptr);
    uint32_t newOffset = desc.Width / 65536  + 1;
    resources.heapOffsets[UPLOAD_HEAP] += (newOffset * 65536);
    //we craete views here as well yipee.
    D3D12_VERTEX_BUFFER_VIEW vbView = {}; //I have to store these views as well... HMM
    vbView.StrideInBytes = sizeof(Vertex);
    UINT previousOffset = 0;
    resources.vbViews.resize(data.VSPArray.count);
    for(UINT i = 0; i < data.VSPArray.count; i++){
        vbView.SizeInBytes = data.VSPArray.arr[i].size;
        vbView.BufferLocation = resources.buffers[VERTEX_BUFFER]->GetGPUVirtualAddress() + previousOffset;
        previousOffset += data.VSPArray.arr[i].size;
        resources.vbViews[i] = vbView;
    }
}

void Resource::initIndexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources){
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
    dataSize = getPSPDataSize(data);
    desc.Width = dataSize;
    HRESULT hr = d3D.device->CreatePlacedResource(resources.heaps[UPLOAD_HEAP].Get(),
                                                  resources.heapOffsets[UPLOAD_HEAP], &desc,
                                                  D3D12_RESOURCE_STATE_COMMON, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[INDEX_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for index buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[INDEX_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.PSPArray.count; i++){
        memcpy(currentPtr, data.PSPArray.arr[i].data, data.PSPArray.arr[i].size);
        currentPtr += data.PSPArray.arr[i].size;
    }
    resources.buffers[INDEX_BUFFER]->Unmap(0, nullptr);
    uint32_t newOffset = desc.Width / 65536  + 1;
    resources.heapOffsets[UPLOAD_HEAP] += (newOffset * 65536);
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.Format = DXGI_FORMAT_R32_UINT;
    UINT previousOffset = 0;
    resources.ibViews.resize(data.PSPArray.count);
    for(UINT i = 0; i < data.PSPArray.count; i++){
        ibView.SizeInBytes = data.PSPArray.arr[i].size;
        ibView.BufferLocation = resources.buffers[INDEX_BUFFER]->GetGPUVirtualAddress() + previousOffset;
        previousOffset += data.PSPArray.arr[i].size;
        resources.ibViews[i] = ibView;
    }
}

void Resource::initConstantBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources){
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
                                                  D3D12_RESOURCE_STATE_COMMON, //D3D12_RESOURCE_STATE_GENERIC_READ is a logically OR'd combination of other read-state bits. This is the required starting state for an upload heap. Application should generally avoid transitioning to D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER shoudl be used for subresource while the GPU is accessing vertex or constant buffer, this is GPU only at that state. A subresourece is a portion of a resourece, mip level, array slice, plane of a texture etc, each subresource can be in a different state at any given time.
                                                  nullptr, //optimized clear value
                                                  IID_PPV_ARGS(&resources.buffers[CONSTANT_BUFFER]));
    if(FAILED(hr)){
    std::cerr<<"Error placement for constant buffer failed!";
    return;
    };
    UINT8* mappedData = nullptr;
    resources.buffers[CONSTANT_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.PSPArray.count; i++){
        memcpy(currentPtr, data.PSPArray.arr[i].data, data.PSPArray.arr[i].size);
        currentPtr += data.PSPArray.arr[i].size;
    }
    float* f = reinterpret_cast<float*>(mappedData);
    for (int i = 0; i < 16*4; ++i) {
        if (i % 4 == 0) printf("\nvec%d: ", i/4);
        printf("%f ", f[i]);
    }
    printf("\n");
    resources.buffers[CONSTANT_BUFFER]->Unmap(0, nullptr);
    resources.heapOffsets[UPLOAD_HEAP] += 65536;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {}; //We are assuming right now that we only have one constant buffer view..
    cbvDesc.BufferLocation = resources.buffers[CONSTANT_BUFFER]->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = 256;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[SRV_CBV_UAV_DH]->GetCPUDescriptorHandleForHeapStart();
    UINT currentDescriptorHeapOffset = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    handle.ptr += currentDescriptorHeapOffset;
    d3D.device->CreateConstantBufferView(&cbvDesc,
                                         handle//Describes the CPU descriptor handle that representes the start of the heap that holds the constant buffer view. This is a pointer or handle that the CPU uses to access and write into the descriptor heap memory. When you create or update a descriptor (like a constant buffer view), the CPU needs to write the descriptor data into the heap. So you get a CPU handle to specify exactly where in the heap to write this descriptor. This handle is valid and usable only on the CPU side, allowing you to fill in descriptor information.
                                         //Simplest way to say it, we are getting the location of the first descriptor of many (if there are more than 1), in the descriptor heap. Right now it's the start of the descriptor heap, now imagine we wanted another descriptor of the same type in the heap, to create the 'view', we would have to increment this handle to the next descriptor 'slot' in the descriptor heap. When we call functions like CreateShaderResourceView or CreateConstantBufferView, we pass a CPU descriptor handle to specify exactly where in the heap the new descriptor should be written. Later, when binding descriptors to the GPU pipeline, we use GPU descriptor handles to tell the GPU where to find these descriptors during execution.
                                         //It's because the descriptor heap is in the CPU accessible memory while what it's ponting to is in the GPU, which is why descriptors/views use gpuvirtualaddress for pointing to a buffer that is committed to the GPU memory, but we use cpuhandle for the memory for the descriptors themselves.
                                         );
    resources.eachDescriptorCount[VIEW_CBV] = resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    resources.descriptorInHeapCount[SRV_CBV_UAV_DH]++;
}

void Resource::updateConstantBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources){
    UINT8* mappedData = nullptr;
    resources.buffers[CONSTANT_BUFFER]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    UINT8* currentPtr = mappedData;
    for(UINT i = 0; i < data.PSPArray.count; i++){
        memcpy(currentPtr, data.PSPArray.arr[i].data, data.PSPArray.arr[i].size);
        currentPtr += data.PSPArray.arr[i].size;
    }
    float* f = reinterpret_cast<float*>(mappedData);
    for (int i = 0; i < 16*4; ++i) {
        if (i % 4 == 0) printf("\nvec%d: ", i/4);
        printf("%f ", f[i]);
    }
    printf("\n");
    resources.buffers[CONSTANT_BUFFER]->Unmap(0, nullptr);
}


void Resource::createBackBuffers(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources){
    Microsoft::WRL::ComPtr<ID3D12Resource> resource0;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource1;
    UINT bb0Pos = resources.texture2Ds.size();
    resources.texture2Ds.push_back(resource0);
    resources.texture2Ds.push_back(resource1);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[RTV_DH]->GetCPUDescriptorHandleForHeapStart(); //Start of the descriptor heap of that type.
    UINT descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //Increment size for each of the descriptor. The function returns the size of the handle increment for the given type of descriptor heap, including any necessary padding
    UINT currentDescriptorHeapOffset = descriptorIncrementSize * resources.descriptorInHeapCount[RTV_DH]; //We are getting the offset to the current descriptor.
    handle.ptr += currentDescriptorHeapOffset;
    for (UINT i = 0; i < 2; i++){
        d3D.xSwapChain->GetBuffer(i, IID_PPV_ARGS(&resources.texture2Ds[bb0Pos + i]));//Basically since our render textures have not been described till now, we use this to get the swapchain buffer, with it's properties and everything and we give it to the render texture. Calling GetBuffer returns a fully described resource created and managed by the swap chain.
        d3D.device->CreateRenderTargetView(resources.texture2Ds[bb0Pos + i].Get(), //Pointer to the resourece, and not the Com object.
                                           nullptr, //We don't need to make a render_target_view_desc, it will inherit the importnat values like size and format and fill in defaults
                                           handle);
        handle.ptr += descriptorIncrementSize;
        resources.eachDescriptorCount[VIEW_RTV] = resources.descriptorInHeapCount[RTV_DH];
        resources.descriptorInHeapCount[RTV_DH]++;
    }
}

void Resource::createGPUTexture(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_RESOURCE_DESC desc = {};
    DXGI_SAMPLE_DESC sampleDesc = {};
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
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    resources.texture2Ds.push_back(resource);
    D3D12_CLEAR_VALUE optimizedClearValue = {};
    optimizedClearValue.Format = format;
    optimizedClearValue.Color[0] = 0.0f;
    optimizedClearValue.Color[1] = 0.0f;
    optimizedClearValue.Color[2] = 0.0f;
    optimizedClearValue.Color[3] = 1.0f;
    HRESULT hr = d3D.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &optimizedClearValue, IID_PPV_ARGS(&resources.texture2Ds.back()));
    if(FAILED(hr)){
        std::cerr<<"Texture: "<<resources.texture2Ds.size()<<" upload failed!";
        return;
    }
    //I am making this in pixel shader resource state because every loop, it will become pixel shader resource later, and I will have to convert that to render target, so basically I can use the same barrier.
    //I am thinking of using another wrapper in graphics.cpp to wrap around this to initialize all the render textcures. I could do it here too, but I can't make a desicison rn. I also want to say that I will create another create sampler function if I have to. But the graphics.cpp will create one sampler descriptor heap and that's enough.

    D3D12_TEX2D_RTV tex2Drtv = {};
    tex2Drtv.MipSlice = 0;
    tex2Drtv.PlaneSlice = 0;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Texture2D = tex2Drtv;
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[RTV_DH]->GetCPUDescriptorHandleForHeapStart(); //Start of the descriptor heap of that type.
    UINT descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //Increment size for each of the descriptor. The function returns the size of the handle increment for the given type of descriptor heap, including any necessary padding
    UINT currentDescriptorHeapOffset = descriptorIncrementSize * resources.descriptorInHeapCount[RTV_DH]; //We are getting the offset to the current descriptor.
    handle.ptr += currentDescriptorHeapOffset;
    d3D.device->CreateRenderTargetView(resources.texture2Ds.back().Get(), &rtvDesc, handle);
    resources.descriptorInHeapCount[RTV_DH]++;

    D3D12_TEX2D_SRV tex2Dsrv = {};
    tex2Dsrv.MipLevels = 1;
    tex2Dsrv.MostDetailedMip = 0;
    tex2Dsrv.PlaneSlice = 0;
    tex2Dsrv.ResourceMinLODClamp = 0.0f;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Texture2D = tex2Dsrv;
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

    handle = resources.descriptorHeaps[SRV_CBV_UAV_DH]->GetCPUDescriptorHandleForHeapStart();
    descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    currentDescriptorHeapOffset = descriptorIncrementSize * resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    handle.ptr += currentDescriptorHeapOffset;
    d3D.device->CreateShaderResourceView(resources.texture2Ds.back().Get(), &srvDesc, handle);
    resources.eachDescriptorCount[VIEW_SRV] = resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    resources.descriptorInHeapCount[SRV_CBV_UAV_DH]++;
}

void Resource::init2DTexture(void* data, UINT width, UINT height, UINT nrChannels, DXGI_FORMAT format, UINT fenceValue,const D3DGlobal &d3D, D3DResources &resources){
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
    hr = d3D.device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON//Reason this and not copy_source state is because all buffers in the upload heap have to be in the generic read state during creation.
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

    D3D12_RESOURCE_BARRIER copyToPSResourceBarrier = {}; //For changing from copy state to pixel shader resource state.
    copyToPSResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    copyToPSResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    D3D12_RESOURCE_TRANSITION_BARRIER copyToPSTransitionBarrier = {};
    copyToPSTransitionBarrier.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    copyToPSTransitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    copyToPSTransitionBarrier.pResource = resources.texture2Ds.back().Get();
    copyToPSTransitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    copyToPSResourceBarrier.Transition = copyToPSTransitionBarrier;
    hr = d3D.commandLists[RENDER]->Close();
    hr = d3D.commandAllocators[PRIMARY]->Reset();
    if(FAILED(hr)){
            std::cerr<<"Command allocator is reset before copying the texture resource, and it couldn't reset!";
            return;
        }
    hr = d3D.commandLists[RENDER]->Reset(d3D.commandAllocators[PRIMARY].Get(), nullptr);
    if(FAILED(hr)){
        std::cerr<<"Command List is reset before copying the texture resource, and it couldn't reset!";
        return;
    }
    d3D.commandLists[RENDER]->CopyTextureRegion(&copyDestLocation, 0, 0, 0, &copySrcLocation, nullptr);
    d3D.commandLists[RENDER]->ResourceBarrier(1, &copyToPSResourceBarrier);
    d3D.commandLists[RENDER]->Close();
    ID3D12CommandList* lists[] = { d3D.commandLists[RENDER].Get() };
    d3D.commandQueue->ExecuteCommandLists(_countof(lists), lists);
    hr = d3D.commandQueue->Signal(d3D.fence.Get(), fenceValue); //Will tell the GPU after finishing all the currently queued commands, set the fence to the fence value. It will set the GPU fence value to the fence value variable provided through this method. Basically saying hey commandqueue, after you finish, at this address (given by the fence), put the value (given by fencevalue).
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
    UINT currentDescriptorHeapOffset = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    handle.ptr += currentDescriptorHeapOffset;
    d3D.device->CreateShaderResourceView(resources.texture2Ds.back().Get(), &texView, handle);
    //tempUploadBuffer->Release();
    resources.eachDescriptorCount[VIEW_SRV] = resources.descriptorInHeapCount[SRV_CBV_UAV_DH];
    resources.descriptorInHeapCount[SRV_CBV_UAV_DH]++;
}

void Resource::createSimpleSampler(const D3DGlobal &d3D, D3DResources &resources){
    D3D12_SAMPLER_DESC desc = {};
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.descriptorHeaps[SAMPLER_DH]->GetCPUDescriptorHandleForHeapStart(); //Start of the descriptor heap of that type.
    UINT descriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); //Increment size for each of the descriptor. The function returns the size of the handle increment for the given type of descriptor heap, including any necessary padding.
    UINT currentDescriptorHeapOffset = descriptorIncrementSize * resources.descriptorInHeapCount[SAMPLER_DH]; //We are getting the offset to the current descriptor.
    handle.ptr += currentDescriptorHeapOffset;
    d3D.device->CreateSampler(&desc, handle);
    resources.descriptorInHeapCount[SAMPLER_DH]++;
}

void RootSignature::createBindlessRootSignature(const D3DGlobal &d3D, D3DResources &resources){
    //Descriptor range defines a contiguous sequence of resource descriptors of a specific type within a descriptor table. Like the 'width' of a slice, that is descriptor table, of a data, that is descriptor heap.
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 100;
    srvRange.BaseShaderRegister = 0;  // t0, t1, t2, ... in shader
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = 100;
    cbvRange.BaseShaderRegister = 0;
    cbvRange.RegisterSpace = 0; // Different register space to avoid conflicts
    cbvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE samplerRange = {};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 100;
    samplerRange.BaseShaderRegister = 0; // s0, s1, s2, ... in shader
    samplerRange.RegisterSpace = 0;
    samplerRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_DESCRIPTOR_TABLE srvTable = {};
    srvTable.NumDescriptorRanges = 1;
    srvTable.pDescriptorRanges = &srvRange;

    D3D12_ROOT_DESCRIPTOR_TABLE cbvTable = {};
    cbvTable.NumDescriptorRanges = 1;
    cbvTable.pDescriptorRanges = &cbvRange;

    D3D12_ROOT_DESCRIPTOR_TABLE samplerTable = {};
    samplerTable.NumDescriptorRanges = 1;
    samplerTable.pDescriptorRanges = &samplerRange;

    //Root parameters define how resoureces are bound to the pipeline. We right now use descriptor tables, which are just pointers to a fixed range of data in a descriptor heap. One root parameter can onyl contain one descriptor table, so only one slice, which is why more than one may be required.
    D3D12_ROOT_PARAMETER rootParams[3] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[0].DescriptorTable = srvTable;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[1].DescriptorTable = cbvTable;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[2].DescriptorTable = samplerTable;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 3; //Can have more than one descriptor table

    rsDesc.pParameters = rootParams; //Pointer to an array of descriptor tables if they are multiple of them.
    rsDesc.NumStaticSamplers = 0;
    rsDesc.pStaticSamplers = nullptr;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob; //Used to return data of an arbitrary length
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    // The root signature is the definition of an arbitrarily arranged collection of descriptor tables (including their layout), root constants and root descriptors. It basically describes the layout of resource used by the pipeline.
    //'Serializing' a root signature means converting a root signature description from the desc structures, to a GPU readable state. We need to serialize into blobs the root signature, which talks about textures and buffers and where to find them and what not, and shaders, so the GPU can understand it.
    //This description is a CPU-side data structure describing how our shaders will access resources. To create a GPU-usable root signature (ID3D12RootSignature), our must serialize this description into a binary format.
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &error);
    if (FAILED(hr)) {
        std::cerr<<"Serialising root signature failure";
        return;
    }
    //Reminder for the future. I dont' need to make a root signature for index or vertex buffers. I only need to copy it into GPU memory, through memcpy and mapping to get the pointer to the GPU memory, and I need to call IASetVertexBuffers, IASetIndexBuffer for it. So just make a d3d12 resource, map cpu resource to it, then call the function! I have to initailize the resource through resource_desc ofcourse, but it's not as crazy as the SRV or CBV ones.

    hr = d3D.device->CreateRootSignature(
                     0, // 0 for a single GPU, or else multipel GPU then have to decide which 'node' the signature needs to be applied to.
                     signatureBlob->GetBufferPointer(), //Pointer to the source data for the serialized signature.
                     signatureBlob->GetBufferSize(), //Size of the blob
                     IID_PPV_ARGS(&resources.rootSignature));
    if(FAILED(hr)){
        std::cerr<<"Root signature creation failed!";
        return;
    }
};

void PipelineState::createGraphicsPSO(psoInfo info, const Shader &shader, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources){
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    //Will be using the same dxgi_sample_desc that I created for swap chain.
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
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
    psoDesc.BlendState = opaqueBlendDesc;
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
    D3D12_INPUT_LAYOUT_DESC simpleInputLayoutDesc = {};
    D3D12_INPUT_ELEMENT_DESC simpleInputElementDesc[] = {{
    "POSITION", //Semantic Name
    0, //Semantic index
    DXGI_FORMAT_R32G32B32A32_FLOAT, //format of the input, since we're using 4 byte floats in the CPU, we might as well use that in the GPU.
    0, //Input slot. The IA stage has n input slots, which are designed to accommodate up to n vertex buffers that provide input data. Each vertex buffer must be assigned to a different slot
    0, //Offset in bytes to this element from the start of the vertex.
    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, //A value that identifies the input data class for a single input slot.
    0 //The number of instances to draw using the same per-instance data before advancing in the buffer by one element. If it's per vertex data, then set this value to 0.
    },
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    simpleInputLayoutDesc.pInputElementDescs = simpleInputElementDesc;
    simpleInputLayoutDesc.NumElements = _countof(simpleInputElementDesc); //Only vertex right now.
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX; //The sample mask for the blend state. Use in Multisampling to selectively enable or disable certain/specific samples.
    psoDesc.RasterizerState = simpleRasterizerDesc;
    psoDesc.InputLayout = simpleInputLayoutDesc;
    //psoDesc.IBStripCutValue, we are not filling this cause we don't have an index buffer.
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = format;
    psoDesc.NodeMask = 0; //Remember just like the 'NodeMask' in the createrootsignature method, this is for multiGPU systems where we are using node masks to choose between GPUs.
    psoDesc.pRootSignature = resources.rootSignature.Get();
    SimpleShaderByteCode byteCode = shader.getShaderByteCode();
    if (byteCode.vsByteCode.pShaderBytecode == nullptr || byteCode.vsByteCode.BytecodeLength == 0) {
    std::cerr << "Invalid vertex shader bytecode\n";
    return;
}
    psoDesc.VS = byteCode.vsByteCode;
    psoDesc.PS = byteCode.psByteCode;
    HRESULT hr = d3D.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.pipelineStates[info]));
    if(FAILED(hr)){
        std::cerr<<"Creation of graphics pipeline state object: "<<info<<" has failed";
        return;
    }
}
