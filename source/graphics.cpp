#include "../include/graphics.h"
#include "../include/resource.h"
#include "D3D12/d3d12.h"
#include "D3D12/dxgiformat.h"
#include "shader.h"
#include "utils.h"
#include <combaseapi.h>
#include <dxgi.h>
#include <dxgi1_5.h>
#include <filesystem>
#include <wrl/client.h>

void Engine:: PrintDebugMessages() {
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

Engine::Engine(){
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("RayTracing", windowWidth, windowHeight, 0);
    Microsoft::WRL::ComPtr<IDXGIFactory2> tempFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&tempFactory));
    if(FAILED(hr)){
        std::cerr<<"Can't create a factory for dxgi";
        return;
    }
    tempFactory.As(&d3D.xFactory);
    //Since creating a SwapChain requires a commandQueue, we need to initialize a queue first.
    //Submit commands to the GPU, which are collected and then executed in bulk. We don't want the GPU to wait for the CPU or vice versa while commands are being collected.

    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //Allows all operations, whether they are graphics, compute or copy. Despite every command type being allowed, only command lists with type DIRECT are allowed to be submitted.
    d3D.device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&d3D.commandQueue));
    if(FAILED(hr)){
        std::cerr<<"Can't create command Queue using the device";
        return;
    }
    Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain; //For compatibility reasons as the createswapchainforhwnd method only takes in swapchain1
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.SampleDesc = sampleDesc;
    swapChainDesc.Width = windowWidth; //Can make it 0 during the creation of swapchain, that will automatically obtain the width from the hwnd
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //Last time I tried to change it to more, the thing kept crashing.
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; //Haven't thought of it
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    hr = d3D.xFactory->CreateSwapChainForHwnd(
        d3D.commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr, //full sreen desc for fullscreen, and not a windowed swapchain
        nullptr, //If restrict some content
        &tempSwapChain //A pointer to a variable that receives a pointer to the IDXGISwapChain interface for the swap chain that CreateSwapChain creates, that's why I don't need to use the get method for the ComPtr

    );
    if(FAILED(hr)){
        std::cerr<<"SwapChain creation failed!";
        return;
    }

    hr = tempSwapChain.As(&d3D.xSwapChain);
    if(FAILED(hr)){
        std::cerr<<"TempSwapChain to SwapChain conversion failed!";
        return;
    }
    //Command queues will execute command lists and signal a fence.
    //Command allocator. A memory pool for where a commands will be placed for the GPU to read. Through a frame from the CPU side, each command is allocated through command lists in a command allocator. Commands are recorded to the command list, when a command list opens, it starts recording commands, and when the command list closes, it stops. So if we want multiple command lists in the command allocator, we must stop the recording of one list and then only move to another. So can only use the command allcoator with multiple command lists if they're all closed of course.
    hr = d3D.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, //Specifies a command buffer that the GPU can execute. A direct command list doesn't inherit any GPU state.
                                            IID_PPV_ARGS(&d3D.commandAllocators[PRIMARY]));
    //So what does it mean to inherit state etc? Command lists set states, basically imagine we have a list of commands for a specific scene, we would set a PSO, a root signature, a vertex buffer etc for the lists to use. Now if we want to draw different types of things in different ways, call different commands in different sequences, we can make two different types of command lists. Now what we can do is we can say that it's a bundle type of command list, which means if we execute commandlistA, we need to set the 'states' of the GPU, but then when we execute commandlistB, whether it be the same commandd allocator (easier to make sense) or different, the states are 'inherited', so we don't need to set the vertex buffers, the root signature etc, however we would stil need to set the PSO.
    if(FAILED(hr)){
        std::cerr<<"Primary Command allocator creation failed!";
        return;
    }

    //Everytime a commandlist is reset, it will allocate a block of memory from the commandallocator, it will pile up. We can reset the command allocator, but if we do so, then we shouldn't use the command list and execute it with the previously allocated memory.
    hr = d3D.device->CreateCommandList(0,//Node mask, GPU(s)
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       d3D.commandAllocators[PRIMARY].Get(),
                                       nullptr,
                                       IID_PPV_ARGS(&d3D.commandLists[RENDER]));
    if(FAILED(hr)){
        std::cout<<"Triangle Command list creation failed!";
        return;
    }
    d3D.commandLists[RENDER]->Close();
    //Synchronization. Why it's important. So the GPU and CPU are both working simulatenously, however, we don't want one to overtake the other, we don't want the CPU to overtake and change the PSO before the frame has been completed by the GPU and cause errors, and we don't want the GPU to start making frames before updating uniforms and stuff by the CPU.
    //A fence is one of the most simple synchronization objects. It exists in both the CPU and the GPU. It has an internal integer counter, and can trigger events on either the GPU or the GPU when the counter reached a certain value, that we can specify.
    //This can be done to trigger an event on the CPU when a command allocator's commands have been executed fully, or we can have the GPU wait while the CPu is completing a certain operation first, or we can get the command queues (if multiple exists), on the GPU to wait upon one another.
    hr = d3D.device->CreateFence(0, //Initial fence value
                                  D3D12_FENCE_FLAG_NONE, //Flag for if shared or not. We would want shared fence for syncrhonization after multi-threading, or we can also have a flag for cross-adapter fences, we would want multiple fences for multithreading. Different threads in the CPU build different command lists and command allocators, then different fences syncrhonize the GPU as well while doing execution of said allocators.
                                  IID_PPV_ARGS(&d3D.fence));
    if(FAILED(hr)){
        std::cout<<"Fence creation failed!";
        return;
    }
    d3D.fenceEvent = CreateEvent(nullptr, //Handle cannot be inhereted by child processes.
                                    FALSE, //If this parameter is FALSE, the function creates an auto-reset event object, and the system automatically resets the event state to nonsignaled after a single waiting thread has been released.
                                    FALSE, //If this parameter is TRUE, the initial state of the event object is signaled; otherwise, it is nonsignaled.
                                    nullptr //If lpName is NULL, the event object is created without a name.
                                    );
    if(d3D.fenceEvent == nullptr){
        std::cout<<"Fence Event creation failed!";
        return;
    }
};

