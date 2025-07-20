#pragma once
#include "SDL3/SDL_video.h"
#include "utils.h"
#include "resource.h"
#include <wrl/client.h>

class Engine{
private:
    UINT windowWidth = 1000;
    UINT windowHeight = 1000;
    SDL_Window* window;
    HRESULT hr;
    HWND hwnd;
    D3DGlobal d3D;
    D3DResources resource;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    void PrintDebugMessages();
    UINT renderTextureOffset;
    UINT backBufferOffset;
    UINT fenceValue = 1;
//    SDL_WINDOW* window = nullptr;
public:
    Engine();
    void prepareData();
    void render();
    //Swapchain is used for presenting, getting backbfuffer index, and for getting the buffer for the backbuffer render target resources to fill their values.
    //allocator's are reset using the clists as well as by themselves.
    //Command lists are the hardest things because they are used for literally everything.
};


