#version 460

layout(binding = 1) uniform texture2DArray tex;
layout(binding = 2) uniform sampler tex_sampler;

layout(location = 0) in vec3 vert_texel_coord;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(sampler2DArray(tex, tex_sampler), vert_texel_coord);
}