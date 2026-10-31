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

    // Memory
    inline constexpr uint32_t MAX_TEXTURES = 1024;

    // Debug
    inline constexpr const char* MESH_DEFAULT_NAME = "Mesh";
} // namespace Config