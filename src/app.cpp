#define VMA_IMPLEMENTATION

#include <stb_image.h>
#include <type_traits>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

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

    if (!pickPhysicalDevice()) {
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

    if (!createSyncResources() || !initFrameResources()) {
        log.fatal("Failed to create synchronization resources.");
        return false;
    }

    if (!createTransistentCommandPool()) {
        log.fatal("Failed to create transistent command pool.");
        return false;
    }

    return true;
}

void App::loop() {
    while (!glfwWindowShouldClose(window)) {
        render();

        glfwPollEvents();
    }
}

void App::exit() {
    vkDeviceWaitIdle(device);

    vkDestroyCommandPool(device, transistentPool, nullptr);

    vkDestroySemaphore(device, timelineSemaphore, nullptr);

    for (const auto& res : frameResources) {
        vkDestroyCommandPool(device, res.commandPool, nullptr);
        vkDestroySemaphore(device, res.imageAcquiredSemaphore, nullptr);
        vkDestroySemaphore(device, res.renderFinishedSemaphore, nullptr);
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

void App::render() {
    if (swapchainExtent.width == 0 || swapchainExtent.height == 0)
        return;

    if (resetSwapchain) {
        vkDeviceWaitIdle(device);
        destroySwapchain();
        createSwapchain();
        resetSwapchain = false;
    }

    const uint32_t frameResIndex = frameIndex++ % Config::FRAMES_IN_FLIGHT;
    const uint64_t signalValue = nextSignalValue++;
    const uint64_t waitValue = signalValue - Config::FRAMES_IN_FLIGHT;

    VkSemaphoreWaitInfo waitInfo {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &timelineSemaphore;
    waitInfo.pValues = &waitValue;

    vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

    FrameResources& res = frameResources[frameResIndex];
    vkResetCommandPool(device, res.commandPool, 0);

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, res.imageAcquiredSemaphore, nullptr, &imageIndex);

    switch (acquireResult) {
        case VK_ERROR_OUT_OF_DATE_KHR:
            resetSwapchain = true;
            return;
        case VK_SUBOPTIMAL_KHR:
            resetSwapchain = true;
            break;
        default:
            break;
    }

    VkCommandBufferBeginInfo cmdBeginInfo {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(res.commandBuffer, &cmdBeginInfo);

    std::vector<VkImageMemoryBarrier2> barriers(2);
    // Color attachment barrier
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].image = swapchainResources[imageIndex].image;
    barriers[0].subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    // Depth barrier
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barriers[1].image = swapchainResources[imageIndex].depthImage;
    barriers[1].subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo depInfo {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    depInfo.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(res.commandBuffer, &depInfo);

    VkRenderingAttachmentInfo colorAttachInfo {};
    colorAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachInfo.imageView = swapchainResources[imageIndex].imageView;
    colorAttachInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachInfo.clearValue = { .color { { 0.01f, 0.01f, 0.01f, 1 } } };

    VkRenderingAttachmentInfo depthAttachInfo {};
    depthAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachInfo.imageView = swapchainResources[imageIndex].depthImageView;
    depthAttachInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachInfo.clearValue = { .depthStencil { 1.0f, 0 } };

    VkRenderingInfo renderInfo {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = {
        .offset { .x = 0, .y = 0 },
        .extent = swapchainExtent,
    };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachInfo;
    renderInfo.pDepthAttachment = &depthAttachInfo;

    vkCmdBeginRendering(res.commandBuffer, &renderInfo);

    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = static_cast<float>(swapchainExtent.width),
        .height = static_cast<float>(swapchainExtent.height),
    };
    vkCmdSetViewport(res.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = swapchainExtent,
    };
    vkCmdSetScissor(res.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(res.commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(res.commandBuffer);

    VkImageMemoryBarrier2 presentLayoutBarrier {};
    presentLayoutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    presentLayoutBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    presentLayoutBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    presentLayoutBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    presentLayoutBarrier.dstAccessMask = 0;
    presentLayoutBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    presentLayoutBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentLayoutBarrier.image = swapchainResources[imageIndex].image;
    presentLayoutBarrier.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo presentDepInfo {};
    presentDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    presentDepInfo.imageMemoryBarrierCount = 1;
    presentDepInfo.pImageMemoryBarriers = &presentLayoutBarrier;

    vkCmdPipelineBarrier2(res.commandBuffer, &presentDepInfo);

    vkEndCommandBuffer(res.commandBuffer);

    VkSemaphoreSubmitInfo imageAcquireWaitInfo {};
    imageAcquireWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    imageAcquireWaitInfo.semaphore = res.imageAcquiredSemaphore;
    imageAcquireWaitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    std::vector<VkSemaphoreSubmitInfo> signalSemaphores(2);
    signalSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphores[0].semaphore = timelineSemaphore;
    signalSemaphores[0].value = signalValue;
    signalSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    signalSemaphores[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphores[1].semaphore = res.renderFinishedSemaphore;
    signalSemaphores[1].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo cmdSubmitInfo {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = res.commandBuffer;

    VkSubmitInfo2 submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &imageAcquireWaitInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphoreInfos = signalSemaphores.data();

    vkQueueSubmit2(gfxQueue, 1, &submitInfo, nullptr);

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &res.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(gfxQueue, &presentInfo);
}

bool App::loadData() {
    constexpr size_t vertexBufferBytes = 64 * 1024 * 1024;
    constexpr size_t indexBufferBytes = 32 * 1024 * 1024;
    constexpr size_t totalVertices = vertexBufferBytes / sizeof(Vertex);
    constexpr size_t totalIndices = indexBufferBytes / sizeof(uint32_t);

    vertices.resize(totalVertices);
    indices.resize(totalIndices);

    uint32_t fallbackColor = Config::FALLBACK_COLOR;

    Image fallbackPixel {
        .width = 1,
        .height = 1,
        .channels = 4,
        .data = reinterpret_cast<std::byte*>(&fallbackColor),
    };

    VkCommandBuffer fallbackCmdBuffer = startTransistentCommandBuffer();
    auto [fallbackPixelImageId, fallfackStagingBuffer] =
        createImage(fallbackCmdBuffer, fallbackPixel.data, fallbackPixel.width, fallbackPixel.height, fallbackPixel.channels);
    submitTransistentCommandBuffer(fallbackCmdBuffer);
    vmaDestroyBuffer(vmaAllocator, fallfackStagingBuffer.buffer, fallfackStagingBuffer.allocation);

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.compareEnable = VK_FALSE;

    VkSampler fallbackSampler = nullptr;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &fallbackSampler) != VK_SUCCESS) {
        log.fatal("Failed to create fallback sampler.");
        return false;
    }

    samplers.push_back(fallbackSampler);
    uint32_t fallbackSamplerId = samplers.size();

    textures.push_back(Texture {
        .imageId = fallbackPixelImageId,
        .samplerId = fallbackSamplerId,
    });

    loadGLTF("D:\\models\\de_dust\\scene.gltf");

    return true;
}

void App::loadGLTF(const std::string& path) {
    if (!fs::exists(path)) {
        log.error("loadGLTF: File doesn't exist.");
        return;
    }

    tg3_parse_options opts;
    tg3_error_stack errors;
    tg3_model model;

    tg3_parse_options_init(&opts);
    tg3_error_stack_init(&errors);

    if (tg3_parse_file(&model, &errors, path.c_str(), path.length(), &opts) != TG3_OK) {
        log.error("loadGLTF: Failed to parse glTF file, errors found");
        for (uint32_t i = 0; i < errors.count; ++i)
            log.error("{}", errors.entries[i].message);
        tg3_error_stack_free(&errors);
        return;
    }
    tg3_error_stack_free(&errors);

    fs::path imageDir = fs::path(path).parent_path();
    std::vector<Image> images = loadImages(model, imageDir);
    std::vector<uint32_t> imageIds = uploadImages(images);

    for (const Image& image : images)
        stbi_image_free(reinterpret_cast<stbi_uc*>(image.data));

    std::vector<uint32_t> samplerIds = loadSamplers(model);
    std::vector<uint32_t> textureIds = loadTextures(model, imageIds, samplerIds);
    std::vector<uint32_t> materialIds = loadMaterials(model, textureIds);
    std::vector<uint32_t> meshIds = loadMeshes(model, materialIds);

    std::vector<uint32_t> nodeIds = loadNodes(model, meshIds);

    log.info("loadGLTF: GLTF loading successful.");
}

std::vector<uint32_t> App::loadNodes(const tg3_model& model, const std::vector<uint32_t>& meshIds) {
    if (model.nodes_count == 0)
        return {};

    // Finding root nodes of scene
    std::vector<bool> isChild(model.nodes_count, false);
    for (uint32_t i = 0; i < model.nodes_count; ++i)
        for (uint32_t j = 0; j < model.nodes[i].children_count; ++j)
            isChild[model.nodes[i].children[j]] = true;

    std::vector<uint32_t> nodesId(model.nodes_count);

    auto processNode = [&](auto& self, uint32_t idx, int32_t parentIdx) -> void {
        const tg3_node& tg3node = model.nodes[idx];
        Node node {};

        if (tg3node.has_matrix) {
            node.localMtx = glm::mat4(glm::make_mat4(tg3node.matrix));
        } else {
            glm::vec3 t = {
                static_cast<float>(tg3node.translation[0]),
                static_cast<float>(tg3node.translation[1]),
                static_cast<float>(tg3node.translation[2]),
            };
            glm::quat r = {
                static_cast<float>(tg3node.rotation[3]),
                static_cast<float>(tg3node.rotation[0]),
                static_cast<float>(tg3node.rotation[1]),
                static_cast<float>(tg3node.rotation[2]),
            };
            glm::vec3 s = {
                static_cast<float>(tg3node.scale[0]),
                static_cast<float>(tg3node.scale[1]),
                static_cast<float>(tg3node.scale[2]),
            };

            node.localMtx = glm::translate(glm::mat4 { 1.0f }, t) * glm::mat4_cast(r) * glm::scale(glm::mat4 { 1.0f }, s);
        }

        node.meshId = (tg3node.mesh != -1) ? meshIds[tg3node.mesh] - 1 : -1;
        node.parentId = parentIdx;

        int32_t currIdx = static_cast<int32_t>(nodes.size());
        nodes.push_back(std::move(node));
        nodesId[idx] = nodes.size();

        for (uint32_t i = 0; i < tg3node.children_count; ++i)
            self(self, static_cast<uint32_t>(tg3node.children[i]), currIdx);
    };

    // Processing nodes for every root
    for (uint32_t i = 0; i < model.nodes_count; ++i)
        if (!isChild[i])
            processNode(processNode, i, -1);

    return nodesId;
}

std::vector<uint32_t> App::loadMeshes(const tg3_model& model, const std::vector<uint32_t>& materialIds) {
    std::vector<uint32_t> meshIds(model.meshes_count);

    auto writeAttribute = [this, &model]<typename T>(T Vertex::* member, const tg3_str_int_pair& attribute) {
        const tg3_accessor& acc = model.accessors[attribute.value];
        const tg3_buffer_view& bv = model.buffer_views[acc.buffer_view];
        const tg3_buffer& buff = model.buffers[bv.buffer];

        const size_t buff_offset = bv.byte_offset + acc.byte_offset;
        const size_t stride = bv.byte_stride != 0 ? bv.byte_stride : sizeof(T);

        for (size_t i = 0; i < acc.count; ++i) {
            const size_t vert_offset = buff_offset + i * stride;
            const float* data = reinterpret_cast<const float*>(buff.data.data + vert_offset);

            if constexpr (std::is_same_v<T, glm::vec3>) {
                vertices[vertex_offset + i].*member = glm::vec3(data[0], data[1], data[2]);
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
                vertices[vertex_offset + i].*member = glm::vec2(data[0], data[1]);
            }
        }
    };

    for (uint32_t i = 0; i < model.meshes_count; ++i) {
        Mesh mesh {};

        const tg3_mesh* tg3mesh = &model.meshes[i];
        mesh.name = (tg3mesh->name.data != nullptr) ? tg3mesh->name.data : Config::MESH_DEFAULT_NAME;

        mesh.primitives.resize(tg3mesh->primitives_count);
        for (uint32_t j = 0; j < tg3mesh->primitives_count; ++j) {
            const tg3_primitive& tg3primitive = tg3mesh->primitives[j];

            mesh.primitives[j].vertexStart = vertex_offset;
            mesh.primitives[j].materialId = (tg3primitive.material != -1) ? materialIds[tg3primitive.material] : 0;

            for (uint32_t k = 0; k < tg3primitive.attributes_count; ++k) {
                const tg3_str_int_pair& attribute = tg3primitive.attributes[k];
                std::string_view key(attribute.key.data, attribute.key.len);

                if (key == "POSITION") {
                    const tg3_accessor& acc = model.accessors[attribute.value];
                    assert(acc.type == TG3_TYPE_VEC3 && acc.component_type == TG3_COMPONENT_TYPE_FLOAT);
                    assert(vertex_offset + acc.count <= vertices.size() && "Not enough space for vertices.");

                    mesh.primitives[j].vertexCount = acc.count;
                    writeAttribute(&Vertex::pos, attribute);
                }
                if (key == "NORMAL") {
                    const tg3_accessor& acc = model.accessors[attribute.value];
                    assert(acc.type == TG3_TYPE_VEC3 && acc.component_type == TG3_COMPONENT_TYPE_FLOAT);

                    writeAttribute(&Vertex::normal, attribute);
                }
                if (key == "COLOR_0") {
                    const tg3_accessor& acc = model.accessors[attribute.value];
                    assert(acc.type == TG3_TYPE_VEC3 || acc.type == TG3_TYPE_VEC4);
                    assert(acc.component_type == TG3_COMPONENT_TYPE_FLOAT);

                    writeAttribute(&Vertex::color, attribute);
                }
                if (key == "TEXCOORD_0") {
                    const tg3_accessor& acc = model.accessors[attribute.value];
                    assert(acc.type == TG3_TYPE_VEC2 && acc.component_type == TG3_COMPONENT_TYPE_FLOAT);

                    writeAttribute(&Vertex::uv, attribute);
                }
            }
            vertex_offset += mesh.primitives[j].vertexCount;

            if (tg3primitive.indices != -1) {
                const tg3_accessor& acc = model.accessors[tg3primitive.indices];
                const tg3_buffer_view& bv = model.buffer_views[acc.buffer_view];
                const tg3_buffer& buff = model.buffers[bv.buffer];
                assert(index_offset + acc.count <= indices.size() && "Not enough space for indices.");

                mesh.primitives[j].indexStart = index_offset;
                mesh.primitives[j].indexCount = acc.count;

                if (acc.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* data = reinterpret_cast<const uint32_t*>(buff.data.data + bv.byte_offset + acc.byte_offset);
                    memcpy(&indices[index_offset], data, acc.count * sizeof(uint32_t));
                } else if (acc.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* data = reinterpret_cast<const uint16_t*>(buff.data.data + bv.byte_offset + acc.byte_offset);
                    for (uint64_t idx = 0; idx < acc.count; ++idx)
                        indices[index_offset + idx] = static_cast<uint32_t>(data[idx]);
                }
            }
            index_offset += mesh.primitives[j].indexCount;
        }

        meshes.push_back(std::move(mesh));
        meshIds[i] = meshes.size();
    }

    return meshIds;
}

std::vector<uint32_t> App::loadMaterials(const tg3_model& model, const std::vector<uint32_t>& textureIds) {
    std::vector<uint32_t> materialIds(model.materials_count);
    for (uint32_t i = 0; i < model.materials_count; ++i) {
        const tg3_material& material = model.materials[i];

        materials.push_back(Material {
            .baseColor = glm::vec4(material.pbr_metallic_roughness.base_color_factor[0], material.pbr_metallic_roughness.base_color_factor[1],
                material.pbr_metallic_roughness.base_color_factor[2], material.pbr_metallic_roughness.base_color_factor[3]),
            // TODO
            .textureId = material.pbr_metallic_roughness.base_color_texture.index != -1
                             ? textureIds[material.pbr_metallic_roughness.base_color_texture.index] - 1
                             : 0,
        });

        materialIds[i] = materials.size();
    }
    return materialIds;
}

std::vector<uint32_t> App::loadTextures(const tg3_model& model, const std::vector<uint32_t>& imageIds, const std::vector<uint32_t>& samplerIds) {
    assert(textures.size() + model.textures_count <= Config::MAX_TEXTURES && "Exceeding max texture count.");

    std::vector<uint32_t> textureIds(model.textures_count);
    for (uint32_t i = 0; i < model.textures_count; ++i) {
        const tg3_texture& texture = model.textures[i];
        textures.push_back(Texture { .imageId = imageIds[texture.source], .samplerId = samplerIds[texture.sampler] });
        textureIds[i] = textures.size();
    }
    return textureIds;
}

std::vector<uint32_t> App::loadSamplers(const tg3_model& model) {
    std::vector<uint32_t> samplerIds(model.samplers_count);
    for (uint32_t i = 0; i < model.samplers_count; ++i) {
        const tg3_sampler& tg_sampler = model.samplers[i];

        // TG3 to VK mappings
        static const std::unordered_map<int32_t, std::tuple<VkFilter, VkSamplerMipmapMode, float>> filterMap {
            { TG3_TEXTURE_FILTER_NEAREST, { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.25f } },
            { TG3_TEXTURE_FILTER_LINEAR, { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.25f } },
            { TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_LOD_CLAMP_NONE } },
            { TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_LOD_CLAMP_NONE } },
            { TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR, { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_LOD_CLAMP_NONE } },
            { TG3_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST, { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_LOD_CLAMP_NONE } },
        };

        auto getFilterParams = [](int32_t filterMode) -> std::tuple<VkFilter, VkSamplerMipmapMode, float> {
            if (auto it = filterMap.find(filterMode); it != filterMap.end())
                return it->second;
            return { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_LOD_CLAMP_NONE };
        };

        static const std::unordered_map<int32_t, VkSamplerAddressMode> wrapMap {
            { TG3_TEXTURE_WRAP_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT },
            { TG3_TEXTURE_WRAP_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT },
            { TG3_TEXTURE_WRAP_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
        };

        auto resolveWrapMode = [](int32_t wrapMode) -> VkSamplerAddressMode {
            if (auto it = wrapMap.find(wrapMode); it != wrapMap.end())
                return it->second;
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        };

        auto [minFilter, mipmapMode, maxLod] = getFilterParams(tg_sampler.min_filter);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = (tg_sampler.mag_filter == TG3_TEXTURE_FILTER_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        samplerInfo.minFilter = minFilter;
        samplerInfo.mipmapMode = mipmapMode;
        samplerInfo.addressModeU = resolveWrapMode(tg_sampler.wrap_s);
        samplerInfo.addressModeV = resolveWrapMode(tg_sampler.wrap_t);
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = maxLod;

        VkSampler sampler = nullptr;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            log.error("loadSamplers: Failed to create texture sampler.");
            samplerIds[i] = textures[0].samplerId;
        }

        samplers.push_back(sampler);
        samplerIds[i] = samplers.size();
    }

    return samplerIds;
}

std::vector<uint32_t> App::uploadImages(const std::vector<Image>& images) {
    VkCommandBuffer cmdBuffer = startTransistentCommandBuffer();
    std::vector<GPUbuffer> stagingBuffers;
    stagingBuffers.reserve(images.size());

    std::vector<uint32_t> imageIds(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        const Image& image = images[i];

        if (image.data) {
            auto [imageId, stagingBuffer] = createImage(cmdBuffer, image.data, image.width, image.height, image.channels);

            imageIds[i] = imageId;
            stagingBuffers.push_back(stagingBuffer);
        } else {
            imageIds[i] = fallbackPixelImageId;
        }
    }

    submitTransistentCommandBuffer(cmdBuffer);

    for (GPUbuffer& stagingBuffer : stagingBuffers)
        vmaDestroyBuffer(vmaAllocator, stagingBuffer.buffer, stagingBuffer.allocation);
    return imageIds;
}

std::vector<Image> App::loadImages(const tg3_model& model, fs::path& imageDir) {
    std::vector<Image> images(model.images_count);
    for (uint32_t i = 0; i < model.images_count; ++i) {
        Image& image = images[i];
        fs::path path = imageDir / model.images[i].uri.data;
        log.info("Loading image {}/{}: {}", i + 1, model.images_count, model.images[i].uri.data);
        image.data = reinterpret_cast<std::byte*>(stbi_load(path.string().c_str(), &image.width, &image.height, &image.channels, 4));

        if (!image.data)
            log.error("Failed to load image: {}", path.string());
    }
    return images;
}

GPUbuffer App::createBuffer(VkBufferUsageFlags2 usage, size_t size, bool mappable, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.flags = mappable ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0u;
    allocInfo.usage = memoryUsage;

    GPUbuffer gpuBuffer;
    if (vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &gpuBuffer.buffer, &gpuBuffer.allocation, nullptr) != VK_SUCCESS) {
        log.error("createBuffer: Failed to allocate buffer.");
        return GPUbuffer {};
    }

    // some code here later

    return gpuBuffer;
}

void App::mapCopyBufferData(const GPUbuffer& buffer, size_t offset, void* data, size_t size) {
    void* bufferPtr = nullptr;
    if (vmaMapMemory(vmaAllocator, buffer.allocation, &bufferPtr) != VK_SUCCESS) {
        log.error("mapCopyBufferData: Failed to map buffer memory.");
        return;
    }
    std::byte* dst = static_cast<std::byte*>(bufferPtr) + offset;
    std::memcpy(dst, data, size);

    vmaUnmapMemory(vmaAllocator, buffer.allocation);
}

std::pair<uint32_t, GPUbuffer> App::createImage(VkCommandBuffer cmdBuffer, std::byte* data, uint32_t width, uint32_t height, int channels) {
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent = { .width = width, .height = height, .depth = 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    GPUimage gpuImage;
    if (vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &gpuImage.image, &gpuImage.allocation, nullptr) != VK_SUCCESS) {
        log.error("createImage: Failed to allocate image.");
        return { 0, GPUbuffer {} };
    }

    VkImageViewCreateInfo imageViewInfo {};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = gpuImage.image;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = imageFormat;
    imageViewInfo.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    if (vkCreateImageView(device, &imageViewInfo, nullptr, &gpuImage.imageView) != VK_SUCCESS) {
        log.error("createImage: Failed to create image view.");
        return { 0, GPUbuffer {} };
    }

    VkImageMemoryBarrier2 transferBarrier {};
    transferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    transferBarrier.image = gpuImage.image;
    transferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    transferBarrier.srcAccessMask = 0;
    transferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    transferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    transferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transferBarrier.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo transferDepInfo {};
    transferDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    transferDepInfo.imageMemoryBarrierCount = 1;
    transferDepInfo.pImageMemoryBarriers = &transferBarrier;

    vkCmdPipelineBarrier2(cmdBuffer, &transferDepInfo);

    const size_t size = width * height * channels;
    GPUbuffer stagingBuffer = createBuffer(VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, size, true, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
    mapCopyBufferData(stagingBuffer, 0, data, size);

    VkBufferImageCopy imgCopy {};
    imgCopy.imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    imgCopy.imageExtent = { .width = width, .height = height, .depth = 1 };
    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer.buffer, gpuImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopy);

    VkImageMemoryBarrier2 shaderBarrier {};
    shaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    shaderBarrier.image = gpuImage.image;
    shaderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    shaderBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    shaderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    shaderBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    shaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    shaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shaderBarrier.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo shaderDepInfo {};
    shaderDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    shaderDepInfo.imageMemoryBarrierCount = 1;
    shaderDepInfo.pImageMemoryBarriers = &shaderBarrier;

    vkCmdPipelineBarrier2(cmdBuffer, &shaderDepInfo);

    gpuImages.push_back(gpuImage);
    uint32_t imageId = gpuImages.size();
    return { imageId, stagingBuffer };
}

void App::submitTransistentCommandBuffer(VkCommandBuffer cmdBuffer) {
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(gfxQueue, 1, &submitInfo, nullptr);
    // Костыль, заменить на нормальную синхронизацию
    vkQueueWaitIdle(gfxQueue);
    vkFreeCommandBuffers(device, transistentPool, 1, &cmdBuffer);
}

VkCommandBuffer App::startTransistentCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = transistentPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer = nullptr;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer) != VK_SUCCESS) {
        log.error("Failed to allocate transistent command buffer.");
        return nullptr;
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        log.error("Failed to begin transistent command buffer.");
        vkFreeCommandBuffers(device, transistentPool, 1, &cmdBuffer);
        return nullptr;
    }

    return cmdBuffer;
}

