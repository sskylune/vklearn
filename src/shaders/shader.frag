// clang-format off

#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(push_constant, scalar) uniform FrameConstants {
    mat4 vp;
    uint64_t vertexBufferAddress;
    uint64_t materialBufferAddress;
    uint64_t renderItemBufferAddress;
} pc;

struct Material {
    vec4 baseColor;
    uint textureId;
};

layout(buffer_reference, scalar) readonly buffer MaterialPtr {
    Material materials[];
};

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat uint inMaterialID;

layout(location = 0) out vec4 fragColor;

void main() {
    MaterialPtr matBuffer = MaterialPtr(pc.materialBufferAddress);
    Material mat = matBuffer.materials[inMaterialID];

    vec4 texColor = texture(textures[mat.textureId], inUV);
    fragColor = texColor * mat.baseColor * vec4(inColor, 1.0);
}