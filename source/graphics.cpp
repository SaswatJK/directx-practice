#include "../include/graphics.h"
#include "../include/resource.h"
#include "../include/importer.h"
#include "../include/imgui.h"
#include "../include/imgui_impl_sdl3.h"
#include "../include/imgui_impl_dx12.h"
#include "camera.h"
#include "glm/detail/qualifier.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "../include/shader.h"
#include "../include/utils.h"
#include "../include/D3D12/d3d12.h"
#include "../include/D3D12/d3dcommon.h"
#include "../include/D3D12/dxgiformat.h"
#include "../include/SDL3/SDL_events.h"
#include "../include/SDL3/SDL_video.h"
#include <combaseapi.h>
#include <cstdlib>
#include <cstring>
#include <dxgi.h>
#include <dxgi1_5.h>
#include <windows.h>
#include <glm/gtc/matrix_access.hpp>

DXGI_SAMPLE_DESC sampleDesc = {
    1, //Number of multisamples per pixel
    0 //The image quality level. The higher the quality, the lower the performance. The valid range is between zero and one less than the level returned by ID3D10Device::CheckMultisampleQualityLevels for Direct3D 10 or ID3D11Device::CheckMultisampleQualityLevels for Direct3D 11.
};

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
    Microsoft::WRL::ComPtr<IDXGIFactory2> tempFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&tempFactory));
    if(FAILED(hr)){
        std::cerr<<"Can't create a factory for dxgi";
        return;
    }
    tempFactory.As(&d3D.xFactory);
    //Since creating a SwapChain requires a commandQueue, we need to initialize a queue first.
    //Submit commands to the GPU, which are collected and then executed in bulk. We don't want the GPU to wait for the CPU or vice versa while commands are being collected.

    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
        //if (SUCCEEDED(debugController.As(&debugController1))) debugController1->SetEnableGPUBasedValidation(TRUE);
    }
    for (uint32_t adapterIndex = 0; true; ++adapterIndex) {
        if (d3D.xFactory->EnumAdapters1(adapterIndex, &d3D.xAdapter) == DXGI_ERROR_NOT_FOUND) {
          break;
        }

        DXGI_ADAPTER_DESC1 desc;
        d3D.xAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
          continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(d3D.xAdapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr))) {
          break;
        }
      }

        hr = (D3D12CreateDevice(d3D.xAdapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&d3D.device)));

        if(!d3D.device){
            std::cerr<<"Direct3D 12 compatible device was not found!";
            return;
        }

    d3D.device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //Allows all operations, whether they are graphics, compute or copy. Despite every command type being allowed, only command lists with type DIRECT are allowed to be submitted.
    hr = d3D.device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&d3D.commandQueue));
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
    window = SDL_CreateWindow("RayTracing", windowWidth, windowHeight, 0);
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

    //Everytime a commandlist is reset, it will allocate a block of memory from the commandallocator, it will pile up. We can reset the command allocator, but if we do so, then we shouldn't use the command list and execute it with the previously allocated memory. So, if we are to reset command allocator, do it before reseting command lists, as the newly reset command list will point to the correct memory block, however in between the resetting of the command allocator (which should be done before the resetting of the list), the command should not be executed.
    hr = d3D.device->CreateCommandList(0,//Node mask, GPU(s)
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       d3D.commandAllocators[PRIMARY].Get(),
                                       nullptr,
                                       IID_PPV_ARGS(&d3D.commandLists[cmdList::RENDER]));
    if(FAILED(hr)){
        std::cout<<"Triangle Command list creation failed!";
        return;
    }
    //d3D.commandLists[cmdList::RENDER]->Close();
    //Synchronization. Why it's important. So the GPU and CPU are both working simulatenously, however, we don't want one to overtake the other, we don't want the CPU to overtake and change the PSO before the frame has been completed by the GPU and cause errors, and we don't want the GPU to start making frames before updating uniforms and stuff by the CPU.
    //A fence is one of the most simple synchronization objects. It exists in both the CPU and the GPU. It has an internal integer counter, and can trigger events on either the GPU or the GPU when the counter reached a certain value, that we can specify.
    //This can be done to trigger an event on the CPU when a command allocator's commands have been executed fully, or we can have the GPU wait while the CPU is completing a certain operation first, or we can get the command queues (if multiple exists), on the GPU to wait upon one another.
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
    glm::vec3 cameraPos = {1.0f, 0.0f, 10.0f};
    glm::vec3 cameraDir = {0.0f, 0.0f, -1.0f};
    glm::vec3 cameraUp = {0.0f, 1.0f, 0.0f};
    camera = new Camera(cameraPos, cameraDir, cameraUp); //Using new instead of malloc cause it's a class with it's constructors and what not.

    Vertex triangleVertices[3] = {
        {{0.0f, 0.5f, -0.5f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-0.5, -0.5f, 0.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}}};

    Vertex quadVertices[4] = {
        {{-1.0f, -1.0f, 0.0f, 1.0f}, //bottom left
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f, 1.0f}, //Top left
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f, 1.0f}, //Top right
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        {{1.0f, -1.0f, 0.0f, 1.0f}, //Bottom right
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}}};

    unsigned int quadIndices[6] = {
        0, 1, 2, //top left triangle
        2, 3, 0 //bottom right triangle
    };
    //NOTE: I should send this to other functions like camera functins and model functions so they can just slot their matrix in easily.
    perFrameConstantData = (char*)malloc(256);
    memset(perFrameConstantData, 0, 256);
    glm::vec4* color = (glm::vec4*)perFrameConstantData;
    *color = {0.0f, 0.5f, 0.5f, 1.0f};
    glm::vec4* metallic = color + 1;
    *metallic = {1.0f, 0.2f, 0.0f, 1.0f};
    glm::mat4 tempViewMat4 = camera->getMatView();
    glm::mat4 tempProjMat4 = camera->getMatProj();
    metallic += 1;
    //Give the mat4 as 4 vec4s
    glm::mat4* viewMatR = (glm::mat4*)metallic;
    *viewMatR = tempViewMat4;
    glm::mat4* projMatR = viewMatR + 1;
    *projMatR = tempProjMat4;
    //*currentDrawIndex = {0, 0.0f, 0.0f, 0.0f};
    //glm::vec4* 
    int textureWidth, textureHeight, nrChannels;
    std::string texturePath = "../resources/practicetexture.png";
    unsigned char* textureData = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &nrChannels, 0);
    if (textureData == nullptr)
        std::cout<<"Texture data can't be opened in memory!";
    VertexSizePair triAndQuad[3]; //Makes them congiguous.
    projMatR = projMatR + 1;
    glm::vec4* viewPos = (glm::vec4*)projMatR;
    *viewPos = camera->getVFront();
    Models firstModels;
    firstModels.loadModels("../resources/modelInfo.txt");
    unsigned int numOfModels = firstModels.models.size();
    perModelConstantData = (char*)malloc(256 * numOfModels);
    DataArray perModelData = {};
    PtrSizePair* perModelPSP = new PtrSizePair[numOfModels]; //Cause array should be compile time.
    glm::mat4* modelMatrixPtr = (glm::mat4*)perModelConstantData;
    char* startPtr = perModelConstantData;
    for(uint32_t i = 0; i < numOfModels; i++){
        glm::mat4 tempModelMat4 = firstModels.models[i].modelMatrix;
        char* newOffsettedPtr = (256 * i) + startPtr;
        modelMatrixPtr = (glm::mat4*) newOffsettedPtr;
        *modelMatrixPtr = tempModelMat4;
        perModelPSP[i].data = modelMatrixPtr;
        perModelPSP[i].size = sizeof(tempModelMat4);
    }
    perModelData.PSPArray.arr = perModelPSP;
    perModelData.PSPArray.count = numOfModels;
    std::cout<<"Num of models is: "<<numOfModels;
    triAndQuad[0].data = triangleVertices;
    triAndQuad[0].size = sizeof(triangleVertices);
    triAndQuad[1].data = quadVertices;
    triAndQuad[1].size = sizeof(quadVertices);
    std::vector<Vertex> allModelVertices;
    for(uint32_t i = 0; i < numOfModels; i++){
        allModelVertices.insert(allModelVertices.end(), 
            firstModels.models[i].vertices.begin(),
            firstModels.models[i].vertices.end());
    }
    triAndQuad[2].data = allModelVertices.data();
    triAndQuad[2].size = allModelVertices.size() * sizeof(Vertex);
    DataArray vertexData = {};
    vertexData.VSPArray.arr = triAndQuad;
    vertexData.VSPArray.count = 3;
    PtrSizePair modelQuadIn[2];
    std::vector<Face> allModelFaceIndices;
    for(uint32_t i = 0; i < numOfModels; i++){
        allModelFaceIndices.insert(allModelFaceIndices.end(),
            firstModels.models[i].faces.begin(),
            firstModels.models[i].faces.end());
    }
    modelQuadIn[0].data = allModelFaceIndices.data();
    modelQuadIn[0].size = allModelFaceIndices.size() * sizeof(Face);
    modelIndices.push_back(allModelFaceIndices.size() * 3);
    modelQuadIn[1].data = quadIndices;
    modelQuadIn[1].size = sizeof(quadIndices);
    DataArray indexData = {};
    indexData.PSPArray.arr = modelQuadIn;
    indexData.PSPArray.count = 2;
    //NOTE: Make constant buffer bigger through better offsetting and whatnot.
    DataArray perFrameConstantBufferData =  {};
    PtrSizePair perFrameConstantBufferPairs[1];
    perFrameConstantBufferPairs[0].data = perFrameConstantData;
    perFrameConstantBufferPairs[0].size = 256;
    perFrameConstantBufferData.PSPArray.arr = perFrameConstantBufferPairs;
    perFrameConstantBufferData.PSPArray.count = 1;
    UINT totalSizeByCount = (getVSPDataSize(vertexData)/65536) + 1;
    totalSizeByCount += (getPSPDataSize(indexData)/65536) + 1;
    totalSizeByCount += (getPSPDataSize(perFrameConstantBufferData)/65536) + 1;
    totalSizeByCount += (getPSPDataSize(perModelData)/65536) + 1;
    std::cout<<"The total size is going to be:"<<totalSizeByCount<<"\n";
    //Creating upload Heap.
    Heap::createHeap(totalSizeByCount, heapInfo::HEAP_UPLOAD, d3D, resource);
    //PrintDebugMessages();
    //Creating a descriptor heap for the srvs and cbvs.
    Heap::createDescriptorHeap(dhInfo::DH_SRV_CBV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, d3D, resource);
    //PrintDebugMessages();
    //Creating the vertex and index buffer.
    Resource::initVertexBuffer(vertexData, d3D, resource);
    //PrintDebugMessages();
    Resource::initIndexBuffer(indexData, d3D, resource);
    //PrintDebugMessages();
    Heap::createDescriptorHeap(dhInfo::DH_RTV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, d3D, resource);
    //PrintDebugMessages();
    //Creating the back buffers.
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resource.descriptorHeaps[dhInfo::DH_RTV]->GetCPUDescriptorHandleForHeapStart();
    UINT rtvDescriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UINT currentDescriptorHeapOffset = rtvDescriptorIncrementSize * resource.descriptorInHeapCount[dhInfo::DH_RTV];
    handle.ptr += currentDescriptorHeapOffset;
    backBufferOffset = resource.descriptorInHeapCount[dhInfo::DH_RTV];
    Resource::createBackBuffers(windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, d3D, resource);
    //PrintDebugMessages();
    //Creating the gBuffer. Turns out I am not using my default heap for anything right now... Will figure out later.
    renderTextureOffset = resource.texture2Ds.size();
    Resource::createGPUTexture(windowWidth, windowHeight, DXGI_FORMAT_R16G16B16A16_UNORM, textureTypeInfo::TEX_TYPE_RGBA, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::DH_DSV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, d3D, resource);
    Resource::createSimpleDepthStencil(windowWidth, windowHeight, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::DH_SAMPLER, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, d3D, resource);
    Heap::createDescriptorHeap(dhInfo::DH_IMGUI_SRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, d3D, resource);
    Resource::createSimpleSampler(d3D, resource);
    Resource::init2DTexture(textureData, textureWidth, textureHeight, nrChannels, DXGI_FORMAT_R8G8B8A8_UNORM, fenceValue, d3D, resource);
    constantBufferOffset = resource.descriptorInHeapCount[dhInfo::DH_SRV_CBV_UAV];
    Resource::initPerFrameConstantBuffer(perFrameConstantBufferData, d3D, resource);
    Resource::initPerModelConstantBuffer(perModelData, d3D, resource);
    //PrintDebugMessages();
    fenceValue++;
    //Creating a bindless root singature.
    RootSignature::createBindlessRootSignature(d3D, resource);
    //Creating the first pipeline state.
    Shader renderShader("../shaders/simple.vsh","../shaders/simple.psh");
    PipelineState::createGraphicsPSO(psoInfo::PSO_RENDER, renderShader, true, DXGI_FORMAT_R16G16B16A16_UNORM, d3D, resource);
    Shader quadShader("../shaders/quad.vsh", "../shaders/quad.psh");
    PipelineState::createGraphicsPSO(psoInfo::PSO_PRESENT, quadShader, false, DXGI_FORMAT_R8G8B8A8_UNORM, d3D, resource);
    PrintDebugMessages();
}

