#include "../include/importer.h"
#include "glm/detail/qualifier.hpp"
#include "utils.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void Model::loadModel(std::string &modelPath, glm::vec3 worldPos, glm::vec3 worldRotate, glm::vec3 worldScale, uint32_t modelIndex, uint32_t indicesOffset){
    std::vector<glm::vec4> normals;
    std::ifstream file(modelPath);
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
                tempNormal.w = modelIndex;
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
                tempFace.indices[0] = groupNumbers[0] + indicesOffset;
                tempFace.indices[1] = groupNumbers[3] + indicesOffset;
                tempFace.indices[2] = groupNumbers[6] + indicesOffset;
                faces.push_back(tempFace);
            }
        }
    }
    file.close();
    glm::mat4 Scale = glm::scale(glm::mat4(1.0f), worldScale);
    glm::mat4 Trans = glm::translate(glm::mat4(1.0f), worldPos);
    modelMatrix = Trans * Scale;
}

void Models::loadModels(const std::string &modelInfoPath){
    std::vector<std::string> actualPath;
    std::ifstream filePath(modelInfoPath);
    std::vector<glm::vec3> modelWorldPos;
    std::vector<glm::vec3> modelWorldRot;
    std::vector<glm::vec3> modelWorldScl;
    if (!filePath.is_open()) {
        std::cerr << "Error opening modelInfo file!" << std::endl;
        return;
    }
    std::string fileLine;
    while (std::getline(filePath, fileLine)){
        std::string currentPath;
        std::string identifier;
        glm::vec3 tempWorldPos;
        glm::vec3 tempWorldRot;
        glm::vec3 tempWorldScl;

        std::istringstream iss(fileLine);
        iss >> identifier;
        if(identifier == "path"){
            iss >> currentPath;
            actualPath.push_back(currentPath);
        }
        if(identifier == "p"){
            float v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            tempWorldPos.x = v1;
            tempWorldPos.y = v2;
            tempWorldPos.z = v3;
            modelWorldPos.push_back(tempWorldPos);
        }
        if(identifier == "r"){
        float v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            tempWorldRot.x = v1;
            tempWorldRot.y = v2;
            tempWorldRot.z = v3;
            modelWorldRot.push_back(tempWorldRot);
        }
        if(identifier == "s"){
        float v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            tempWorldScl.x = v1;
            tempWorldScl.y = v2;
            tempWorldScl.z = v3;
            modelWorldScl.push_back(tempWorldScl);
        }
    }
    models.resize(actualPath.size());
    uint32_t modelIndicesOffset = 0;
    for(size_t i = 0; i < actualPath.size(); i++){
        models[i].loadModel(actualPath[i], modelWorldPos[i], modelWorldRot[i], modelWorldScl[i], i, modelIndicesOffset);
        modelIndicesOffset += (models[i].faces.size() * 3);
    }
}
