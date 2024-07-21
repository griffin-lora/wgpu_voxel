#version 450

// layout(set = 0, binding = 0) uniform uniform_t {
//     float time;
// };

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 vert_color;

void main() {
    float time = 0.0;
    gl_Position = vec4(position + vec2(0.3 * cos(time), 0.3 * sin(time)), 0.0, 1.0);
    vert_color = color;
}