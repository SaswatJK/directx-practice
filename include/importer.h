#pragma once
#include <utils.h>
#include <vector>

typedef struct{
    uint32_t indices[3];
}Face;

typedef struct{
    DataArray* modelVertices;
    DataArray* modelIndices;
    DataArray* modelMatrices;
}ModelData;

class Model{
public:
    Vertex* aVertices;
    Face* aFaces;
    u32 numFaces;
    u32 numVertices;
    glm::mat4 modelMatrix;
    //We will check that the number of Meshes isn't going to be more than the maximum number of Triangles that can be made with 'N' Vertices:
    //I calculated this using basic deduction through a complete graph: Maximum number of triangles in a complete graph is:
    // N        n           |
    // |E       |E (i - 2)  | E stands for summation.
    // n = 2    i = 2       |
    // Or simply put: NC3
    void loadModel(std::string &modelPath, glm::vec3 worldPos, glm::vec3 worldRotate, glm::vec3 worldScale, uint32_t modelIndex, uint32_t indicesOffset, Arena* vertexArena, Arena* indicesArena);
};

class Models{
    Arena vertexArena;
    Arena indicesArena;
    ModelData data;
public:
    std::vector<Model> models;
    ModelData* loadModels(const std::string &modelInfoPath, u32 minModelSizeInBytes);
    ~Models();
};
