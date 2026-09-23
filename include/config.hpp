#pragma once

#include <cstdint>

namespace Config {
    // Window
    inline constexpr uint32_t SCR_WIDTH = 800;
    inline constexpr uint32_t SCR_HEIGHT = 600;
    inline constexpr const char* SCR_TITLE = "Vulkan";

    // Render
    inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;
} // namespace Config