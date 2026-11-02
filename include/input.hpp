#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#include <glm/glm.hpp>

class Input {
public:
    inline static GLFWwindow* glfwWindow = nullptr;

    // Keyboard
    inline static std::array<bool, GLFW_KEY_LAST + 1> current { false };
    inline static std::array<bool, GLFW_KEY_LAST + 1> previous { false };

    // Mouse
    inline static glm::vec2 mouseDelta = glm::vec2 { 0.0f };
    inline static glm::vec2 mouseAccumulated = glm::vec2 { 0.0f };
    inline static glm::vec2 lastMouse = glm::vec2 { 0.0f };
    inline static bool firstMouse = true;

    // Scroll
    inline static float scrollDelta = 0.0f;
    inline static float scrollAccumulated = 0.0f;

    static bool init(GLFWwindow* window) {
        if (!window)
            return false;

        glfwWindow = window;

        glfwSetKeyCallback(glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (key > 0 && key <= GLFW_KEY_LAST) {
                if (action == GLFW_PRESS) {
                    current[key] = true;
                } else if (action == GLFW_RELEASE) {
                    current[key] = false;
                }
            }
        });

        glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow* window, double xPos, double yPos) { mouseCallback(xPos, yPos); });

        glfwSetScrollCallback(glfwWindow, [](GLFWwindow*, double xOffset, double yOffset) { scrollCallback(xOffset, yOffset); });

        return true;
    }

    static void update() {
        if (!glfwWindow) {
            std::cerr << "Input system not initialized!\n";
            return;
        }

        mouseDelta = mouseAccumulated;
        mouseAccumulated = glm::vec2 { 0.0f };

        scrollDelta = scrollAccumulated;
        scrollAccumulated = 0.0f;

        previous = current;
    }

    static void setCursorLocked(bool locked) {
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }

    [[nodiscard]] static bool isPressed(int key) {
        if (key < 0 || key > GLFW_KEY_LAST)
            return false;
        return current[key] && !previous[key];
    }

    [[nodiscard]] static bool isDown(int key) {
        if (key < 0 || key > GLFW_KEY_LAST)
            return false;
        return current[key];
    }

    [[nodiscard]] static bool isReleased(int key) {
        if (key < 0 || key > GLFW_KEY_LAST)
            return false;
        return !current[key] && previous[key];
    }

    [[nodiscard]] static glm::vec2 mouseLook() {
        return mouseAccumulated;
    }

    [[nodiscard]] static float scroll() {
        return scrollAccumulated;
    }

private:
    static void mouseCallback(double xPos, double yPos) {
        glm::vec2 currentPos {
            static_cast<float>(xPos),
            static_cast<float>(yPos),
        };

        if (firstMouse) {
            lastMouse = currentPos;
            firstMouse = false;
        }

        mouseAccumulated += glm::vec2 {
            currentPos.x - lastMouse.x,
            currentPos.y - lastMouse.y,
        };

        lastMouse = currentPos;
    }

    static void scrollCallback(double xOffset, double yOffset) {
        scrollAccumulated += static_cast<float>(yOffset);
    }
};