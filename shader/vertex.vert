#version 450

void main() {
    vec2 p = vec2(0.0, 0.0);
    if (gl_VertexIndex == 0u) {
        p = vec2(-0.5, -0.5);
    } else if (gl_VertexIndex == 1u) {
        p = vec2(0.5, -0.5);
    } else {
        p = vec2(0.0, 0.5);
    }

    gl_Position = vec4(p, 0.0, 1.0);
}