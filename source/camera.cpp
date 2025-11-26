#include "../include/camera.h"
#include "glm/ext/matrix_clip_space.hpp"

Camera::Camera(glm::vec4 initPos, glm::vec4 initDir, glm::vec4 initUp) : vEye(initPos), vFront(initDir), vUp(initUp) {
    glm::vec3 targetPos = glm::vec3(vEye) + glm::vec3(vFront);
    matView = glm::lookAtRH(glm::vec3(vEye), targetPos, glm::vec3(vUp));
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
}

void Camera::updateCamera(glm::vec4 changePos, glm::vec4 changeDir, glm::vec4 changeUp) {
    vEye += changePos;
    vFront += changeDir;
    vUp += changeUp;
    glm::vec3 targetPos = glm::vec3(vEye) + glm::vec3(vFront);
    matView = glm::lookAtRH(glm::vec3(vEye), targetPos, glm::vec3(vUp));
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
}

