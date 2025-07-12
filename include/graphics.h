#pragma once
#include "SDL3/SDL_video.h"
#include "utils.h"
#include "resource.h"
#include <wrl/client.h>

DXGI_SAMPLE_DESC sampleDesc = {
    1, //Number of multisamples per pixel
    0 //The image quality level. The higher the quality, the lower the performance. The valid range is between zero and one less than the level returned by ID3D10Device::CheckMultisampleQualityLevels for Direct3D 10 or ID3D11Device::CheckMultisampleQualityLevels for Direct3D 11.
};


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
//    SDL_WINDOW* window = nullptr;
public:
    Engine();
    void prepareData();
    void initializeHeaps();
    void initializeResourceWithView();
    //Swapchain is used for presenting, getting backbfuffer index, and for getting the buffer for the backbuffer render target resources to fill their values.
    //allocator's are reset using the clists as well as by themselves.
    //Command lists are the hardest things because they are used for literally everything.
};


