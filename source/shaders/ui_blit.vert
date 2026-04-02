#version 430 core

struct vulkan_ui_blit_uniform
{
    /** This doesn't make sense for the current API because we're working in simple XY coordinates */
    mat4 transform;
    /** May be scaled */
    vec2 draw_size;
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

layout(location = 4) out smooth vec2 uv_coord;

// Full-screen double size triangle     //   1+------+-----/ 3
vec3 triangle[3] = {                    //    |      |  /--
    { -1.0, -1.0, 0.0 },                //    +------/--
    { -1.0, 3.0, 0.0 },                 //    |    /-
    { 3.0, -1.0, 0.0 },                 //    | /--
};                                      //   2+

// Fullscreen exact size quad
vec3 quad[6] = {           //      1,6              4
    { -1.0, -1.0, 0.0 },   //       o+-------------+
    { -1.0, 1.0, 0.0 },    //        | \-          |
    { 1.0,  1.0, 0.0 },    //        |   \-        |
                           //        |     \--     |
    {  1.0, 1.0, 0.0 },    //        |        \-   |
    {  1.0,-1.0, 0.0 },    //        |          \- |
    { -1.0,-1.0, 0.0 },    //        +------------\+
};                         //       2              3,4

vec2 triangle_uvs[3] = {
    vec2( 0.0, 0.0 ),
    vec2( 0.0, 2.0 ),
    vec2( 2.0, 0.0 ),
};

// Already flipped Y in the image transfer, don't need to do it again
vec2 quad_uvs[6] = {
    vec2( 0.0, 0.0 ),
    vec2( 0.0, 1.0 ),
    vec2( 1.0, 1.0 ),

    vec2( 1.0, 1.0 ),
    vec2( 1.0, 0.0 ),
    vec2( 0.0, 0.0 ),
};

void main()
{
    vulkan_ui_blit_uniform global = globals.data[ push.uniform_index ];
    int vertex_id = gl_VertexIndex;
    vec4 vert = vec4( quad[ vertex_id ], 1.0 );
    vec2 uv = quad_uvs[ vertex_id ];

    // Remap draw_size into into -1 - 1.0 (2 length) NDC coordinates and rescale to draw_size
    // NOTE: I thought this was supposed to 1 / a but this seems to give the wrong result
    float x_ratio = (1.0 / global.surface_size.x);
    float y_ratio = (1.0 / global.surface_size.y);
    // Have to translate last to stop scaling pushing around the triangles
    // NOTE: Flip y so we have normal y up coordinates
    float x_scaled = vert.x * (x_ratio * global.draw_size.x);
    float y_scaled = vert.y * (y_ratio * global.draw_size.y);

    float x_translated = x_scaled + (x_ratio * global.position.x);
    float y_translated = y_scaled + (y_ratio * global.position.y);
    vert.x = x_translated;
    vert.y = y_translated;

    gl_Position = vert;
    uv_coord = uv;
}
