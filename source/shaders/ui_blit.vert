#version 430 core

layout(std140, binding = 0) uniform stub_uniform { float a;};

vec3 vertexes[3] = {
    { -1.0, -1.0, 0.0 },
    { -1.0, 3.0, 0.0 },
    { 3.0, -1.0, 0.0 },
};

vec3 uvs[3] = {
    vec3( 0.0, 0.0, 0.0 ),
    vec3( 0.0, 2.0, 0.0 ),
  { 2.0, 0.0, 0.0 },
};

void main()
{
     gl_Position = vec4( vertexes[ gl_VertexIndex ], 1.0 );
}
