#version 460

layout(binding = 2) uniform sampler2DArray color_sampler;

layout(location = 0) in vec3 vert_texel_coord;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(color_sampler, vert_texel_coord);
}