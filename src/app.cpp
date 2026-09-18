#define VMA_IMPLEMENTATION
#include <app.hpp>
#include <config.hpp>

bool App::init() {
    if (!glfwInit()) {
        log.fatal("Failed to initialize GLFW.");
        return false;
    }

    if (!createWindow()) {
        log.fatal("Failed to create GLFW window.");
        return false;
    }

    if (!createVulkanInstance()) {
        log.fatal("Failed to create Vulkan instance.");
        return false;
    }

    if (!createVulkanSurface()) {
        log.fatal("Failed to create Vulkan surface.");
        return false;
    }

    if (!peakPhysicalDevice()) {
        log.fatal("Could't find suitable physical device.");
        return false;
    }

    if (!findGraphicsQueue()) {
        log.fatal("Could't find graphics queue.");
        return false;
    }

    if (!createDevice()) {
        log.fatal("Failed to create Vulkan device.");
        return false;
    }

    if (!initVMA()) {
        log.fatal("Failed to initialize Vulkan Memory Allocator.");
        return false;
    }

    if (!createSwapchain()) {
        log.fatal("Failed to create swapchain.");
        return false;
    }

    return true;
}

void App::loop() {
    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);

        glfwPollEvents();
    }
}

void App::exit() {
    destroySwapchain();

    vmaDestroyAllocator(vmaAllocator);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    glfwTerminate();
}

void App::destroySwapchain() {
    for (const auto& imageView : swapchainImageViews)
        vkDestroyImageView(device, imageView, nullptr);
    swapchainImageViews.clear();

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;

    vkDestroyImageView(device, depthImageView, nullptr);
    depthImageView = VK_NULL_HANDLE;
    vmaDestroyImage(vmaAllocator, depthImage, depthAllocation);
    depthImage = VK_NULL_HANDLE;
}

bool App::createSwapchain() {
    swapchainExtent = { .width = config::SCR_WIDTH, .height = config::SCR_HEIGHT };

    VkSurfaceCapabilitiesKHR surfaceCaps {};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps) != VK_SUCCESS) {
        log.fatal("Couldn't get the surface capabilities.");
        return false;
    }

    // Query at least 2 images
    uint32_t requestedImageCount = std::max(2u, surfaceCaps.minImageCount);
    if (surfaceCaps.maxImageCount > 0)
        requestedImageCount = std::min(requestedImageCount, surfaceCaps.maxImageCount);

    // Swapchain creation
    VkSwapchainCreateInfoKHR swapchainInfo {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = requestedImageCount;
    swapchainInfo.imageFormat = swapchainFormat;
    swapchainInfo.imageColorSpace = swapchainColorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.preTransform = surfaceCaps.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = swapchainPresentMode;

    if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS)
        return false;

    // Swapchain image & image view
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
    swapchainImageViews.resize(imageCount);

    for (int i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo imageViewInfo {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = swapchainImages[i];
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = swapchainFormat;
        imageViewInfo.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        if (vkCreateImageView(device, &imageViewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            log.fatal("Failed to create swapchain image view.");
            return false;
        }
    }

    // Depth buffer
    VkImageCreateInfo depthInfo {};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent = {
        .width = swapchainExtent.width,
        .height = swapchainExtent.height,
        .depth = 1,
    };
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(vmaAllocator, &depthInfo, &allocInfo, &depthImage, &depthAllocation, nullptr) != VK_SUCCESS) {
        return false;
    }

    VkImageViewCreateInfo depthViewInfo {};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    if (vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
        log.fatal("Failed to create depth image view.");
        return false;
    }

    return true;
}

bool App::initVMA() {
    VmaVulkanFunctions vmaFunctions {};

    VmaAllocatorCreateInfo vmaAllocInfo {};
    vmaAllocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaAllocInfo.physicalDevice = physicalDevice;
    vmaAllocInfo.device = device;
    vmaAllocInfo.pVulkanFunctions = &vmaFunctions;
    vmaAllocInfo.instance = instance;
    vmaAllocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFunctions);

    if (vmaCreateAllocator(&vmaAllocInfo, &vmaAllocator) != VK_SUCCESS)
        return false;

    return true;
}

