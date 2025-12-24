#pragma once
#include <utils.h>

class Camera{
private:
    glm::vec3 vEye;
    glm::vec3 vFront;
    glm::vec3 vRight;
    glm::vec3 vUp;
    glm::mat4 matView;
    glm::mat4 matProj;
    glm::mat4 matTestView;
    glm::mat4 matTestProj;
public:
    Camera(glm::vec3 initPos, glm::vec3 initDir, glm::vec3 initUp);
    void updateCamera(glm::vec3 changePos, glm::vec3 changeDir, glm::vec3 changeUp);
    glm::mat4 getMatView() { return matView; }
    glm::mat4 getMatProj() { return matProj; }
    glm::mat4 getMatTestView() { return matTestView; }
    glm::mat4 getMatTestProj() { return matTestProj; }
    glm::vec4 getVFront() { return glm::vec4(vFront, 0.0); }
};
