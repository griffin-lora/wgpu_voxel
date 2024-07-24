#version 460

layout(set = 0, binding = 0) uniform voxel_uniform_t {
    mat4 view_projection;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texel_coord;

layout(location = 0) out vec2 vert_texel_coord;

void main() {
    gl_Position = view_projection * vec4(position, 1.0);
    vert_texel_coord = texel_coord;
}