bool App::createDevice() {
    VkPhysicalDeviceVulkan14Features supported14 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr };
    VkPhysicalDeviceVulkan13Features supported13 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supported14 };
    VkPhysicalDeviceVulkan12Features supported12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supported13 };
    VkPhysicalDeviceFeatures2 supported { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supported12 };
    vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);

    if (!supported13.dynamicRendering || !supported13.synchronization2 || !supported12.timelineSemaphore) {
        log.fatal("Physical device doesn't meet the feature requirements.");
        return false;
    }

    VkPhysicalDeviceVulkan14Features features14 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr,
    };
    VkPhysicalDeviceVulkan13Features features13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features14,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features features12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .timelineSemaphore = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
    };

    std::vector<float> queuePriorities { 1.0f };
    VkDeviceQueueCreateInfo queueInfo {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = gfxQueueIndex;
    queueInfo.queueCount = static_cast<uint32_t>(queuePriorities.size());
    queueInfo.pQueuePriorities = queuePriorities.data();

    const std::vector<const char*> deviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceInfo {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
        return false;

    vkGetDeviceQueue(device, gfxQueueIndex, 0, &gfxQueue);
    if (!gfxQueue) {
        log.fatal("Couldn't get the graphics queue.");
        return false;
    }

    return true;
}

bool App::findGraphicsQueue() {
    uint32_t queueFamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamCount, nullptr);
    std::vector<VkQueueFamilyProperties> props(queueFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamCount, props.data());

    for (int idx = 0; idx < props.size(); ++idx) {
        VkBool32 surfaceSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, idx, surface, &surfaceSupport);

        const auto& currProps = props[idx];
        if (currProps.queueFlags & VK_QUEUE_GRAPHICS_BIT && surfaceSupport) {
            gfxQueueIndex = idx;
            return true;
        }
    }

    return false;
}

bool App::peakPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount) {
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const VkPhysicalDevice& device : devices) {
            VkPhysicalDeviceProperties props {};
            vkGetPhysicalDeviceProperties(device, &props);

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                uint32_t surfaceFormatCount = 0;
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatCount, nullptr);

                uint32_t presentModeCount = 0;
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

                if (surfaceFormatCount && presentModeCount) {
                    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatCount, surfaceFormats.data());

                    swapchainFormat = surfaceFormats[0].format;
                    swapchainColorSpace = surfaceFormats[0].colorSpace;

                    for (const auto& surfaceFormat : surfaceFormats) {
                        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                            swapchainFormat = surfaceFormat.format;
                            swapchainColorSpace = surfaceFormat.colorSpace;
                            break;
                        }
                    }

                    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
                    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());

                    swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

                    for (const auto& presentMode : presentModes) {
                        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                            swapchainPresentMode = presentMode;
                            break;
                        }
                    }

                    physicalDevice = device;
                    return true;
                }
            }
        }
    }

    return false;
}

bool App::createVulkanSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        return false;
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL App::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    auto logger = reinterpret_cast<Log*>(pUserData);
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            logger->info(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logger->warn(pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logger->error(pCallbackData->pMessage);
            break;
        default:
            logger->info(pCallbackData->pMessage);
    }

    return VK_FALSE;
}

bool App::createVulkanInstance() {
    if (volkInitialize() != VK_SUCCESS) {
        log.fatal("Failed to initialize Volk.");
        return false;
    }

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Instance extensions
    uint32_t extensionCount = 0;
    const char** windowExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    std::vector<const char*> requestedExtensions;
    for (int i = 0; i < extensionCount; ++i)
        requestedExtensions.push_back(windowExtensions[i]);
#ifndef NDEBUG
    requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // Instance layers
    std::vector<const char*> requestedLayers;
#ifndef NDEBUG
    requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // Debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debugInfo {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    debugInfo.pUserData = &log;

    VkInstanceCreateInfo instanceInfo {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = &debugInfo;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size());
    instanceInfo.ppEnabledExtensionNames = requestedExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size());
    instanceInfo.ppEnabledLayerNames = requestedLayers.data();

    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
        return false;

    volkLoadInstance(instance);
    return true;
}

bool App::createWindow() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(config::SCR_WIDTH, config::SCR_HEIGHT, config::SCR_TITLE, nullptr, nullptr);

    if (!window)
        return false;

    glfwMakeContextCurrent(window);

    return true;
}