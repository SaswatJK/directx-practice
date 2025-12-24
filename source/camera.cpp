#include "../include/camera.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"

Camera::Camera(glm::vec3 initPos, glm::vec3 initDir, glm::vec3 initUp) : vEye(initPos), vFront(initDir), vUp(initUp) {
    glm::vec3 targetPos = vEye + vFront;
    vRight = glm::normalize(glm::cross(vFront, vUp));
    matView = glm::lookAtRH(vEye, targetPos, vUp);
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
    glm::vec3 sunPos = {-20, 50, 0};
    matTestProj = glm::perspectiveRH_ZO(float(glm::radians(30.0f)), 0.5f, 0.1f, 10000.0f);
    matTestView = glm::lookAtRH(sunPos, targetPos, vUp);

}
void Camera::updateCamera(glm::vec3 changePos, glm::vec3 changeDir) {
    float yaw = changeDir.x;
    float pitch = changeDir.y;
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, worldUp);
    glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, vRight);
    vEye += glm::normalize(vRight) * changePos.x + glm::normalize(vUp) * changePos.y + glm::normalize(vFront) * changePos.z;
    //vEye += changePos;
    vFront = glm::vec3(pitchRotation * yawRotation * glm::vec4(vFront, 0.0));
    vFront = glm::vec4(glm::normalize(glm::vec3(vFront)), 0.0f);
    vRight = glm::normalize(glm::cross(vFront, worldUp));
    vUp = glm::vec4(glm::normalize(glm::cross(vRight, vFront)), 0.0);
    glm::vec3 targetPos = glm::vec3(vEye) + glm::vec3(vFront);
    matView = glm::lookAtRH(glm::vec3(vEye), targetPos, glm::vec3(vUp));
    matProj = glm::perspectiveRH_ZO(float(glm::radians(60.0f)), 1.0f, 0.1f, 10000.0f); //could make my own frustum but this is far simpler and more effective afaik
}
