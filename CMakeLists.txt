cmake_minimum_required(VERSION 3.20)
project(vklearn)

# Vulkan
find_package(Vulkan REQUIRED)

# Volk
add_library(volk STATIC "$ENV{VULKAN_SDK}/Include/Volk/volk.c")
target_include_directories(volk PUBLIC "$ENV{VULKAN_SDK}/Include/Volk")
target_link_libraries(volk PUBLIC Vulkan::Vulkan)

# GLFW
add_subdirectory(external/glfw)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(VK_NO_PROTOTYPES)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -fno-exceptions
    -fno-rtti
)
