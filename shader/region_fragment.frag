#version 460

layout(binding = 2) uniform sampler2DArray color_sampler;

layout(location = 0) in vec3 vertex_texel_coord;

layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = texture(color_sampler, vertex_texel_coord);
}