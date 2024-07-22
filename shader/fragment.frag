#version 450

layout(binding = 1) uniform texture2D tex;
layout(binding = 2) uniform sampler tex_sampler;

layout(location = 0) in vec2 vert_tex_coord;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, tex_sampler), vert_tex_coord);
}