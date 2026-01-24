#include "../include/importer.h"
#include "utils.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void Model::loadModel(std::string &modelPath, glm::vec3 worldPos, glm::vec3 worldRotate, glm::vec3 worldScale, u32 modelIndex, u32 indicesOffset, Arena* vertexArena, Arena* indicesArena){
    std::vector<unsigned int> vertexInfo; // Flag for each vertex, if it's duplicated or not.
    GET_POINTER_IN_ARENA(*vertexArena, Vertex, aVertices);
    Vertex* currentVertex = aVertices;
    GET_POINTER_IN_ARENA(*indicesArena, Faces, aFaces);
    Face* currentFace = aFaces;
    u32 vertexNum = 0;
    u32 faceNum = 0;
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
                vertexNum++;
                float v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                tempVertexData.position.x = v1;
                tempVertexData.position.y = v2;
                tempVertexData.position.z = v3;
                tempVertexData.position.w = 1.0;
                *currentVertex = tempVertexData;
                currentVertex++;
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
                faceNum++;
                std::string group;
                std::vector<u32> groupNumbers;
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
                    aVertices[groupNumbers[0]].normal = normals[groupNumbers[2]];
                    vertexInfo[groupNumbers[0]] = 79;
                }
                else{
                    tempVertexData.position = aVertices[groupNumbers[0]].position;
                    tempVertexData.normal = normals[groupNumbers[2]];
                    groupNumbers[0] = vertexNum;
                    *currentVertex = tempVertexData;
                    currentVertex++;
                    vertexNum++;
                    vertexInfo.push_back(79);
                }
                if(vertexInfo[groupNumbers[3]] == 3773){
                    aVertices[groupNumbers[3]].normal = normals[groupNumbers[5]];
                    vertexInfo[groupNumbers[3]] = 79;
                }
                else{
                    tempVertexData.position = aVertices[groupNumbers[3]].position;
                    tempVertexData.normal = normals[groupNumbers[5]];
                    groupNumbers[3] = vertexNum;
                    *currentVertex = tempVertexData;
                    currentVertex++;
                    vertexNum++;
                    vertexInfo.push_back(79);
                }
                if(vertexInfo[groupNumbers[6]] == 3773){
                    aVertices[groupNumbers[6]].normal = normals[groupNumbers[8]];
                    vertexInfo[groupNumbers[6]] = 79;
                }
                else{
                    tempVertexData.position = aVertices[groupNumbers[6]].position;
                    tempVertexData.normal = normals[groupNumbers[8]];
                    groupNumbers[6] = vertexNum;
                    *currentVertex = tempVertexData;
                    currentVertex++;
                    vertexNum++;
                    vertexInfo.push_back(79);
                }
                tempFace.indices[0] = groupNumbers[0] + indicesOffset;
                tempFace.indices[1] = groupNumbers[3] + indicesOffset;
                tempFace.indices[2] = groupNumbers[6] + indicesOffset;
                *currentFace= tempFace;
                if (indicesOffset == 0)
                    printf("%d, %d, %d \n", tempFace.indices[0], tempFace.indices[1], tempFace.indices[2]);
                currentFace++;
            }
        }
    }
    file.close();
    glm::mat4 Scale = glm::scale(glm::mat4(1.0f), worldScale);
    glm::mat4 Trans = glm::translate(glm::mat4(1.0f), worldPos);
    modelMatrix = Trans * Scale;
    PUSH_POINTER_IN_ARENA(*vertexArena, Vertex, vertexNum);
    PUSH_POINTER_IN_ARENA(*indicesArena, Face, faceNum);
    numFaces = faceNum;
    numVertices = vertexNum;
    printf("The index is: %d\n", aFaces->indices[0]);
    //printf("The number of indices is: %d \n", numFaces * 3);
}

ModelData* Models::loadModels(const std::string &modelInfoPath, u32 minModelSizeInBytes){
    vertexArena.reserveArena(minModelSizeInBytes);
    vertexArena.commitArena(minModelSizeInBytes);
    indicesArena.reserveArena(minModelSizeInBytes);
    indicesArena.commitArena(minModelSizeInBytes);
    std::vector<std::string> actualPath;
    std::ifstream filePath(modelInfoPath);
    std::vector<glm::vec3> modelWorldPos;
    std::vector<glm::vec3> modelWorldRot;
    std::vector<glm::vec3> modelWorldScl;
    if (!filePath.is_open()) {
        std::cerr << "Error opening modelInfo file!" << std::endl;
        return NULL;
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
    u32 modelNum = actualPath.size();
    u32 modelIndicesOffset = 0;
    for(size_t i = 0; i < modelNum; i++){
        models[i].loadModel(actualPath[i], modelWorldPos[i], modelWorldRot[i], modelWorldScl[i], i, modelIndicesOffset, &vertexArena, &indicesArena);
        modelIndicesOffset += (models[i].numFaces * 3);
    }
    printf("The index in 'models' is: %d\n", models[0].aFaces->indices[0]);
    ARENA_ERROR error;
    DataArray* tempPtr;
    error = GET_POINTER_IN_ARENA(vertexArena, DataArray, tempPtr);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    data.modelVertices = tempPtr;
    data.modelIndices = tempPtr + 1;
    data.modelMatrices = tempPtr + 2;
    error = PUSH_POINTER_IN_ARENA(vertexArena, DataArray, 3);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    VertexSizePair* vertexVSP;
    VertexSizePair* currentPointer;
    error = GET_POINTER_IN_ARENA(vertexArena, VertexSizePair, vertexVSP);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    currentPointer = vertexVSP;
    for(size_t i = 0; i < modelNum; i++){
        currentPointer->size = models[i].numVertices * sizeof(Vertex);
        currentPointer->data = models[i].aVertices;
        currentPointer++;
    }
    error = PUSH_POINTER_IN_ARENA(vertexArena, VertexSizePair, modelNum);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    PtrSizePair* indicesPSP;
    PtrSizePair* currentPtr;
    error = GET_POINTER_IN_ARENA(vertexArena, PtrSizePair, indicesPSP);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    currentPtr = indicesPSP;
    for(size_t i = 0; i < modelNum; i++){
        currentPtr->size = models[i].numFaces * sizeof(Face);
        //printf("The num of indices after dataArray are: %d \n", currentPtr->size / sizeof(u32));
        currentPtr->data = models[i].aFaces;
        printf("The index is: %d \n", models[i].aFaces->indices[0]);
        currentPtr++;
    }
    PtrSizePair* matPSP = currentPtr;
    error = PUSH_POINTER_IN_ARENA(vertexArena, PtrSizePair, modelNum);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    for(size_t i = 0; i < modelNum; i++){
        currentPtr->data = &models[i].modelMatrix;
        currentPtr->size = sizeof(glm::mat4);
        currentPtr++;
    }
    error = PUSH_POINTER_IN_ARENA(vertexArena, PtrSizePair, modelNum);
    if (error != ARENA_OK) std::cerr<<"Arena: "<<error;
    data.modelVertices->VSPArray.arr = vertexVSP;
    data.modelVertices->VSPArray.count = modelNum;
    data.modelMatrices->PSPArray.arr = matPSP;
    data.modelMatrices->PSPArray.count = modelNum;
    data.modelIndices->PSPArray.count = modelNum;
    data.modelIndices->PSPArray.arr = indicesPSP;
    return &data;
}

Models::~Models(){
    vertexArena.removeArena();
    indicesArena.removeArena();
}
