#pragma once
#include "D3D12/d3d12.h"
#include "utils.h"
#include "shader.h"
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include "importer.h"

UINT getPSPDataSize(const DataArray &data);
UINT getVSPDataSize(const DataArray &data);

namespace Heap{
    void createHeap(UINT size, heapInfo heap, const D3DGlobal &d3D, D3DResources &resources);
    void createDescriptorHeap(dhInfo dh, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, const D3DGlobal &d3D, D3DResources &resources);
};

//I should probably rename for easier searches. Meaning, for initVertexBuffer etc, I shoudl call it vertexBufferInit instead so that it can be easier for autocomplete and easier to search. The only problem is that with a camel case, it won't look as good xD.

namespace Resource{
    //I am so confused, I wasted so many hours thinking of a data agnostinc way of uplaoding to buffers but I know what I will upload in the vertex and constant buffer tho... I guess it'll be useful for constant and intermmediate bufferse?
    void initVertexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources); //width should be the sum of the data.
    void initIndexBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void initBLAS(const DataArray &vertexData, const DataArray &indexData, const DataArray &modelMat, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags, const D3DGlobal &d3D, D3DResources &resources);
    void buildBLAS(const D3DGlobal &d3D, D3DResources &resources);
    void initTLAS(const D3DGlobal &d3D, D3DResources &resources);
    void buildTLAS(const D3DGlobal &d3D, D3DResources &resources);
    void initPerFrameConstantBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void updateConstantBuffer(const DataArray &data, bufferInfo buffer, const D3DGlobal &d3D, D3DResources &resources);
    void initConstantBuffer(const DataArray &data, const D3DGlobal &d3D, D3DResources &resources);
    void createGPUTexture(UINT width, UINT height, DXGI_FORMAT format, textureTypeInfo type ,const D3DGlobal &d3D, D3DResources &resources); //For like RTVs for frame buffers.
    void createGPUTextureXR(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources);
    void createBackBuffers(UINT width, UINT height, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources); //For like the 2 back buffers.
    void init2DTexture(void* data, UINT width, UINT height, UINT nrChannels, DXGI_FORMAT format, UINT fenceValue, const D3DGlobal &d3D, D3DResources &resources); //Copy stuff from intermeddiate buffer/upload heap to default heap. The question is, should I use createpalcedresource for the intermeddiate case or createcommittedreosurce
    void createSimpleSampler(const D3DGlobal &d3D, D3DResources &resources);
    void createSimpleDepthStencil(UINT width, UINT height, const D3DGlobal &d3D, D3DResources &resources);
    void createShaderBindingTable(const D3DGlobal& d3D, D3DResources& resources);
};

namespace RootSignature{
    void createBindlessRootSignature(const D3DGlobal &d3D, D3DResources &resources);
}

// Basically: DXR uses compute pipeline instead of graphics piepline.
namespace PipelineState{
    void createGraphicsPSO(psoInfo info, const Shader &shader, bool depthEnable, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources);
    void createDXRSO(const Shader &shader, DXGI_FORMAT format, const D3DGlobal &d3D, D3DResources &resources);
}

namespace Lighting{
    typedef enum{
        LIGHT_POINT = 1917,
        LIGHT_AREA,
        LIGHT_DIRECTIONAL
    }LightType;

    typedef enum{
        LIGHT_OK,
        LIGHT_NO_SPACE,
        LIGHT_WRONG_TYPE
    }LIGHT_ERROR;
    typedef struct{
        glm::vec4 vColor;
        union{
            struct{
                glm::vec4 endPosA;
                glm::vec4 endPosB;
                u32       thickness;
            }AreaLight;
            struct{
                glm::vec4 vDir;
                glm::vec4 vPos; // World coordinate.
            }PointLight;
            struct{
                glm::vec4 vDir;
            }DirectionalLight;
        }PerTypeData;
        LightType type;
    }LightInfo;

    typedef struct simpleLightInfoStruct{
        LightInfo* lights;
        int numLights;
    }SLInfo;

    typedef struct LightStruct{
        Arena arena;
        LightInfo* lights;
        int currLightNum;
        int numLights;
        LIGHT_ERROR insertAreaLight(glm::vec4 endPosA, glm::vec4 endPosB, glm::vec4 vColor, u32 thickness);
        LIGHT_ERROR insertPointLight(glm::vec4 vDir, glm::vec4 vPos, glm::vec4 vColor);
        LIGHT_ERROR insertDirectionalLight(glm::vec4 vDir, glm::vec4 vColor);
    }Lights;

    Lights initLights(int numLights);

    SLInfo getLight(Lights &lights);
}
