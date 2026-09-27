#pragma once

#include <log.hpp>
#include <config.hpp>

#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <shaderc/shaderc.hpp>

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

    // GLFW window
    static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window = nullptr;
    bool createWindow();

    // Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    bool createVulkanInstance();

    // Surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool createVulkanSurface();

    // Physical device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    bool peakPhysicalDevice();

    // Graphics queue
    VkQueue gfxQueue = VK_NULL_HANDLE;
    uint32_t gfxQueueIndex = 0;
    bool findGraphicsQueue();

    // Device
    VkDevice device = VK_NULL_HANDLE;
    bool createDevice();

    // VMA
    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    bool initVMA();

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent { .width = Config::SCR_WIDTH, .height = Config::SCR_HEIGHT };

    VkFormat swapchainFormat;
    VkColorSpaceKHR swapchainColorSpace;
    VkPresentModeKHR swapchainPresentMode;

    struct SwapchainResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        // Depth
        VkImage depthImage = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VmaAllocation depthAllocation = VK_NULL_HANDLE;
    };
    std::vector<SwapchainResources> swapchainResources;

    bool resetSwapchain = false;

    bool createSwapchain();
    void destroySwapchain();

    // Graphics pipeline
    VkShaderModule createShaderModule(const std::string& filename, shaderc_shader_kind kind);

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    bool createGraphicsPipeline();

    // Synchronization
    struct FrameResources {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        VkSemaphore imageAcquiredSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    };
    std::array<FrameResources, Config::FRAMES_IN_FLIGHT> frameResources;

    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;

    bool createSyncResources();
    bool initFrameResources();

    // Render
    uint64_t frameIndex = 0;
    uint64_t nextSignalValue = Config::FRAMES_IN_FLIGHT + 1;

    void render();
};