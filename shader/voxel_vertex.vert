#version 460

#define NUM_CUBE_VOXEL_VERTICES 36u
#define NUM_CUBE_VOXEL_FACE_VERTICES 6u

struct vertex_t {
    vec3 position;
    vec2 texel_coord;
};

vertex_t cube_vertices[NUM_CUBE_VOXEL_VERTICES] = {
    // +X
    vertex_t(vec3(1.0, 1.0, -0.0), vec2(1.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(1.0, 0.0, -1.0), vec2(-0.0, 1.0)),
    vertex_t(vec3(1.0, 1.0, -0.0), vec2(1.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -1.0), vec2(-0.0, 1.0)),
    vertex_t(vec3(1.0, 1.0, -1.0), vec2(0.0, 0.0)),
    // -X
    vertex_t(vec3(0.0, 0.0, -0.0), vec2(-0.0, 1.0)),
    vertex_t(vec3(-0.0, 1.0, -0.0), vec2(0.0, 0.0)),
    vertex_t(vec3(-0.0, 1.0, -1.0), vec2(1.0, 0.0)),
    vertex_t(vec3(0.0, 0.0, -0.0), vec2(-0.0, 1.0)),
    vertex_t(vec3(-0.0, 1.0, -1.0), vec2(1.0, 0.0)),
    vertex_t(vec3(0.0, 0.0, -1.0), vec2(1.0, 1.0)),
    // +Y
    vertex_t(vec3(0.0, 1.0, -0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(1.0, 1.0, -0.0), vec2(0.0, 1.0)),
    vertex_t(vec3(1.0, 1.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(0.0, 1.0, -0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(1.0, 1.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(0.0, 1.0, -1.0), vec2(1.0, 0.0)),
    // -Y
    vertex_t(vec3(0.0, 0.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -1.0), vec2(1.0, 0.0)),
    vertex_t(vec3(1.0, -0.0, -0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, 0.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, -0.0, -0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, -0.0, -0.0), vec2(0.0, 1.0)),
    // +Z
    vertex_t(vec3(0.0, 0.0, -0.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -0.0), vec2(1.0, 0.0)),
    vertex_t(vec3(1.0, 1.0, 0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, 0.0, -0.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, 1.0, 0.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, 1.0, 0.0), vec2(0.0, 1.0)),
    // -Z
    vertex_t(vec3(0.0, 1.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, 1.0, -1.0), vec2(1.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -1.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, 1.0, -1.0), vec2(0.0, 0.0)),
    vertex_t(vec3(1.0, 0.0, -1.0), vec2(1.0, 1.0)),
    vertex_t(vec3(0.0, 0.0, -1.0), vec2(0.0, 1.0))
};

layout(set = 0, binding = 0) uniform voxel_uniform_t {
    mat4 view_projection;
};

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in uint vertex_index;

layout(location = 0) out vec2 vert_texel_coord;

void main() {
    vertex_t vertex = cube_vertices[vertex_index];
    vec3 position = vertex_position + vertex.position;

    gl_Position = view_projection * vec4(position, 1.0);
    vert_texel_coord = vertex.texel_coord;
}