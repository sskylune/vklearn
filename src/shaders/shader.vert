// clang-format off

#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Vertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec2 uv;
};

struct RenderItem {
    mat4 model;
    uint materialId;
};

struct Material {
    vec4 baseColor;
    uint textureId;
};

layout(push_constant, scalar) uniform FrameConstants {
    mat4 vp;
    uint64_t vertexBufferAddress;
    uint64_t materialBufferAddress;
    uint64_t renderItemBufferAddress;
} pc;

layout(buffer_reference, scalar) readonly buffer VertexPtr {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer RenderItemPtr {
    RenderItem items[];
};

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out flat uint outMaterialId;

void main() {
    VertexPtr vBuffer = VertexPtr(pc.vertexBufferAddress);
    Vertex v = vBuffer.vertices[gl_VertexIndex];

    RenderItemPtr riBuffer = RenderItemPtr(pc.renderItemBufferAddress);
    RenderItem ri = riBuffer.items[gl_InstanceIndex];

    gl_Position = pc.vp * ri.model * vec4(v.pos, 1.0);

    outColor = v.color;
    outUV = v.uv;
    outMaterialId = ri.materialId;
}