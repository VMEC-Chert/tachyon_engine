#version 430 core

layout(std140, binding = 0) uniform stub_uniform { float a;};

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
    int vertex_id = gl_VertexIndex;
    gl_Position = vec4( vertexes[ vertex_id ], 1.0 );
    uv_coord = uvs[ vertex_id ];
}
