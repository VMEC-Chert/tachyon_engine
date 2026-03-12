#version 430 core

// SECTION: Shader Inputs
layout(std140, binding = 0) uniform stub_uniform { float a;};
layout(binding = 1) uniform sampler2D texSampler;


// SECTION: Shader Outputs
layout (location = 0) out vec4 out_color;d

void main()
{
    vec4 old_color = subpassLoad( input_color );
    out_color = old_color;
    // textSampler
    // out_color = old_color;
}
