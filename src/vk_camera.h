#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    // Spherical coordinates for orbital camera
    float theta = 0.0f;      // Horizontal angle (radians)
    float phi = 0.5f;        // Vertical angle (radians), clamped to avoid poles
    float distance = 3.0f;   // Distance from target
    
    glm::vec3 target = glm::vec3(0.0f);  // Look-at target
    
    float fov = 45.0f;       // Field of view in degrees
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float aspectRatio = 800.0f / 600.0f;
    
    // Mouse state for drag handling
    bool dragging = false;
    double lastX = 0.0, lastY = 0.0;
    
    // Get camera position from spherical coordinates
    glm::vec3 getPosition() const {
        float x = distance * sinf(phi) * cosf(theta);
        float y = distance * cosf(phi);
        float z = distance * sinf(phi) * sinf(theta);
        return target + glm::vec3(x, y, z);
    }
    
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(getPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    
    glm::mat4 getProjectionMatrix() const {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        proj[1][1] *= -1;  // Vulkan clip space Y is inverted
        return proj;
    }
    
    // Update rotation from mouse drag delta
    void rotate(float deltaX, float deltaY) {
        const float sensitivity = 0.01f;
        theta += deltaX * sensitivity;
        phi -= deltaY * sensitivity;
        
        // Clamp phi to avoid gimbal lock at poles
        const float epsilon = 0.1f;
        phi = glm::clamp(phi, epsilon, glm::pi<float>() - epsilon);
    }
    
    // Update zoom from scroll delta
    void zoom(float delta) {
        distance -= delta * 0.5f;
        distance = glm::clamp(distance, 0.5f, 50.0f);
    }
};
