#version 450

layout(binding = 1) uniform texture2D tex;
layout(binding = 2) uniform sampler tex_sampler;

layout(location = 0) in vec3 vert_color;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = vec4(vert_color, 1.0);
    frag_color = texture(sampler2D(tex, tex_sampler), vec2(1.0, 1.0));
}