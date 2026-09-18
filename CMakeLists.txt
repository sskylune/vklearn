cmake_minimum_required(VERSION 3.20)
project(vklearn)

# spdlog
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

# Vulkan (for volk)
find_package(Vulkan REQUIRED)

# Submodules
add_subdirectory(external/glfw)
add_subdirectory(external/volk)
add_subdirectory(external/VulkanMemoryAllocator)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(VK_NO_PROTOTYPES)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    volk
    glfw
    GPUOpen::VulkanMemoryAllocator
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -fno-exceptions
    -fno-rtti
)