bool App::createTransistentCommandPool() {
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = gfxQueueIndex;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &transistentPool) != VK_SUCCESS)
        return false;

    return true;
}

bool App::initFrameResources() {
    for (auto& res : frameResources) {
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = gfxQueueIndex;

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

        VkSemaphoreCreateInfo imageAcquiredSemaphoreInfo {};
        imageAcquiredSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device, &imageAcquiredSemaphoreInfo, nullptr, &res.imageAcquiredSemaphore) != VK_SUCCESS) {
            log.fatal("Failed to initialize per-frame image-acquire semaphore.");
            return false;
        }

        VkSemaphoreCreateInfo renderFinishedSemaphoreInfo {};
        renderFinishedSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device, &renderFinishedSemaphoreInfo, nullptr, &res.renderFinishedSemaphore) != VK_SUCCESS) {
            log.fatal("Failed to initialize per-frame render-finished semaphore.");
            return false;
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

    VkShaderModule vertShader = createShaderModule("build/shader.vert.spv");
    VkShaderModule fragShader = createShaderModule("build/shader.frag.spv");

    if (vertShader == nullptr || fragShader == nullptr) {
        log.fatal("Failed to create shader modules.");
        return false;
    }

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
    pipelineInfo.renderPass = nullptr;

    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        return false;

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    return true;
}