void Engine::render(){
    bool running;
    running = true;
    SDL_Event sdlEvent;
    float eachStep = 0.005;
    float eachDir = 0.00025;
    if(!window){
        std::cerr<<"SDL Window Creation has failed!";
        return;
    }
    UINT frameIndex = 0;
    d3D.commandLists[cmdList::RENDER]->Close();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    io.FontGlobalScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.5f); // scales all paddings, borders, etc.
    ImGui_ImplSDL3_InitForD3D(window);
    ImGui_ImplDX12_Init(
        d3D.device.Get(),
        2,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        resource.descriptorHeaps[dhInfo::DH_SRV_CBV_UAV].Get(),
        resource.descriptorHeaps[dhInfo::DH_IMGUI_SRV]->GetCPUDescriptorHandleForHeapStart(),
        resource.descriptorHeaps[dhInfo::DH_IMGUI_SRV]->GetGPUDescriptorHandleForHeapStart()
    );
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    while(running){
        while (SDL_PollEvent(&sdlEvent))
            if(sdlEvent.type == SDL_EVENT_QUIT){
                running = false;
                break;
            }
        // areDownMov   || areDownDir
        // 0bWASDCtSp00 || 0bLRUpDn0000
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        //ImGui::Begin("Debug Info");
        //ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::SetNextWindowSize(ImVec2(200, 200));
        ImGui::Begin("My First Tool", NULL, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
                if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        float samples[100];
        for (int n = 0; n < 100; n++)
            samples[n] = sinf(n * 0.2f + ImGui::GetTime() * 1.5f);
        ImGui::PlotLines("Samples", samples, 100);

        ImGui::TextColored(ImVec4(1,1,0,1), "Important Stuff");
        ImGui::BeginChild("Scrolling");
        for (int n = 0; n < 50; n++)
            ImGui::Text("%04d: Some text", n);
        ImGui::EndChild();
        ImGui::End();

        SHORT stateW = GetAsyncKeyState('W');
        SHORT stateA = GetAsyncKeyState('A');
        SHORT stateS = GetAsyncKeyState('S');
        SHORT stateD = GetAsyncKeyState('D');
        SHORT stateCt = GetAsyncKeyState(VK_LCONTROL);
        SHORT stateSp  = GetAsyncKeyState(VK_SPACE);
        SHORT stateL = GetAsyncKeyState(VK_LEFT);
        SHORT stateR = GetAsyncKeyState(VK_RIGHT);
        SHORT stateUp = GetAsyncKeyState(VK_UP);
        SHORT stateDn = GetAsyncKeyState(VK_DOWN);
        unsigned char areDownMovement = 0b00000000;
        unsigned char areDownDirection = 0b00000000;
        areDownMovement = (stateW & 0x8000) ? (areDownMovement | 0b10000000) : (areDownMovement & 0b01111111);
        areDownMovement = (stateA & 0x8000) ? (areDownMovement | 0b01000000) : (areDownMovement & 0b10111111);
        areDownMovement = (stateS & 0x8000) ? (areDownMovement | 0b00100000) : (areDownMovement & 0b11011111);
        areDownMovement = (stateD & 0x8000) ? (areDownMovement | 0b00010000) : (areDownMovement & 0b11101111);
        areDownMovement = (stateCt & 0x8000) ? (areDownMovement | 0b00001000) : (areDownMovement & 0b11110111);
        areDownMovement = (stateSp & 0x8000) ? (areDownMovement | 0b00000100) : (areDownMovement & 0b11111011);
        areDownDirection = (stateL & 0x8000) ? (areDownDirection | 0b10000000) : (areDownDirection & 0b01111111);
        areDownDirection = (stateR & 0x8000) ? (areDownDirection | 0b01000000) : (areDownDirection & 0b10111111);
        areDownDirection = (stateUp & 0x8000) ? (areDownDirection | 0b00100000) : (areDownDirection & 0b11011111);
        areDownDirection = (stateDn & 0x8000) ? (areDownDirection | 0b00010000) : (areDownDirection & 0b11101111);
        float xMovChange = 0;
        float zMovChange = 0;
        float yMovChange = 0;
        float xDirChange = 0;
        float yDirChange = 0;
        if(areDownMovement != 0 || areDownDirection != 0) {
            DataArray perFrameConstantBufferData = {};
            PtrSizePair constantBufferPairs[1];
            glm::vec4* start = (glm::vec4*)perFrameConstantData;
            start += 2;
            glm::mat4* viewMat = (glm::mat4*)start;
            xMovChange = (areDownMovement & 0b01010000) ?
                                (areDownMovement & 0b01000000) ? -eachStep : eachStep
                                : 0;
            zMovChange = (areDownMovement & 0b10100000) ?
                                (areDownMovement & 0b10000000) ? eachStep : -eachStep
                                : 0;
            yMovChange = (areDownMovement & 0b00001100) ?
                                (areDownMovement & 0b00001000) ? -eachStep : eachStep
                                : 0;
            xDirChange = (areDownDirection & 0b11000000) ?
                                (areDownDirection & 0b10000000) ? eachDir : -eachDir
                                : 0;
            yDirChange = (areDownDirection & 0b00110000) ?
                                (areDownDirection & 0b00100000) ? eachDir : -eachDir
                                : 0;

            camera->updateCamera(glm::vec3(xMovChange, yMovChange, zMovChange), glm::vec3(xDirChange, yDirChange, 0));
            *viewMat = camera->getMatView();
            glm::mat4* projMat = viewMat + 1;
            *projMat = camera->getMatProj();
            constantBufferPairs->data = perFrameConstantData;
            constantBufferPairs->size = 256;
            perFrameConstantBufferData.PSPArray.arr = constantBufferPairs;
            perFrameConstantBufferData.PSPArray.count = 1;
            Resource::updateConstantBuffer(perFrameConstantBufferData, bufferInfo::BUFFER_PER_FRAME_CONSTANT, d3D, resource);
            }
        hr = d3D.commandAllocators[cmdAllocator::PRIMARY]->Reset();
        if(FAILED(hr)){
            std::cerr<<"Command allocator is reset during the start of each frame, and it couldn't reset!";
            return;
        }
        hr = d3D.commandLists[cmdList::RENDER]->Reset(d3D.commandAllocators[cmdAllocator::PRIMARY].Get(), resource.pipelineStates[psoInfo::PSO_RENDER].Get());
        if(FAILED(hr)){
            std::cerr<<"Command list is reset during the start of each frame, and it couldn't reset!";
            return;
        }
        d3D.commandLists[cmdList::RENDER]->SetGraphicsRootSignature(resource.rootSignature.Get());
        // RTV heaps that we use for presenting to the screen, and not for rendering to a frame buffer or for re-use by the shader, do not need to be set with SetDescriptorHeaps, they are instead set by OMSetRenderTarget. If I wanted to reuse those render targets, then I should create a frame buffer and while creating the rtv in the RTVHeapDesc, I should have used the flag for ALLOW_RENDER_TARGET. Now I can re-use the same RTVs for both Framebuffer and for the swapchain, but then the swapchain format would need to be the same as the RTV format, and we will lose lots of information with only 1 byte for each color channel.
        ID3D12DescriptorHeap* dHeaps[] = { resource.descriptorHeaps[dhInfo::DH_SRV_CBV_UAV].Get(), resource.descriptorHeaps[dhInfo::DH_SAMPLER].Get() }; //No RTV here because they're not shader visible.
        d3D.commandLists[cmdList::RENDER]->SetDescriptorHeaps(_countof(dHeaps), dHeaps);
        d3D.commandLists[cmdList::RENDER]->SetGraphicsRootDescriptorTable(0, //Root parameter index, base register + index (b0, b1, and dependingly...) in the shader. If range says 5 descriptors, then it will basically start from that base register range and from the uniform heap pointer, and then allocate 5 consecutive descriptors.
                                                                resource.descriptorHeaps[dhInfo::DH_SRV_CBV_UAV]->GetGPUDescriptorHandleForHeapStart());
        resource.descriptorHeaps[0]->GetGPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE cbvHandle = resource.descriptorHeaps[dhInfo::DH_SRV_CBV_UAV]->GetGPUDescriptorHandleForHeapStart();
        UINT descriptorSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        cbvHandle.ptr += descriptorSize * constantBufferOffset;
        d3D.commandLists[cmdList::RENDER]->SetGraphicsRootDescriptorTable(1, cbvHandle);
        //d3D.commandLists[cmdList::RENDER]->SetGraphicsRootDescriptorTable(1, gHandle);
        d3D.commandLists[cmdList::RENDER]->SetGraphicsRootDescriptorTable(2, resource.descriptorHeaps[dhInfo::DH_SAMPLER]->GetGPUDescriptorHandleForHeapStart());
        //Each 'slot' in the root parameter can hold anyting from one descriptor to descriptor tables. Each descriptor table can have multiple ranges. Where a range describes a contiguous block of descriptors of same type. Set a certain table. So this binds the 0'th table to the descriptorhandlestart.. So however it has defined it.
        D3D12_VIEWPORT viewPort = {}; //Viewport defines where we want to render to in the swapchain or the render target, whatever we are rendering to. So if I have a viewport of 500x500 in a 1000x1000 window, it will only render to the other 500x500
        viewPort.TopLeftX = 0.0f; //Position of X on the left
        viewPort.TopLeftY = 0.0f; //Position of Y on the top
        viewPort.Width = static_cast<float>(windowWidth);
        viewPort.Height = static_cast<float>(windowHeight);
        viewPort.MinDepth = D3D12_MIN_DEPTH;
        viewPort.MaxDepth = D3D12_MAX_DEPTH;
        // Viewport can clip pixels, and so does a rect, but why 2 different types of clipping? Why would I ever use a viewport and also a rect, why ever use a rect? It's because if I change a viewport, the projection would change, so if I don't wanna change the projection, then I would use a scissor clipping through a rect. Imagine rotating a screen, if u wanna adjust stuff, then change viewport, if u just wanna cut stuff, change rect.
        D3D12_RECT rect = {};
        rect.left = 0;
        rect.top = 0;
        rect.right = windowWidth;
        rect.bottom = windowHeight;
        d3D.commandLists[cmdList::RENDER]->RSSetViewports(1, &viewPort);
        d3D.commandLists[cmdList::RENDER]->RSSetScissorRects(1, &rect);
        // Resource barrier is used when we want to use the same resource (say a texture) to write and read both, and in that case, the applicaiton will tell the GPU that this resource is in a write-ready state or read-ready state,and the GPU will wait for the transition if it's trying to read and the resource is in write-ready state. Now, since our pipeline cannot actually read the render texture, we are going to change state from 'present' meaning, to present to the swapchain, to render target, and vice-versa.
        D3D12_RESOURCE_BARRIER bbBarrierRender = {}; // Describes a resource barrier (transition in resource use).
        bbBarrierRender.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; //Transition of a set of subreosuces between different usages. The caller must specify the before and after usages of the subresources.
        bbBarrierRender.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        D3D12_RESOURCE_TRANSITION_BARRIER bbTransition = {}; //Describes the transition of subresources between different usages.
        bbTransition.pResource = resource.texture2Ds[renderTextureOffset].Get(); //First it's the render texture for firstpass.
        bbTransition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; //Transition all subresources at the same time when transitioning state of a resource.
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbBarrierRender.Transition = bbTransition;
        d3D.commandLists[cmdList::RENDER]->ResourceBarrier(1, &bbBarrierRender);//A resource barrier is a command you insert into a command list to inform the GPU driver that a resource (like a texture or buffer) is about to be used in a different way than before. This helps the GPU synchronize access to that resource and avoid hazards such as reading while writing or using stale data.
        D3D12_CPU_DESCRIPTOR_HANDLE handle = resource.descriptorHeaps[dhInfo::DH_RTV]->GetCPUDescriptorHandleForHeapStart();
        UINT rtvDescriptorIncrementSize = d3D.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        handle.ptr += (backBufferOffset + 2) * rtvDescriptorIncrementSize; //Can do it right now because I make the G buffer as soon as I make back buffers. Pointing to the G buffer instead of one of the two back buffers.
        const float clearColor[4] { 0.0f, 0.0f, 0.0f, 1.0f };
        d3D.commandLists[cmdList::RENDER]->ClearRenderTargetView(handle, clearColor,
                                                        0, //Number of RECTs to clear.
                                                        nullptr //Rects to clear, basically we can clear only a certain rect of the RTV if we want to.
                                                        );

        //Nullptr and false since no depth/stencil.
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = resource.descriptorHeaps[dhInfo::DH_DSV]->GetCPUDescriptorHandleForHeapStart();
        d3D.commandLists[cmdList::RENDER]->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        d3D.commandLists[cmdList::RENDER]->OMSetRenderTargets(1, &handle, FALSE, &dsvHandle);
        d3D.commandLists[cmdList::RENDER]->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        //d3D.commandLists[cmdList::RENDER]->IASetVertexBuffers(0, 1, &resource.vbViews[0]);
        d3D.commandLists[cmdList::RENDER]->IASetVertexBuffers(0, 1, &resource.vbViews[2]);
        d3D.commandLists[cmdList::RENDER]->IASetIndexBuffer(&resource.ibViews[0]);
        d3D.commandLists[cmdList::RENDER]->DrawIndexedInstanced(modelIndices[0], 1, 0, 0, 0);
        PrintDebugMessages();
        //d3D.commandLists[cmdList::RENDER]->DrawInstanced(3, 1, 0, 0);

        //PrintDebugMessages();
        D3D12_RESOURCE_BARRIER bbBarrierRead = {};
        bbBarrierRead.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        bbBarrierRead.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bbBarrierRead.Transition = bbTransition;

        d3D.commandLists[cmdList::RENDER]->ResourceBarrier(1, &bbBarrierRead);
        d3D.commandLists[cmdList::RENDER]->SetPipelineState(resource.pipelineStates[psoInfo::PSO_PRESENT].Get());
        handle = resource.descriptorHeaps[dhInfo::DH_RTV]->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (backBufferOffset + frameIndex) * rtvDescriptorIncrementSize; //Pointing to one of the back buffers instead of the G buffer to render to.
        bbTransition.pResource = resource.texture2Ds[renderTextureOffset - 2 + frameIndex].Get();
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbBarrierRender.Transition = bbTransition;
        d3D.commandLists[cmdList::RENDER]->ResourceBarrier(1, &bbBarrierRender);

        D3D12_RESOURCE_BARRIER bbBarrierPresent = {};
        bbBarrierPresent.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        bbBarrierPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bbTransition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bbTransition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        bbBarrierPresent.Transition = bbTransition;
        //PrintDebugMessages();
        d3D.commandLists[cmdList::RENDER]->OMSetRenderTargets(1, &handle, FALSE, nullptr);
        d3D.commandLists[cmdList::RENDER]->ClearRenderTargetView(handle, clearColor, 0, nullptr);
        d3D.commandLists[cmdList::RENDER]->IASetVertexBuffers(0, 1, &resource.vbViews[1]);
        d3D.commandLists[cmdList::RENDER]->IASetIndexBuffer(&resource.ibViews[1]);
        d3D.commandLists[cmdList::RENDER]->DrawIndexedInstanced(6, 1, 0, 0, 0);

        ImGui::Render();
        ID3D12DescriptorHeap* imgui_heaps[] = { resource.descriptorHeaps[dhInfo::DH_IMGUI_SRV].Get() };
        d3D.commandLists[cmdList::RENDER]->SetDescriptorHeaps(1, imgui_heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3D.commandLists[cmdList::RENDER].Get());
        d3D.commandLists[cmdList::RENDER]->ResourceBarrier(1, &bbBarrierPresent);
        hr = d3D.commandLists[cmdList::RENDER]->Close();
        if(FAILED(hr)){
            std::cerr<<"Command list close during render-loop failed!";
            std::cout<<hr;
            return;
        }
        //PrintDebugMessages();
        ID3D12CommandList* cLists[] = { d3D.commandLists[cmdList::RENDER].Get() };
        d3D.commandQueue->ExecuteCommandLists(_countof(cLists), cLists);
        //PrintDebugMessages();
        hr = d3D.xSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        if(FAILED(hr)){
            std::cerr<<"Swapchain present failed!";
//            return;
        }
        hr = d3D.commandQueue->Signal(d3D.fence.Get(), fenceValue); //Will tell the GPU after finishing all the currently queued commands, set the fence to the fence value. It will se the GPU fence value to the fence value variable provided through this method. Basically saying hey commandqueue, after you finish, at this address (given by the fence), put the value (given by fencevalue).
        if(FAILED(hr)){
            std::cerr<<"Command queue signal failed!";
            return;
        }
        if (d3D.fence->GetCompletedValue() < fenceValue) { //If the value in the fence object is less than the 'fence value' variable, then that means that the command queue has not finished yet, as the command queue will set the value in the fence object to be fence value after finishing.
            hr = d3D.fence->SetEventOnCompletion(fenceValue, d3D.fenceEvent); //Basically will say that if the fence's value is the value provided through the 'fencevalue' variable, then fenceevent should be "signaled". "Signaling" in this context means that the fence object will notify a event handle if that happens.
            if(FAILED(hr)){
                std::cerr<<"Failed to set fence event on completion in the rendering loop!";
                return;
            }
            WaitForSingleObject(d3D.fenceEvent, INFINITE); //Basically saying that the CPU thread should wait an 'INFINITE' amount of time until event has been signaled. The thread will do other things, and the reason we don't just run an infinite loop instead, checking and seeing if the fence value is reached or not, is because the thread will not be free for other tasks + the memory will keep being accessed over and over again, which may not be bad since cache, but it'll waste the other 64-8 bytes in the cache.
        }
        frameIndex = d3D.xSwapChain->GetCurrentBackBufferIndex();
        fenceValue++;
        PrintDebugMessages();
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return;
}
