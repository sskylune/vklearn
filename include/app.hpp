#pragma once

#include <log.hpp>

#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>

#include <vector>

class App {
public:
    App(LogHandler& handler)
        : log(&handler) {}

    [[nodiscard]] bool init();
    void loop();
    void exit();

private:
    // Debug
    Log log;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    // Window
    GLFWwindow* window = nullptr;
    bool createWindow();

    // Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    bool createVulkanInstance();

    // Vulkan surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool createVulkanSurface();

    // Physical device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    bool peakPhysicalDevice();

    // Graphics queue
    VkQueue gfxQueue = VK_NULL_HANDLE;
    uint32_t gfxQueueIndex = 0;
    bool findGraphicsQueue();

    // Vulkan device
    VkDevice device = VK_NULL_HANDLE;
    bool createDevice();

    // VMA
    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    bool initVMA();

    // Vulkan swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent;

    VkFormat swapchainFormat;
    VkColorSpaceKHR swapchainColorSpace;
    VkPresentModeKHR swapchainPresentMode;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    // Depth
    VkImage depthImage = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VmaAllocation depthAllocation = VK_NULL_HANDLE;

    bool createSwapchain();
    void destroySwapchain();
};