
#pragma once

namespace tyon
{

enum class e_window_platform : i32
{
    none = 0,
    any = 1,
    x11 = 2,
    wayland = 3,
    windows = 4
};

/** physical shader types that a gpu may be able to support
 */
enum class shader_type
{
    vertex,
    fragment,
    geometry,
    compute,
    tesselation_control,
    tesselation_eval
    };

// enum class e_vsync_mode
// {
//     off,
//     adaptive,
//     double_buffered,
//     triple_buffered,
//     variable_refresh
// };

struct scene_camera
{
    ftransform transform;
    vec2 sensor_size;
    f32 near_clip;
    f32 far_clip;

    PROC create_perspective_projection() -> matrix;
    PROC create_orthographic_projection() -> matrix;
    // Create local normalized foward vector
    PROC forward() -> v3;
    // Create local normalized up vector
    PROC up() -> v3;
    // Create local normalized right vector
    PROC right() -> v3;
};

struct mesh
{
    uid id;
    fstring name;
    ftransform transform;
    array<v3> vertexes;
    array<v3> vertex_normals;
    array<i32> vertex_indexes;
    array<v4> vertex_colors;

    // These are used as statistics/convenience variables

    // Number of faces
    i32 faces_n = 0;
    // Number of vertecies
    i32 vertexes_n = 0;
    // Number of vertex indices
    i32 vertex_indexes_n = 0;
};

#pragma pack(push, 1)
struct frame_general_uniform
{
    // Timestamp of the very beginning of the program lifetime
    f32 epoch = 0;
    // Time elapsed since program epoch
    f32 time_since_epoch = 0;
    /// Time since epoch at the beginning of previous frame
    f32 last_begin_epoch = 0;
    /// Time since epoch at end of previous frame
    f32 last_end_epoch = 0;
    /// Time between last frame and current frame measured at unspecified time during frame
    f32 delta_time = 0;
    /// Time between last frame and current frame measured at beginning of each frame
    f32 delta_time_begin = 0;
    /// Time between last frame and current frame measured at beginning of each frame
    f32 delta_time_end = 0;
    // Screen aspect ratio given as vertical over horizontal
    f32 screen_vh_aspect_ratio = 1080.f/1920.f;
    // 32nd byte here. Already aligned, no padding required
    // Primary activate camera
    matrix camera;
};
#pragma pack(pop)

struct render_image
{
    uid id;
    fstring name;
    image<rgba> image;
    /** The subportion of the image that will be drawn as coordinates from the bottom left XY */
    box_2d clip_region;
    box_2d draw_box;
    /** Changed API, it is now draw location / size. */
    box_2d draw_region;
    /** Absolute display depth from -1 million to 1 million */
    i32 depth = 0;
    time_monotonic_ns write_timestamp = time_now_ns();
    i64 write_version = 0;
};

struct render_args
{
    window default_window;
};

// NOTE: Context structs should always be as near the bottom of the decleration order as possible.
struct render_context
{
    render_args args;
    // Primary window size
    v2 window_size = { 1920.0f, 1080.0f };
    e_window_platform window_platform = e_window_platform::none;
    bool renderdoc_attached = false;
    bool nsight_attached = false;

    /* Orthographic UI camera */
    scene_camera ui_camera;

    array< render_image > images;
    array< mesh > meshes;

    array< mesh* > draw_queue_mesh;
    array< render_image* > draw_queue_image;
    /** Doesn't get cleared every frame, retained convenience, not to be used often.
        Mainly for debugging */
    array< render_image* > permanent_draw_queue_image;

    bool display_ready = false;
};

extern render_context* g_render;
extern platform_subsystem* sdl;

/** Threaded entry point */
PROC render_thread( render_args* args ) -> void;

PROC render_init( render_args* args ) -> void;

PROC render_destroy() -> void;

/** Run before the main tick, does resetting functions and such */
PROC render_tick_start() -> void;

PROC render_tick() -> void;

PROC mesh_init( mesh* arg ) -> fresult;

PROC mesh_bounding_box_3d( mesh* arg ) -> box_3d;

/** Find the extents of the pre-transformed mesh in screen camera coordinates */
PROC mesh_bounding_box_2d( mesh* arg ) -> box_2d;

}
