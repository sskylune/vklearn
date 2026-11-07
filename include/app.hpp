#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <log.hpp>
#include <config.hpp>
#include <volk.h>
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <tiny_gltf_v3.h>

#include <vector>
#include <chrono>
#include <filesystem>
namespace fs = std::filesystem;

struct Camera {
    glm::vec3 pos { 0.0f };

    glm::vec3 forward { 0.0f, 0.0f, 1.0f };
    glm::vec3 right { 1.0f, 0.0f, 0.0f };
    glm::vec3 up { 0.0f, 1.0f, 0.0f };

    float fov = 90.0f;
    float nearPlane = 0.01f;
    float farPlane = 1000.0f;

    float yaw = -90.0f;
    float pitch = 0.0f;

    void update() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward = glm::normalize(front);

        right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, forward));
    }

    [[nodiscard]]
    glm::mat4 getViewMatrix() {
        return glm::lookAt(pos, pos + forward, up);
    }

    [[nodiscard]]
    glm::mat4 getProjectionMatrix(float aspect) {
        return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    }
};

struct Node {
    glm::mat4 localMtx { 1.0f };
    glm::mat4 worldMtx { 1.0f };
    int32_t parentId = -1;
    int32_t meshId = -1;
};

struct AABB {
    glm::vec3 min { 0.0f };
    glm::vec3 max { 0.0f };
};

struct Primitive {
    uint64_t vertexStart = 0;
    uint64_t vertexCount = 0;
    uint64_t indexStart = 0;
    uint64_t indexCount = 0;
    uint32_t materialId = 0;
    AABB aabb {
        glm::vec3 { 0.0f },
        glm::vec3 { 0.0f },
    };
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

struct RenderObject {
    // Render Item & frustum culling
    AABB aabb {
        glm::vec3 { 0.0f },
        glm::vec3 { 0.0f },
    };
    glm::mat4 world { 1.0f };
    uint32_t materialId = 0;

    // MDI
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
};

struct RenderItem {
    glm::mat4 world { 1.0f };
    uint32_t materialId = -1;
};

struct FrameResources {
    VkCommandPool commandPool = nullptr;
    VkCommandBuffer commandBuffer = nullptr;

    VkSemaphore imageAcquiredSemaphore = nullptr;
    VkSemaphore renderFinishedSemaphore = nullptr;

    // MDI buffer
    GPUbuffer mdiBuffer;

    // Render item buffer
    GPUbuffer renderItemBuffer;

    // Render object buffer
    GPUbuffer renderObjectBuffer;
    RenderObject* renderObjectBufferPtr = nullptr;

    // Atomic counter buffer
    GPUbuffer counterBuffer;
    uint32_t* counterPtr = nullptr;
};

struct ComputeConstants {
    uint64_t renderItemBufferAddress = 0;
    uint64_t mdiBufferAddress = 0;
    uint64_t renderObjectBufferAddress = 0;
    uint64_t atomicBufferCounterAddress = 0;
    glm::mat4 vp { 1.0f };
    uint32_t totalObjects = 0;
};

struct FrameConstants {
    glm::mat4 vp { 1.0f };
    uint64_t vertexBufferAddress = 0;
    uint64_t materialBufferAddress = 0;
    uint64_t renderItemBufferAddress = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color { 1.0f };
    glm::vec3 normal;
    glm::vec2 uv;
};

class Time {
public:
    static void update() {
        auto currTime = std::chrono::steady_clock::now();
        dt = std::chrono::duration<float>(currTime - lastTime).count();
        lastTime = currTime;
    }

    [[nodiscard]]
    static float deltaTime() {
        return dt;
    }

private:
    inline static float dt = 0.0f;
    inline static auto lastTime = std::chrono::steady_clock::now();
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

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

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
    float aspectRatio = Config::SCR_WIDTH / static_cast<float>(Config::SCR_HEIGHT);

    VkFormat swapchainFormat;
    VkColorSpaceKHR swapchainColorSpace;
    VkPresentModeKHR swapchainPresentMode;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    uint32_t imageCount = 0;
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

    // IMGUI
    VkDescriptorPool imguiDescPool = nullptr;

    bool initImGui();

    // Compute pipeline
    VkPipelineLayout computePipelineLayout = nullptr;
    VkPipeline computePipeline = nullptr;

    bool createComputePipeline();

    // Graphics pipeline
    VkShaderModule createShaderModule(const std::string& filename);

    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline pipeline = nullptr;
    bool createGraphicsPipeline();

    // Synchronization
    std::array<FrameResources, Config::FRAMES_IN_FLIGHT> frameResources;
    VkSemaphore timelineSemaphore = nullptr;

    uint32_t drawObjectsCount = 0;

    bool createSyncResources();
    bool initFrameResources();
    void freeFrameResources();

    // Render
    uint64_t frameIndex = 0;
    uint64_t nextSignalValue = Config::FRAMES_IN_FLIGHT + 1;

    void render();

    // Transient cmd pool & buffers
    VkCommandPool transientPool = nullptr;

    bool createTransientCommandPool();
    VkCommandBuffer startTransientCommandBuffer();
    void submitTransientCommandBuffer(VkCommandBuffer cmdBuffer);

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

    // Camera related
    Camera camera;

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