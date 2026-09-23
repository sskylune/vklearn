#define VMA_IMPLEMENTATION
#include <app.hpp>
#include <tools.hpp>

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

    if (!createGraphicsPipeline()) {
        log.fatal("Failed to create graphics pipeline.");
        return false;
    }

    if (!createSyncResources() && !initFrameResources()) {
        log.fatal("Failed to create synchronization resources.");
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
    vkDeviceWaitIdle(device);

    vkDestroySemaphore(device, timelineSemaphore, nullptr);

    for (const auto& res : frameResources) {
        vkDestroyCommandPool(device, res.commandPool, nullptr);
        vkDestroySemaphore(device, res.imageAcquiredSemaphore, nullptr);
    }

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);

    destroySwapchain();

    vmaDestroyAllocator(vmaAllocator);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    glfwTerminate();
}

bool App::initFrameResources() {
    for (auto& res : frameResources) {
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = gfxQueueIndex;
        // Написать флаги когда будет ясно поведение ресурсов кадра
        // poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSISTENT_BIT;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &res.commandPool) != VK_SUCCESS) {
            log.fatal("Failed to initialize per-frame command pool.");
            return false;
        }

        VkCommandBufferAllocateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandBufferCount = 1;
        bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferInfo.commandPool = res.commandPool;

        if (vkAllocateCommandBuffers(device, &bufferInfo, &res.commandBuffer) != VK_SUCCESS) {
            log.fatal("Failed to initialize per-frame command buffer.");
            return false;
        }

        for (auto& res : frameResources) {
            VkSemaphoreCreateInfo semaphoreInfo {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &res.imageAcquiredSemaphore) != VK_SUCCESS) {
                log.fatal("Failed to initialize per-frame image-acquire semaphore.");
                return false;
            }
        }
    }

    return true;
}

bool App::createSyncResources() {
    VkSemaphoreTypeCreateInfo semaphoreTypeInfo {};
    semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    semaphoreTypeInfo.initialValue = Config::FRAMES_IN_FLIGHT;

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &semaphoreTypeInfo;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &timelineSemaphore) != VK_SUCCESS) {
        log.fatal("Failed to create the timeline semaphore.");
        return false;
    }

    return true;
}

bool App::createGraphicsPipeline() {
    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        log.fatal("Failed to create pipeline layout.");
        return false;
    }

    VkShaderModule vertShader = createShaderModule("shader.vert", shaderc_vertex_shader);
    VkShaderModule fragShader = createShaderModule("shader.frag", shaderc_fragment_shader);

    const char* entryPoint = "main";
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(2);

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShader;
    shaderStages[0].pName = entryPoint;

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShader;
    shaderStages[1].pName = entryPoint;

    VkPipelineVertexInputStateCreateInfo vertInputInfo {};
    vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo {};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilInfo.stencilTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportInfo {};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterInfo {};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleInfo {};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment {};
    blendAttachment.blendEnable = VK_FALSE;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blendInfo {};
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &blendAttachment;

    std::vector<VkDynamicState> dynamicState { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicState.size());
    dynamicInfo.pDynamicStates = dynamicState.data();

    VkPipelineRenderingCreateInfo renderInfo {};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &swapchainFormat;
    renderInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &blendInfo;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        return false;

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    return true;
}

VkShaderModule App::createShaderModule(const std::string& filename, shaderc_shader_kind kind) {
    std::string filepath = "src/shaders/" + filename;
    std::string src = readTextFile(filepath);
    if (src.empty()) {
        log.error("Failed to create shader module. Shader source file doesn't exist.");
        return nullptr;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    shaderc::CompilationResult res = compiler.CompileGlslToSpv(src, kind, filename.c_str(), options);

    if (res.GetCompilationStatus() != shaderc_compilation_status_success) {
        log.error("Shader compilation error: {}", res.GetErrorMessage());
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo moduleInfo {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = (res.cend() - res.cbegin()) * sizeof(uint32_t);
    moduleInfo.pCode = res.cbegin();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        log.error("Failed to create shader module.");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
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
    swapchainExtent = { .width = Config::SCR_WIDTH, .height = Config::SCR_HEIGHT };

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

    if (vmaCreateImage(vmaAllocator, &depthInfo, &allocInfo, &depthImage, &depthAllocation, nullptr) != VK_SUCCESS)
        return false;

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
            logger->info("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logger->warn("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logger->error("{}", pCallbackData->pMessage);
            break;
        default:
            logger->info("{}", pCallbackData->pMessage);
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

    window = glfwCreateWindow(Config::SCR_WIDTH, Config::SCR_HEIGHT, Config::SCR_TITLE, nullptr, nullptr);

    if (!window)
        return false;

    glfwMakeContextCurrent(window);

    return true;
}