#version 430 core

// SECTION: Shader Inputs
layout(std140, binding = 0) uniform stub_uniform { float a;};
layout(binding = 1) uniform sampler2D tex;

layout(location = 4) in vec2 uv_coord;


// SECTION: Shader Outputs
layout (location = 0) out vec4 out_color;

void main()
{

    out_color = texture( tex, uv_coord );
    // textSampler
    // out_color = old_color;
}
