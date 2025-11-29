#include "../include/camera.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/geometric.hpp"

Camera::Camera(glm::vec4 initPos, glm::vec4 initDir, glm::vec4 initUp) : vEye(initPos), vFront(initDir), vUp(initUp) {
    glm::vec3 targetPos = glm::vec3(vEye) + glm::vec3(vFront);
    matView = glm::lookAtRH(glm::vec3(vEye), targetPos, glm::vec3(vUp));
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
}

void Camera::updateCamera(glm::vec4 changePos, glm::vec4 changeDir, glm::vec4 changeUp) {
    vEye += changePos; //should move relative to front.
    float yaw = changeDir.x;
    float pitch = changeDir.y;
    glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 vRight = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(vFront)));
    glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, vRight);
    vFront = pitchRotation * yawRotation * vFront;
    vFront = glm::vec4(glm::normalize(glm::vec3(vFront)), 0.0f);
    vUp = glm::vec4(glm::normalize(glm::cross(glm::vec3(vFront), vRight)), 1.0);
    glm::vec3 targetPos = glm::vec3(vEye) + glm::vec3(vFront);
    matView = glm::lookAtRH(glm::vec3(vEye), targetPos, glm::vec3(vUp));
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
}
