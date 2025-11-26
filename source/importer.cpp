#include "../include/importer.h"
#include "glm/detail/qualifier.hpp"
#include "utils.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

void Model::loadModel(const std::string &path, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale){
    std::vector<glm::vec4> normals;
    std::ifstream file(path);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string identifier;
        Vertex tempVertexData;
        tempVertexData.color = glm::vec4(0);
        tempVertexData.position = glm::vec4(0);
        tempVertexData.normal = glm::vec4(0);
        glm::vec4 tempNormal;
        if(iss >> identifier){
            if (identifier == "v"){
                float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                tempVertexData.position.x = v1;
                tempVertexData.position.y = v2;
                tempVertexData.position.z = v3;
                tempVertexData.position.w = 1.0;
                vertices.push_back(tempVertexData);

            }
            else if (identifier == "vn") {
                float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                tempNormal.x = v1;
                tempNormal.y = v2;
                tempNormal.z = v3;
                tempNormal.w = 0.0;
                normals.push_back(tempNormal);
            }
            else if (identifier == "f"){
                std::string group;
                std::vector<uint32_t> groupNumbers;
                Mesh tempMesh;
                while (iss >> group) {
                    std::istringstream groupStream(group);
                    std::string numStr;
                    while (std::getline(groupStream, numStr, '/')) {
                        int num = std::stoi(numStr);
                        groupNumbers.push_back(num-1); //Indices start from 0 in vectors.
                    }
                }
                tempMesh.indices[0] = groupNumbers[0];
                tempMesh.indices[1] = groupNumbers[3];
                tempMesh.indices[2] = groupNumbers[6];
                vertices[groupNumbers[0]].normal = normals[groupNumbers[2]];
                vertices[groupNumbers[3]].normal = normals[groupNumbers[5]];
                vertices[groupNumbers[6]].normal = normals[groupNumbers[8]];
                meshes.push_back(tempMesh);
            }
        }
    }
    file.close();
    glm::mat4 Scale = glm::scale(glm::mat4(1.0f), scale);
    glm::mat4 Trans = glm::translate(glm::mat4(1.0f), position);
    modelMatrix = Trans * Scale;
}
