#pragma once
#include "SDL3/SDL_video.h"
#include "utils.h"
#include "importer.h"
#include "resource.h"
#include "camera.h"
#include <cstdint>
#include <wrl/client.h>

class Engine{
private:
    UINT windowWidth = 1000;
    UINT windowHeight = 1000;
    SDL_Window* window;
    HRESULT hr;
    HWND hwnd; //Window handler.
    D3DGlobal d3D;
    D3DResources resource;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    void PrintDebugMessages();
    UINT renderTextureOffset; //Offset in the texture2D array of 'd3d12 resource', which offset the render texture is in.
    UINT backBufferOffset; //Offset in the descriptorheaps for rtvs, where the descriptorheap of the the back buffer start. Atleast that's how these 2 are used right now. I forgot my own design decision.
    UINT constantBufferOffset;
    UINT fenceValue = 1;
    char* perFrameConstantData;
    char* perModelConstantData;
    std::vector<uint32_t> modelIndices;
    Camera* camera;
//    SDL_WINDOW* window = nullptr;
public:
    Engine();
    void prepareData();
    void render();
    //Swapchain is used for presenting, getting backbfuffer index, and for getting the buffer for the backbuffer render target resources to fill their values.
    //allocator's are reset using the clists as well as by themselves.
    //Command lists are the hardest things because they are used for literally everything.
};