VkShaderModule App::createShaderModule(const std::string& filename) {
    auto shader = readBinaryFile(filename);
    if (shader.empty()) {
        log.error("Failed to read shader file: {}", filename);
        return nullptr;
    }

    VkShaderModuleCreateInfo moduleInfo {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = shader.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(shader.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        log.error("Failed to create shader module.");
        return nullptr;
    }

    return shaderModule;
}

void App::destroySwapchain() {
    for (auto& res : swapchainResources) {
        vkDestroyImageView(device, res.imageView, nullptr);

        vkDestroyImageView(device, res.depthImageView, nullptr);
        res.depthImageView = nullptr;
        vmaDestroyImage(vmaAllocator, res.depthImage, res.depthAllocation);
        res.depthImage = nullptr;
    }
    swapchainResources.clear();

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = nullptr;
}

bool App::createSwapchain() {
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
    std::vector<VkImage> buffer(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, buffer.data());

    swapchainResources.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        swapchainResources[i].image = buffer[i];

        VkImageViewCreateInfo imageViewInfo {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = swapchainResources[i].image;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = swapchainFormat;
        imageViewInfo.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        if (vkCreateImageView(device, &imageViewInfo, nullptr, &swapchainResources[i].imageView) != VK_SUCCESS) {
            log.fatal("Failed to create swapchain image view.");
            return false;
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

        if (vmaCreateImage(vmaAllocator, &depthInfo, &allocInfo, &swapchainResources[i].depthImage, &swapchainResources[i].depthAllocation,
                nullptr) != VK_SUCCESS) {
            log.fatal("Failed to allocate depth buffer.");
            return false;
        }

        VkImageViewCreateInfo depthViewInfo {};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = swapchainResources[i].depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        if (vkCreateImageView(device, &depthViewInfo, nullptr, &swapchainResources[i].depthImageView) != VK_SUCCESS) {
            log.fatal("Failed to create depth image view.");
            return false;
        }
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

    if (!supported13.dynamicRendering || !supported13.synchronization2 || !supported12.timelineSemaphore || !supported12.bufferDeviceAddress) {
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
        .bufferDeviceAddress = VK_TRUE,
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

    for (size_t idx = 0; idx < props.size(); ++idx) {
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

bool App::pickPhysicalDevice() {
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
    std::vector<const char*> requiredExtensions;
    for (uint32_t i = 0; i < extensionCount; ++i)
        requiredExtensions.push_back(windowExtensions[i]);
#ifndef NDEBUG
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // Instance layers
    std::vector<const char*> requiredLayers;
#ifndef NDEBUG
    requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
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
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceInfo.ppEnabledExtensionNames = requiredExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    instanceInfo.ppEnabledLayerNames = requiredLayers.data();

    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
        return false;

    volkLoadInstance(instance);
    return true;
}

void App::glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (app) {
        app->swapchainExtent = {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
        };

        app->resetSwapchain = true;
    }
}

bool App::createWindow() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(Config::SCR_WIDTH, Config::SCR_HEIGHT, Config::SCR_TITLE, nullptr, nullptr);

    if (!window)
        return false;

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);

    return true;
}