#version 430 core

// NOTE: We changed the push constant layout from the old simple mesh shader days to support more
layout(push_constant) uniform mesh {
    mat4 local_space;
    vec2 surface_size;
    vec4 base_color;
    int debug_mode;
} push;


layout (location = 2) in vec4 v_color;

layout (location = 0) out vec4 frag_color;

void main()
{
    frag_color = v_color;
    // frag_color = vec4( 1.0, 1.0, 1.0, 0.4 );
}
