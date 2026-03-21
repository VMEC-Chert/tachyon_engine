
#include "include_core.h"

namespace tyon
{

render_context* g_render = nullptr;

platform_subsystem* sdl = nullptr;

PROC render_thread( render_args* args ) -> void
{
    thread_options options = {
        .short_name = "render",
        .name = "name",
        .description = "GPU Orchestration, windowing and UI subsystems",
        .scheduler_priority = -10,
        .permanant_block_size = 400_MiB,
        .scratch_block_size = 400_MiB
    };

    thread_self_init( options );

    render_init( args );
    while (global->kill_program == false && thread_self_interupt() == false)
    {
        time_monotonic next_frame = time_now() + 16666us;
        render_tick();
        std::this_thread::sleep_until( next_frame );
    }
    render_destroy();
}

PROC render_init( render_args* args ) -> void
{
    PROFILE_SCOPE_FUNCTION();

    g_render = memory_allocate<render_context>( 1 );
    sdl = memory_allocate<platform_subsystem>(1);
    *sdl = sdl_create_platform_subsystem();

    g_render->args = *args;

    /* If nsight or renderdoc is attached it will break with wayland, so we
       should try to disable pre-emptively that if possible. */
    if (REFLECTION_PLATFORM_LINUX)
    {   char* renderdoc_env = std::getenv( "ENABLE_VULKAN_RENDERDOC_CAPTURE" );
        cstring x11_env_cstring = std::getenv( "DISPLAY" );
        cstring wayland_env_cstring = std::getenv( "WAYLAND_DISPLAY" );
        fstring x11_env = (x11_env_cstring ? x11_env_cstring : "");
        fstring wayland_env = (wayland_env_cstring ? wayland_env_cstring : "");
        if (renderdoc_env && renderdoc_env[0] == '1')
        {   TYON_LOG( "Renderdoc is attached and doesn't support Wayland on Linux, falling back to X11" );
            g_render->renderdoc_attached = true;
        }
    }

    cmdline_argument window_arg = g_library->cmdline_arguments.linear_search(
        []( cmdline_argument& arg ) {
        return arg.name == "window_platform"; }).copy_default( {} );
    fstring window_platform = window_arg.value.get_string().copy_default("");
    if (window_platform.size())
    {   TYON_LOGF( "Selected window_platform arg '{}'", window_platform );
        if (window_platform == "x11")
        {   g_render->window_platform = e_window_platform::x11;
        }
        else if (window_platform == "wayland")
        {   g_render->window_platform = e_window_platform::wayland;
        }
        else if (window_platform == "windows")
        {   g_render->window_platform = e_window_platform::windows;
        }
        else
        {   TYON_ERROR( "window_platform specified on command line does not match known platform" );
        }
    }

    // TODO: Temporary location
    auto debugger_search = g_library->cmdline_arguments.linear_search(
        []( cmdline_argument& arg ) {
        return arg.name == "debugger_mode"; });
    if (debugger_search.match_found)
    {   global->debugger_mode = true; }

    // SDL needs to setup after the render context but before vulkan init
    sdl->init();

    sdl->window_open( &g_render->args.default_window );

    /* This "camera" is for UI usage... So the sensor size should be the window
     * size, and all the primitives exactly on the near clip or slightly
     * behind. */
    g_render->ui_camera = scene_camera {
        .transform {
            /* Move it back a bit to make -1 to 1 fit inside the view frustum.
               2 seems to be the right number for this. */
            .translation = { -2.0, 0.0, 0.0 },
            .rotation = { 0.0, 0.0, 0.0 * 6.28 }
        },
        // TODO: Good size for UI, needs to be updated on the fly though...
        // This has been done for Vulkan, but no other backend
        .sensor_size = g_render->args.default_window.size,
        .near_clip = 1.0f,
        // Fairly generous far clip for random 3D UI effects if we want that
        .far_clip = 2000.0f
    };

    bool vulkan_ok = false;
    bool opengl_ok = false;

    switch (global->render_backend)
    {
        case e_render_backend::vulkan:
            x11_init();
            x11_window_open();
            vulkan_ok = vulkan_init();
            break;
        case e_render_backend::opengl:
            // opengl_ok = opengl_init(); break;
            TYON_BREAK();
            break;
        default: break;

    }
    // Should be good to start the UI now
    ui_init();
}

PROC render_destroy() -> void
{
    vulkan_destroy();
}

PROC render_tick() -> void
{
    PROFILE_SCOPE_FUNCTION();
    // SECTION: Reset data for new frame
    g_thread->scratch->blank_all();
    g_render->draw_queue_mesh.reset();
    g_render->draw_queue_image.reset();
    // Add the permanant draws back onto the draw list
    g_render->draw_queue_image = g_render->permanent_draw_queue_image;

    sdl->tick();
    ui_tick();
    switch (global->render_backend)
    {
        case e_render_backend::vulkan:
            // TODO: temporary disabled whilst working on allocator
            vulkan_tick();
            break;
        case e_render_backend::opengl:
            TYON_BREAK();
            break;
        default: break;
    }
}

PROC mesh_init( mesh* arg ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    if (arg->id.valid())
    {   TYON_ERRORF( "Tried to initialize already initialize mesh '{}'", arg->name );
        return false;
    }
    if (arg->vertexes.size() <= 0)
    {   TYON_ERRORF( "Tried to initialize mesh '{}' with {} vertexes",
                       arg->name, arg->vertexes.size() * 3 );
        return false;
    }

    arg->id = uuid_generate();
    arg->faces_n = limit<i32>( arg->vertexes.size() / 3 );
    arg->vertexes_n = limit<i32>( arg->vertexes.size() );
    arg->vertex_indexes_n = limit<i32>( arg->vertex_indexes.size() );

    TYON_LOG( "Created mesh: ")
    TYON_LOGF( "    ID: {}", arg->id );
    TYON_LOGF( "    Name: {}", arg->name );
    TYON_LOGF( "    Faces: {}", arg->faces_n );
    TYON_LOGF( "    Vertexes: {}", arg->vertexes_n );
    TYON_LOGF( "    Vertex Indexes: {}", arg->vertex_indexes_n );
    return true;
}

PROC mesh_bounding_box_3d( mesh* arg ) -> box_3d
{
    return {};
}

PROC mesh_bounding_box_2d( mesh* arg ) -> box_2d
{
    box_2d result;
    f32 up = 0.0;
    f32 down = 0.0;
    f32 left = 0.0;
    f32 right = 0.0;
    // TODO: hardcoded for screen camera coordinates
    arg->vertexes.map_procedure( [&] (v3 a1) {
        // Iterate over and find the furthest extents of the mesh
        right = (a1.y > right ? a1.y : right);
        left  = (a1.y < left  ? a1.y : left);
        up    = (a1.z < up    ? a1.z : up);
        down  = (a1.z > down  ? a1.z : down);
    } );
    f32 left_right_distance = (up - down);
    f32 up_down_distance = (left - down);
    // NOTE: Whatever you pick make sure you subtract from left argument of the distance calculation
    result.size.x = absolute(left_right_distance);
    result.size.y = absolute(up_down_distance);
    result.position.x = (left - left_right_distance);
    result.position.y = (up - up_down_distance);
    return result;
}

matrix scene_camera::create_perspective_projection()
{
    f32 camera_width = std::round( sensor_size.x );
    f32 aspect_ratio_vh = sensor_size.y / sensor_size.x;
    // Clip Plane Dimensions
    // Near
    f32 n = this->near_clip;
    // Far
    f32 f = this->far_clip;
    // Right
    f32 r = -(camera_width / 2.0f) / aspect_ratio_vh;
    // Left
    f32 l = -r;
    // Top
    f32 t = (camera_width / 2.0f);
    // Bottom
    f32 b = -t;

    // TODO: Temporary fix cuz too lazy to transpose algorithm.
    // TODO:. Well... This is for NDC coordinates...
    matrix result = matrix{
        (2.f*n) / (r-l),       0.f,               0.0f,         0,
              0.f,        (2.f*n) / (t-b),        0.0f,         0,
        (r+l) / (r-l),    (t+b) / (t-b),    -(f+n) / (f-n),    -1,
              0.f,             0.f,      -(2.f*f*n) / (f-n),    0
    }.transpose();

    return result;
}

PROC scene_camera::create_orthographic_projection() -> matrix
{
    // Clip Plane Dimensions
    // Near
    f32 n = this->near_clip;
    // Far
    f32 f = this->far_clip;
    // Right - NOTE: aspect divide doesn't seem to be nessesary
    f32 r = (sensor_size.x / 2.0f);
    // Left
    f32 l = -r;
    // Top
    f32 t = (sensor_size.y / 2.0f);
    // Bottom
    f32 b = -t;

    /* This is rearranged for z up, x forward, y right, Unreal/Tachyon coordinates

       NOTE: This used to be X right to make it a bit like screen coordinates,
       but I think that's a bit stupid and using forward-up-right declerations
       is much more user friendly. Additionally it means coordinate versions are
       done in one time constants, not constantly throughout the code.

    NOTE: This is also a Vulkan specific function. It generates a Vulkan
    compatible projection but the coordinate rearrangement to NDC is performed
    later. This does format depth in 0-1.0 however.*/
    matrix result = matrix{
        1/(f-n),    0.0f,       0.0f,       -n/(f-n),
        0.0f,       2/(r-l),    0.0f,       -(r+l) / (r-l),
        0.0f,       0.0f,       2/(t-b),    -(t+b)/(t-b),
        0.0f,       0.0f,       0.0f,       1.0f
    };

    return result;
}

// Create local normalized foward vector
PROC scene_camera::forward() -> v3
{   return matrix_create_rotation( transform.rotation ) * v3::forward();
}
// Create local normalized up vector
PROC scene_camera::up() -> v3
{   return matrix_create_rotation( transform.rotation ) * v3::up();
}
// Create local normalized right vector
PROC scene_camera::right() -> v3
{   return matrix_create_rotation( transform.rotation ) * v3::right();
}

}
