
namespace tyon
{
    sdl_context* g_sdl = nullptr;

    // Platform Hooks
    PROC sdl_init() -> fresult
    {   PROFILE_SCOPE_FUNCTION();
        TYON_LOG( "Initialization Start for Platform SDL" );
        g_sdl = memory_allocate<sdl_context>( 1 );
        entity_type_register<sdl_window>();

        // NOTE: SDL must not ever move threads
        SDL_SetLogPriorities( SDL_LOG_PRIORITY_TRACE );

        if (g_render->window_platform == e_window_platform::x11)
        {   SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "x11" );
        }
        if (g_render->window_platform == e_window_platform::wayland)
        {   SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "wayland" );
        }
        if (g_render->window_platform == e_window_platform::windows)
        {   SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "windows" );
        }

        if (REFLECTION_PLATFORM_LINUX)
        {
            if (g_render->renderdoc_attached || g_render->nsight_attached)
            {   SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "x11" );
                g_render->window_platform = e_window_platform::x11;
            }
        }

        // TODO: Init more stuff here as you use more things
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD );

        i32 major = 4;
        i32 minor = 4;
        SDL_GL_GetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, &major );
        SDL_GL_GetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, &minor );

        // Enumerate video drivers
        array<fstring> video_drivers;
        TYON_LOG( "Enumerating SDL Video Drivers: " );
        for (i32 i=0; i<100; ++i)
        {
            cstring x_driver = SDL_GetVideoDriver( i );
            if (x_driver == nullptr) { break; }
            TYON_LOGF( "    {}", video_drivers.push_tail( x_driver ) );
        }

        fstring video_driver = SDL_GetCurrentVideoDriver();
        if (video_driver == "x11")
        {   g_render->window_platform = e_window_platform::x11;
        }
        else if (video_driver == "wayland")
        {   g_render->window_platform = e_window_platform::wayland;
        }
        else if (video_driver == "windows")
        {   g_render->window_platform = e_window_platform::windows;
        }

        TYON_LOGF( "Current SDL selected Video Driver/Window platform: {}", video_driver );
        TYON_LOG( "Initialization Complete for Platform SDL" );
        return true;
    }

    PROC sdl_tick() -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        sdl_event_process();
        return true;
    }

    PROC sdl_destroy() -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        SDL_DestroyWindow( g_sdl->windows[0].handle );
        // TTF_CloseFont( default_font );
        // default_font = nullptr;
        // TTF_Quit();
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD );
        SDL_Quit();

        g_sdl->~sdl_context();
        return true;
    }

    PROC sdl_window_open( window* arg ) -> fresult
    {   PROFILE_SCOPE_FUNCTION();
        TYON_LOG( "Opening Window using SDL platform" );

        sdl_window& platform_window = *entity_allocate<sdl_window>();
        arg->id = uuid_generate();
        platform_window.id = arg->id;

        // TODO: Temporarily hardcoded to Vulkan window type
        platform_window.handle = SDL_CreateWindow(
            arg->title.c_str(),
            arg->size.x,
            arg->size.y,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE );

        if ( platform_window.handle == nullptr)
        {   TYON_ERROR( "Failed to open SDL window" );
            return false;
        }
        // TODO: Hardcoded main window
        g_sdl->main_window = arg;
        (void)&g_sdl->windows.push_tail( platform_window );

        // NOTE: Do this before showing the window so you don't see the ugly default platform icon
        // NOTE: It still shows the default icon, I don't know how to fix without an even uglier hack
        // stbi uses RGBA order reguardless of endianness so use the non-endian SDL_PIXELFORMAT_RGBA32
        // TODO: Fix this to use native format later, not important because only we use the engine
        SDL_Surface* icon = SDL_CreateSurfaceFrom(
            arg->icon.size.x, arg->icon.size.y, SDL_PIXELFORMAT_RGBA32,
            arg->icon.data, arg->icon.stride_bytes() );
        SDL_SetWindowIcon( platform_window.handle, icon );

        TYON_LOG( "Showing SDL Window" );
        SDL_ShowWindow( platform_window.handle );
        if (arg->maximized)
        {   SDL_MaximizeWindow( platform_window.handle );
            TYON_LOG( "Maximizing SDL window" );
        }

        i32 window_width = 0;
        i32 window_height = 0;
        SDL_GetWindowSize( platform_window.handle, &window_width, &window_height );
        TYON_LOGF( "Created window size, width: {} : height {}" , window_height, window_height );

        return true;
    }

    PROC sdl_window_close( window* arg ) -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        sdl_window* platform_window = entity_search<sdl_window>( arg->id ).copy_default({});
        SDL_DestroyWindow( platform_window->handle );
        TYON_LOGF( "Closed platform window '{}'", arg->name );
        return false;
    }

    // Internal
    PROC sdl_event_process() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        SDL_PumpEvents();

        SDL_Event x_event;
        // Actually move step, not move speed
        f32 move_speed = 50.0f;
        while (SDL_PollEvent( &x_event ))
        {
            switch (x_event.type)
            {
                case SDL_EVENT_MOUSE_WHEEL:
                    // g_frame->scroll_y += x_event.wheel.y;
                    break;
                case SDL_EVENT_QUIT:
                    global->kill_program = true;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    // NOTE: UI Camera sensor must always match the viewport,
                    // you can clip it however you like after
                    g_render->ui_camera.sensor_size.x = f32(x_event.window.data1);
                    g_render->ui_camera.sensor_size.y = f32(x_event.window.data2);
                    TYON_LOGF( "SDL Window resize event, new size: {}",
                               g_render->ui_camera.sensor_size );
                    break;
                case SDL_EVENT_WINDOW_MOVED:
                    break;
                case SDL_EVENT_KEY_DOWN:
                {
                    // TODO: tmp testing, remove me
                    if (x_event.key.repeat == false)
                    {
                        TYON_LOG( "Move Event", SDL_GetScancodeName( x_event.key.scancode ) );
                        TYON_LOGF( "[{} {} {}]",
                                   g_vulkan->test_ui_triangle.transform.translation.x,
                                   g_vulkan->test_ui_triangle.transform.translation.y,
                                   g_vulkan->test_ui_triangle.transform.translation.z );
                        v3 up = g_render->ui_camera.up();
                        v3 right = g_render->ui_camera.right();
                        if (x_event.key.scancode == SDL_SCANCODE_W)
                        {   g_vulkan->test_ui_triangle.transform.translation +=
                            (up * move_speed);
                        }
                        else if (x_event.key.scancode == SDL_SCANCODE_A)
                        {   g_vulkan->test_ui_triangle.transform.translation +=
                            (-right * move_speed);
                        }
                        else if (x_event.key.scancode == SDL_SCANCODE_D)
                        {   g_vulkan->test_ui_triangle.transform.translation +=
                            (right * move_speed);
                        }
                        else if (x_event.key.scancode == SDL_SCANCODE_S)
                        {   g_vulkan->test_ui_triangle.transform.translation +=
                            (-up * move_speed);
                        }

                        static i32 i_mesh = 0;
                        if (x_event.key.scancode == SDL_SCANCODE_SPACE)
                        {   i_mesh = (i_mesh + 1) % g_vulkan->tmp_meshes.size();
                            g_vulkan->draw_mesh = g_vulkan->tmp_meshes[ i_mesh ];
                            TYON_LOGF( "selected mesh {}", i_mesh );
                        }
                        if (x_event.key.scancode == SDL_SCANCODE_F2)
                        {
                            // Cycle through all debug modes
                            array<e_vulkan_shader_debug> modes = {
                                e_vulkan_shader_debug::none,
                                e_vulkan_shader_debug::vertex_weighted,
                                e_vulkan_shader_debug::triangle_mosaic
                            };
                            i32 selected_mode = g_vulkan->mesh_debug_mode_cycle++ % modes.size();
                            g_vulkan->mesh_debug_mode = modes[ selected_mode ];
                            TYON_LOG( "Changed mesh debug mode to {}", selected_mode );
                        }
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                {
                    SDL_MouseMotionEvent e = x_event.motion;
                    g_ui->input.mouse_update_time = e.timestamp;
                    g_ui->input.mouse_window = v2_f32{ e.x, e.y };
                    g_ui->input.mouse_delta = v2_f32{ e.xrel, e.yrel };
                    /* timestamp, windowID, SDL_MouseID which (unique mouse id),
                          SDL_MouseButtonFlags state, x, y, xrel, yrel */
                    // TYON_LOG( "Mouse motion", e.x, e.y, e.xrel, e.yrel );
                    break;
                }
                default:
                {
                    // break;
                }
            }
        }
    }

    PROC sdl_vulkan_surface_create(
        window* arg,
        VkInstance vk_instance,
        const struct VkAllocationCallbacks* vk_allocator,
        VkSurfaceKHR* surface
    ) -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        if (arg == nullptr)
        {   TYON_ERROR( "Window is nullptr" );
            return false;
        }
        auto search = g_sdl->windows.linear_search( [=]( sdl_window& x ) {
            return x.id == arg->id; } );
        if (search.match_found == false)
        {   return false;
        }

        auto platform_window = search.match;
        bool create_ok = SDL_Vulkan_CreateSurface(
            platform_window->handle, vk_instance, vk_allocator, surface );
        if (create_ok == false)
        {   TYON_ERROR( "Failed to create Vulkan surface using SDL Platform" );
            return false;
        }
        TYON_LOG( "Created Vulkan surface using SDL Platform" );
        return true;
    }

    PROC sdl_create_platform_subsystem() -> platform_subsystem
    {
        PROFILE_SCOPE_FUNCTION();
        platform_subsystem result = {
            .name = "tyon::sdl",
            .id = uuid_generate(),
            .active = true,

            .subsystem_dependencies = { "tyon::library", "tyon::render" },
            .init = sdl_init,
            .tick = sdl_tick,
            .destroy = sdl_destroy,
            .window_open = sdl_window_open,
            .window_close = sdl_window_close,
            .vulkan_surface_create = sdl_vulkan_surface_create
        };
        return result;
    }
}
