#version 430 core

// NOTE: We changed the push constant layout from the old simple mesh shader days to support more
layout(std140, push_constant) uniform mesh {
    // TODO: Needs to be simplified to mainly just a uniform index
    mat4 local_space;
    vec4 base_color;
    int debug_mode;
    int uniform_index;
} push;

layout( location = 0 ) in vec3 normal;
layout( location = 1 ) in vec3 vert;
// layout( location = 2 ) in vec4 col;

// layout( location = 0 ) out vec3 v_normal;
layout( location = 1 ) out vec3 v_position;
layout( location = 2 ) smooth out vec4 v_color;
layout( location = 3 ) out mat4 world_matrix;


layout(std140, binding = 0) uniform frame_data
{
    float epoch;
    float time_since_epoch;
    /// Time since epoch at the beginning of previous frame
    float last_begin_epoch;
    /// Time since epoch at end of previous frame
    float last_end_epoch;
    /// Time between last frame and current frame measured at unspecified time during frame
    float delta_time;
    /// Time between last frame and current frame measured at beginning of each frame
    float delta_time_begin;
    /// Time between last frame and current frame measured at beginning of each frame
    float delta_time_end;
    // Screen aspect ratio given as vertical over horizontal
    float screen_vh_aspect_ratio;
    // Primary activate camera
    mat4 camera;
};

const int none = 0;
const int any = 1;
const int vertex_weighted = 2;
const int triangle_mosaic = 3;

const vec3 arbitrary_axis = vec3( 0.662f, 0.2f, 0.722f );

const float tau = 6.283185307;

// Create a Euler Rotation Matrix with w component being around an arbitrary axis
mat4 create_rotation( vec4 euler )
{
    // Rotation amounts
    vec4 r = euler;
    vec3 a = arbitrary_axis;
    float ar = euler.w;
    a = normalize(a.xyz);

    // Rotation around an arbitrary axis defined by Euler coordinates
    mat4 arbitraty_matrix =
        mat4( cos(ar)+(a.x*a.x) * (1-cos(ar)),
              a.y*a.x *(1 - cos(ar)) + a.z*sin(ar),
              a.z*a.x* (1 - cos(ar)) - a.y*sin(ar),
              0,

              a.x*a.y*(1-cos(ar)) - a.z*sin(ar),
              cos(ar) + (a.y*a.y)*(1 - cos(ar)),
              a.z*a.y* (1 - cos(ar)) + a.z*sin(ar),
              0,

              a.x*a.z*(1-cos(ar)) + a.y*sin(ar),
              a.y*a.z*(1 - cos(ar)) - a.x*sin(ar),
              cos(ar) + (a.x*a.x) * (1 - cos(ar)),
              0,

              0, 0, 0, 1 );


    // Math representation rotation matrix, needs to be rearranged to collumn major
    // [[cos(y) * cos(z), -cos(y) * sin(z), sin(y), 0],

    //  [cos(x) * sin(z) + cos(z) * sin(x) * sin(y),
    //  cos(x) * cos(z) - sin(x) * sin(y) * sin(z),
    //   -cos(y) * sin(x),
    //   0],

    //  [sin(x) * sin(z) - cos(x) * cos(z) * sin(y),
    //  cos(x) * sin(y) * sin(z) + cos(z) * sin(x),
    //   cos(x) * cos(y),
    //   0],

    //  [0, 0, 0, 1]]

    // // Euler Combined 3 Axis Rotation Matrix
    mat4 rotation_matrix =
            // Row 1
        mat4( cos(r.y)*cos(r.z),
              -cos(r.y)*sin(r.z),
              sin(r.y),
              0,
              // Row 2
              cos(r.x)*sin(r.z) + cos(r.z)*sin(r.x)*sin(r.y),
              cos(r.x)*cos(r.z) - sin(r.x)*sin(r.y)*sin(r.z),
              -cos(r.y)*sin(r.x),
              0,
              // Row 3
              sin(r.x)*sin(r.z) - cos(r.x)*cos(r.z)*sin(r.y),
              cos(r.x)*sin(r.y)*sin(r.z) + cos(r.z)*sin(r.x),
              cos(r.x)*cos(r.y),
              0,
              // Row 4
              0, 0, 0, 1);
    return rotation_matrix;
}

vec4 perspective_divide( vec4 a )
{
    return vec4( a.x/a.w, a.y/a.w, a.z/a.w, 1.0f );
}

void main()
{
    mat4 identity = mat4( 1., .0, .0, 0,
                          .0, 1., .0, 0,
                          .0, .0, 1., 0,
                          .0, .0, .0, 1. );
    // Transformation Matrices
    mat4 local = push.local_space;               // Local Space
    mat4 world = identity;                       // Local to World Space
    mat4 cam = camera;                           // World to Camera Space
    mat4 projection = identity;                  // Orthographic Camera to Clip-Space
    mat4 viewport = identity;                    // Clip-Space to Screen Space
    // cam = mat4(1);                            // Debug

    vec4 vertex = vec4( vert.x, vert.y, vert.z, 1.0 );

    mat4 transform = identity;

    // Normals don't particularly need or want scale and translation so we're skipping some
    // Add back camera rotation when its made available
    vec4 norm = projection * local * vec4( normal, 1.0 );

    // local = mat4(
    //              1, 0, 0, 0,
    //              0, 1, 0, 0,
    //              0, 0, 1, 0,
    //              0, 0, 0, 1
    //              );

    vertex = projection * cam * world * local * vertex;
    gl_Position = vertex;
    // gl_Position = perspective_divide( vertex );

    // Vertex Color
    bool debug_color_by_vertex = false;
    bool debug_color_by_triangle = true;
    v_color = push.base_color;

    if (push.debug_mode == vertex_weighted)
    {
        /*  Orange
        Mod 3 returns integers in the range 0, 1, 2
        divide by 2 remaps it into 0, 0.5, 1.0
        which is more useful for colours. */
        float col = mod( gl_VertexIndex,3.0 ) / 2;
        // Even-odd
        int triangle_vertex = (gl_VertexIndex % 3);
        float col1 = (triangle_vertex == 0 ? 0.8 : 0.0);
        float col2 = (triangle_vertex == 1 ? 0.6 : 0.0);
        float col3 = (triangle_vertex == 2 ? 0.8 : 0.0);

        v_color = vec4( 1.0-col, col, col2, 1.0 );
        v_color = vec4( col1, col2, col3, 1.0 );

        if (triangle_vertex == 0)
        {   v_color = vec4( 0.4, 0.0, 0.0, 1.0 );
        }
        if (triangle_vertex == 1)
        {   v_color = vec4( 0.0, 0.0, 0.0, 1.0 );
        }
        if (triangle_vertex == 2)
        {   v_color = vec4( 0.8, 0.8, 0.8, 1.0 );
        }
    }
    else if (push.debug_mode == triangle_mosaic)
    {
        float vertex_index = floor( gl_VertexIndex/3.0 );
        // offset by small float (0.5) to make prevent adjacent triangles being *exactly* identical
        float t = (mod(vertex_index, 5.0) / 5.0) * 1;
        v_color = vec4( t, t, t, 1.0 );
    }
    world_matrix = projection * cam;

    // Debug Coloring
    // v_color = vec4( 1.0 );
}
