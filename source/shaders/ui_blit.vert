#version 430 core

struct vulkan_ui_blit_uniform
{
    mat4 transform;
    vec2 size;
    vec4 tint;
};

layout(push_constant) uniform push_t
{
    int uniform_index;
} push;

layout(std140, binding = 0) uniform vulkan_ui_blit_push {
    // NOTE: Sigh. Hardcoed size
    vulkan_ui_blit_uniform data[2000];
} globals;

layout(location = 4) out vec2 uv_coord;

vec3 vertexes[3] = {
    { -1.0, -1.0, 0.0 },
    { -1.0, 3.0, 0.0 },
    { 3.0, -1.0, 0.0 },
};

vec2 uvs[3] = {
    vec2( 0.0, 0.0 ),
    vec2( 0.0, 2.0 ),
    vec2( 2.0, 0.0 ),
};

void main()
{
    vulkan_ui_blit_uniform global = globals.data[ push.uniform_index ];
    int vertex_id = gl_VertexIndex;
    gl_Position = vec4( vertexes[ vertex_id ], 1.0 );
    uv_coord = uvs[ vertex_id ];
}
