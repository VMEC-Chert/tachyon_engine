#version 430 core

// SECTION: Shader Inputs

struct vulkan_ui_blit_uniform
{
    /** This doesn't make sense for the current API because we're working in simple XY coordinates */
    mat4 transform;
    /** May be scaled */
    vec2 draw_size;
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

layout(binding = 1) uniform sampler2D tex;

layout(location = 4) in vec2 uv_coord;


// SECTION: Shader Outputs
layout (location = 0) out vec4 out_color;

const bool debug_visualize_clip = false;
const vec4 debug_transparent_magenta = vec4( 0.4, 0.2, 0.4, 0.2 );


void main()
{
    vec2 uv = uv_coord;
    out_color = texture( tex, uv_coord );

    if (debug_visualize_clip)
    {
        bool out_of_clip = (uv.x > 1.0 || uv.x < 0.0 || uv.y > 1.0 || uv.y < 0.0);
        if (out_of_clip)
        out_color = debug_transparent_magenta;
    }

}