void Engine::prepareData(){
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
    float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f };
    float metallic[4] = { 1.0f, 0.2f, 0.0f, 1.0f };
    int textureWidth, textureHeight, nrChannels;
    std::string texturePath = "C:/Users/broia/Downloads/practicetexture.png";
    unsigned char* textureData = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &nrChannels, 0);
    if (textureData == nullptr)
        std::cout<<"Texture data can't be opened in memory!";
    VertexSizePair triAndQuad[2];
    triAndQuad[0].data = triangle;
    triAndQuad[0].size = sizeof(triangle);
    triAndQuad[1].data = quad;
    triAndQuad[1].size = sizeof(quad);
    DataArray vertexData = {};
    vertexData.VSPArray.arr = triAndQuad;
    vertexData.VSPArray.count = 2;
    PtrSizePair quadIn;
    quadIn.data = quadIndices;
    quadIn.size = sizeof(quadIndices);
    DataArray indexData = {};
    indexData.PSPArray.arr = &quadIn;
    indexData.PSPArray.count = 1;

    //Creating upload Heap.
    Heap::createHeap(0, heapInfo::UPLOAD_HEAP, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::SRV_CBV_UAV_DH, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, d3D, resource);
    //Creating the vertex and index buffer.
    Resource::initVertexBuffer(vertexData, d3D, resource);
    Resource::initIndexBuffer(indexData, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::RTV_DH, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, d3D, resource);
    //Creating the back buffers.
    Resource::createBackBuffers(windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, d3D, resource);
    //Creating the gBuffer. Turns out I am not using my default heap for anything right now... Will figure out later.
    Resource::createGPUTexture(windowWidth, windowHeight, DXGI_FORMAT_R16G16B16A16_UNORM, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::SAMPLER_DH, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, d3D, resource);
    Resource::createSimpleSampler(d3D, resource);
    Resource::init2DTexture(textureData, textureWidth, textureHeight, nrChannels, DXGI_FORMAT_R8G8B8A8_UNORM, d3D, resource);
    //Creating a bindless root singature.
    RootSignature::createBindlessRootSignature(d3D, resource);
    //Creating the first pipeline state.
    Shader renderShader("../shaders/simple.vert","../shaders/simple.pixel");
    PipelineState::createGraphicsPSO(psoInfo::RENDER_PSO, renderShader, DXGI_FORMAT_R16G16B16A16_UNORM, d3D, resource);
    Shader quadShader("../shaders/quad.vert", "../shaders/quad.pixel");
    PipelineState::createGraphicsPSO(psoInfo::PRESENT_PSO, quadShader, DXGI_FORMAT_R8G8B8A8_UNORM, d3D, resource);
}
