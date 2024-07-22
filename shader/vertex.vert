#version 450

layout(set = 0, binding = 0) uniform uniform_t {
    vec2 offset;
};

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 vert_color;

void main() {
    gl_Position = vec4(position + offset, 0.0, 1.0);
    vert_color = color;
}