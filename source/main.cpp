#include <D3D12/D3dx12.h>
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgiformat.h>
#include <iostream>
#include "SDL3/SDL.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_properties.h"
#include "SDL3/SDL_video.h"
#include "glm/glm.hpp"
#include <wrl.h>
#include <wrl/client.h>

struct Vertex{
  float position[3];
  float color[4];
};

int main(){
if (SDL_Init(SDL_INIT_VIDEO) == false) {
    //SDL_Log("SDL_Init failed: %s", SDL_GetError());
    std::cout<<SDL_GetError();
    SDL_Log("Joe amamamamammaamam");
    return 1;
}

Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

Microsoft::WRL::ComPtr<ID3D12Device> device;
Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;

for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &hardwareAdapter); ++i) {
  if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    break;
    }
  }
  if(!device)
  std::cout<<"The device failed to create for d3d12 \n";

Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

unsigned int width = 800;
unsigned int height = 800;

Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
swapChainDesc.BufferCount = 2;
swapChainDesc.Width = width;
swapChainDesc.Height = height;
swapChainDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
swapChainDesc.SampleDesc.Count = 1;

Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;

SDL_Window* window = SDL_CreateWindow("SDL3 + DirectX12", width, height, 0);

HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

if(!hwnd){
  std::cerr<<"The hwnd from sdl is failed:"<<SDL_GetError();
}

std::cout<<"The hwnd value is:"<<hwnd;

hr = factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain);

if(FAILED(hr)){
  std::cerr<<"failed to create swap chain";
  return 1;
}

hr = tempSwapChain.As(&swapChain);

//render texture descriptior heap

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
rtvHeapDesc.NumDescriptors = 2; //Double buffering
rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[2];
CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

for(UINT i = 0; i < 2; i++){
  hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
  device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
  rtvHandle.Offset(1, rtvDescriptorSize);
}


if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    return 1;
}

SDL_Delay(3000);  // Show the window for 3 seconds
SDL_DestroyWindow(window);
SDL_Quit();
return 0;
}
