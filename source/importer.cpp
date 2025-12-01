#include "../include/importer.h"
#include "glm/detail/qualifier.hpp"
#include "utils.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void Model::loadModel(const std::string &modelInfoPath){
    std::vector<glm::vec4> normals;
    std::string actualPath;
    std::ifstream filePath(modelInfoPath);
    glm::vec3 modelWorldPos = glm::vec3(0);
    glm::vec3 modelWorldRot = glm::vec3(0);
    glm::vec3 modelWorldScl = glm::vec3(0);
    if (!filePath.is_open()) {
        std::cerr << "Error opening modelInfo file!" << std::endl;
        return;
    }
    std::string fileLine;
    while (std::getline(filePath, fileLine)){
        std::string identifier;
        std::istringstream iss(fileLine);
        iss >> identifier;
        if(identifier == "path"){
            iss >> actualPath;
        }
        if(identifier == "p"){
         float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                modelWorldPos.x = v1;
                modelWorldPos.y = v2;
                modelWorldPos.z = v3;
        }
        if(identifier == "r"){
         float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                modelWorldRot.x = v1;
                modelWorldRot.y = v2;
                modelWorldRot.z = v3;
        }
        if(identifier == "s"){
         float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                modelWorldScl.x = v1;
                modelWorldScl.y = v2;
                modelWorldScl.z = v3;
        }
    }

    std::ifstream file(actualPath);
    std::string line;
    if (!file.is_open()) {
        std::cerr << "Error opening model file!" << std::endl;
        return;
    }
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string identifier;
        Vertex tempVertexData;
        tempVertexData.color = glm::vec4(0);
        tempVertexData.position = glm::vec4(0);
        tempVertexData.normal = glm::vec4(0);
        unsigned int tempInfo = 3773;
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
                vertexInfo.push_back(tempInfo);
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
                Face tempFace;
                while (iss >> group) {
                    std::istringstream groupStream(group);
                    std::string numStr;
                    while (std::getline(groupStream, numStr, '/')) {
                        int num = std::stoi(numStr);
                        groupNumbers.push_back(num-1); //Indices start from 0 in vectors.
                    }
                }
               //Check to see if the normals have been repeated. If so, do some basic mathematics and logic and just repeat that vertex.
                if(vertexInfo[groupNumbers[0]] == 3773){
                    vertices[groupNumbers[0]].normal = normals[groupNumbers[2]];
                    vertexInfo[groupNumbers[0]] = 79;
                }
                else{
                    tempVertexData.position = vertices[groupNumbers[0]].position;
                    tempVertexData.normal = normals[groupNumbers[2]];
                    groupNumbers[0] = vertices.size();
                    vertices.push_back(tempVertexData);
                    vertexInfo.push_back(79);
                }
                if(vertexInfo[groupNumbers[3]] == 3773){
                    vertices[groupNumbers[3]].normal = normals[groupNumbers[5]];
                    vertexInfo[groupNumbers[3]] = 79;
                }
                else{
                    tempVertexData.position = vertices[groupNumbers[3]].position;
                    tempVertexData.normal = normals[groupNumbers[5]];
                    groupNumbers[3] = vertices.size();
                    vertices.push_back(tempVertexData);
                    vertexInfo.push_back(79);

                }
                if(vertexInfo[groupNumbers[6]] == 3773){
                    vertices[groupNumbers[6]].normal = normals[groupNumbers[8]];
                    vertexInfo[groupNumbers[6]] = 79;
                }
                else{
                    tempVertexData.position = vertices[groupNumbers[6]].position;
                    tempVertexData.normal = normals[groupNumbers[8]];
                    groupNumbers[6] = vertices.size();
                    vertices.push_back(tempVertexData);
                    vertexInfo.push_back(79);

                }
                tempFace.indices[0] = groupNumbers[0];
                tempFace.indices[1] = groupNumbers[3];
                tempFace.indices[2] = groupNumbers[6];
                faces.push_back(tempFace);
            }
        }
    }
    file.close();
    glm::mat4 Scale = glm::scale(glm::mat4(1.0f), modelWorldScl);
    glm::mat4 Trans = glm::translate(glm::mat4(1.0f), modelWorldPos);
    modelMatrix = Trans * Scale;
}
