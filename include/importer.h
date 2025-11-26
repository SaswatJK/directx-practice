#pragma once
#include <utils.h>

typedef struct{
    uint32_t indices[3];
}Mesh;
class Model{
public:
    std::vector<Vertex> vertices;
    std::vector<Mesh> meshes;
    glm::mat4 modelMatrix;
    //We will check that the number of Meshes isn't going to be more than the maximum number of Triangles that can be made with 'N' Vertices:
    //I calculated this using basic deduction through a complete graph: Maximum number of triangles in a complete graph is:
    // N        n           |
    // |E       |E (i - 2)  | E stands for summation.
    // n = 2    i = 2       |
    // Or simply put: NC3
    void loadModel(const std::string &path, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);
};
