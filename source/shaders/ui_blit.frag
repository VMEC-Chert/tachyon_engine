#version 430 core

// SECTION: Shader Inputs

struct vulkan_ui_blit_uniform
{
    mat4 transform;
    /** May be scaled */
    vec2 draw_size;
    vec2 _pad_0;
    vec4 tint;
    vec2 surface_size;
};

layout(push_constant) uniform push_t
{
    int uniform_index;
} push;

layout(std140, binding = 0) uniform vulkan_ui_blit_push {
    // NOTE: Sigh. Hardcoed size
    vulkan_ui_blit_uniform data[4000];
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
