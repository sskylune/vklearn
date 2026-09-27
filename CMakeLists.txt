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

# Module path for custom Find modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

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

# Shader Compilation
find_program(GLSLC glslc REQUIRED)

set(SHADER_SOURCES
    src/shaders/shader.vert
    src/shaders/shader.frag
)

set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV_FILE "${CMAKE_BINARY_DIR}/${FILE_NAME}.spv")
    
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/${SHADER} -o ${SPIRV_FILE}
        DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER}
        COMMENT "Compiling ${FILE_NAME} to SPIR-V..."
    )
    list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app.cpp
)

add_dependencies(${PROJECT_NAME} Shaders)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    volk
    glfw
    GPUOpen::VulkanMemoryAllocator
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /GR- /EHs-c-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -fno-exceptions -fno-rtti)
endif()
