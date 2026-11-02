#pragma once

#include <log.hpp>
#include <config.hpp>

#include <volk.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <tiny_gltf_v3.h>

#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

struct RenderItem {};

struct Node {
    glm::mat4 localMtx { 1.0f };
    glm::mat4 worldMtx { 1.0f };
    int32_t parentId = -1;
    int32_t meshId = -1;
};

struct Primitive {
    uint64_t vertexStart = 0;
    uint64_t vertexCount = 0;
    uint64_t indexStart = 0;
    uint64_t indexCount = 0;
    uint32_t materialId = 0;
};

struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
};

// В будущем PBR
struct Material {
    glm::vec4 baseColor = glm::vec4(1, 1, 1, 1);
    uint32_t textureId = 0;
};

struct Texture {
    uint32_t imageId = 0;
    uint32_t samplerId = 0;
};

struct GPUbuffer {
    VkBuffer buffer = nullptr;
    uint64_t deviceAddress = 0;
    VmaAllocation allocation = nullptr;
};

struct GPUimage {
    VkImage image = nullptr;
    VkImageView imageView = nullptr;
    VmaAllocation allocation = nullptr;
};

struct Image {
    int width;
    int height;
    int channels;
    std::byte* data;
};

struct FrameConstants {
    uint64_t vertexBufferAddress = 0;
    uint64_t materialBufferAddress = 0;
    uint64_t renderItemBufferAddress = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
};

class App {
public:
    App(LogHandler& handler)
        : log(&handler) {}

    [[nodiscard]] bool init();
    bool loadData();
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
    VkInstance instance = nullptr;
    bool createVulkanInstance();

    // Surface
    VkSurfaceKHR surface = nullptr;
    bool createVulkanSurface();

    // Physical device
    VkPhysicalDevice physicalDevice = nullptr;
    bool pickPhysicalDevice();

    // Graphics queue
    VkQueue gfxQueue = nullptr;
    uint32_t gfxQueueIndex = 0;
    bool findGraphicsQueue();

    // Device
    VkDevice device = nullptr;
    bool createDevice();

    // VMA
    VmaAllocator vmaAllocator = nullptr;
    bool initVMA();

    // Swapchain
    VkSwapchainKHR swapchain = nullptr;
    VkExtent2D swapchainExtent { .width = Config::SCR_WIDTH, .height = Config::SCR_HEIGHT };

    VkFormat swapchainFormat;
    VkColorSpaceKHR swapchainColorSpace;
    VkPresentModeKHR swapchainPresentMode;

    struct SwapchainResources {
        VkImage image = nullptr;
        VkImageView imageView = nullptr;
        // Depth
        VkImage depthImage = nullptr;
        VkImageView depthImageView = nullptr;
        VmaAllocation depthAllocation = nullptr;
    };
    std::vector<SwapchainResources> swapchainResources;

    bool resetSwapchain = false;

    bool createSwapchain();
    void destroySwapchain();

    // Descriptor sets
    VkDescriptorPool glDescPool = nullptr;
    VkDescriptorSetLayout glDescSetLayout = nullptr;
    VkDescriptorSet glDescSet = nullptr;

    bool createDescriptorSets();
    void updateTextureDescriptors() const;

    // Graphics pipeline
    VkShaderModule createShaderModule(const std::string& filename);

    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline pipeline = nullptr;
    bool createGraphicsPipeline();

    // Synchronization
    struct FrameResources {
        VkCommandPool commandPool = nullptr;
        VkCommandBuffer commandBuffer = nullptr;

        VkSemaphore imageAcquiredSemaphore = nullptr;
        VkSemaphore renderFinishedSemaphore = nullptr;
    };
    std::array<FrameResources, Config::FRAMES_IN_FLIGHT> frameResources;

    VkSemaphore timelineSemaphore = nullptr;

    bool createSyncResources();
    bool initFrameResources();

    // Render
    uint64_t frameIndex = 0;
    uint64_t nextSignalValue = Config::FRAMES_IN_FLIGHT + 1;

    void render();

    // Transistent cmd pool & buffers
    VkCommandPool transistentPool = nullptr;

    bool createTransistentCommandPool();
    VkCommandBuffer startTransistentCommandBuffer();
    void submitTransistentCommandBuffer(VkCommandBuffer cmdBuffer);

    // Some helper functions
    std::pair<uint32_t, GPUbuffer> createImage(VkCommandBuffer cmdBuffer, std::byte* data, uint32_t width, uint32_t height, int channels);
    GPUbuffer createBuffer(VkBufferUsageFlags2 usage, size_t size, bool mappable, VmaMemoryUsage memoryUsage);
    void mapCopyBufferData(const GPUbuffer& buffer, size_t offset, void* data, size_t size);
    uint32_t addBuffer(const GPUbuffer& buffer);

    // Resources
    void loadGLTF(const std::string& path);

    std::vector<Image> loadImages(const tg3_model& model, fs::path& imageDir);
    std::vector<uint32_t> uploadImages(const std::vector<Image>& images);

    std::vector<uint32_t> loadSamplers(const tg3_model& model);
    std::vector<uint32_t> loadTextures(const tg3_model& model, const std::vector<uint32_t>& imageIds, const std::vector<uint32_t>& samplerIds);
    std::vector<uint32_t> loadMaterials(const tg3_model& model, const std::vector<uint32_t>& textureIds);
    std::vector<uint32_t> loadMeshes(const tg3_model& model, const std::vector<uint32_t>& materialIds);
    std::vector<uint32_t> loadNodes(const tg3_model& model, const std::vector<uint32_t>& meshIds);

    void freeData();

    uint32_t fallbackPixelImageId = 0;

    // CPU resources
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    // GPU resources
    std::vector<GPUimage> gpuImages;
    std::vector<GPUbuffer> gpuBuffers;
    std::vector<VkSampler> samplers;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    uint32_t vertexBufferId = 0;
    uint32_t indexBufferId = 0;
    uint32_t materialBufferId = 0;
};