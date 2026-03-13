#version 430 core

// SECTION: Shader Inputs
layout(std140, binding = 0) uniform stub_uniform { float a;};
layout(binding = 1) uniform sampler2D tex;

layout(location = 4) in vec2 uv_coord;


// SECTION: Shader Outputs
layout (location = 0) out vec4 out_color;

void main()
{
    vec2 uv = uv_coord;
    out_color = texture( tex, uv_coord );

    bool debug_visualize_clip = false;
    bool out_of_clip = (uv.x > 1.0 || uv.x < 0.0 || uv.y > 1.0 || uv.y < 0.0);
    vec4 debug_transparent_magenta = vec4( 0.4, 0.2, 0.4, 0.2 );
    if (debug_visualize_clip && out_of_clip)
    {
        out_color = debug_transparent_magenta;
    }

}
