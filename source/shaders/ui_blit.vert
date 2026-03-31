#version 430 core

struct vulkan_ui_blit_uniform
{
    /** This doesn't make sense for the current API because we're working in simple XY coordinates */
    mat4 transform;
    /** May be scaled */
    vec2 draw_scale;
    vec2 size;
    vec2 position;
    vec2 surface_size;
    vec4 tint;
};

layout(push_constant) uniform push_t
{
    int uniform_index;
} push;

layout(std140, binding = 0) buffer vulkan_ui_blit_push {
    vulkan_ui_blit_uniform data[];
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
    vec4 vert = vec4( vertexes[ vertex_id ], 1.0 );
    // Remap draw_size into into NDC coordinates and rescale to draw_size
    vert.x = (vert.x / global.surface_size.x * (global.size.x + global.position.x)) *
        global.draw_scale.x;
    vert.y = (vert.y / global.surface_size.y * (global.size.y + global.position.y)) *
        global.draw_scale.y;

    gl_Position = vert;
    uv_coord = uvs[ vertex_id ];
}
