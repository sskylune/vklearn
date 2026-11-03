#pragma once

#include <cstdint>

namespace Config {
    // Window
    inline constexpr uint32_t SCR_WIDTH = 800;
    inline constexpr uint32_t SCR_HEIGHT = 600;
    inline constexpr const char* SCR_TITLE = "Vulkan";

    // Render
    inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;
    inline constexpr uint32_t FALLBACK_COLOR = 0x808080FF;
    inline constexpr const char* MODEL_PATH = "D:\\models\\de_dust\\scene.gltf";

    // Memory
    inline constexpr uint32_t MAX_TEXTURES = 1024;
    inline constexpr uint32_t MAX_RENDER_ITEMS = 65536;

    // Debug
    inline constexpr const char* MESH_DEFAULT_NAME = "Mesh";
} // namespace Config