#version 460

layout(set = 0, binding = 0) uniform uniform_t {
    vec2 offset;
};

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texel_coord;

layout(location = 0) out vec2 vert_texel_coord;

void main() {
    gl_Position = vec4(position + offset, 0.0, 1.0);
    vert_texel_coord = texel_coord;
}