#pragma once
#include <utils.h>

class Camera{
private:
    glm::vec4 vEye;
    glm::vec4 vFront;
    glm::vec4 vUp;
    glm::mat4 matView;
    glm::mat4 matProj;
public:
    Camera(glm::vec4 initPos, glm::vec4 initDir, glm::vec4 initUp);
    void updateCamera(glm::vec4 changePos, glm::vec4 changeDir, glm::vec4 changeUp);
    glm::mat4 getMatView() { return matView; }
    glm::mat4 getMatProj() { return matProj; }
    glm::vec4 getVFront() { return vFront; }
};
