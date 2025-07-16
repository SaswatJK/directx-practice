#include "../include/graphics.h"
#include "D3D12/d3d12.h"
#include "utils.h"
#include <combaseapi.h>
#include <dxgi.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

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

}

void Engine::initializeHeaps(){
    
};
