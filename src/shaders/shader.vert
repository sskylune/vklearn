#version 460

const vec3 pos[3] = vec3[](vec3(0.0, -0.5, 0.0), vec3(0.5, 0.5, 0.0), vec3(-0.0, 0.5, 0.0));
const vec3 color[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = vec4(pos[gl_VertexIndex], 1.0);
    outColor = color[gl_VertexIndex];
}