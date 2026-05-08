
namespace tyon
{
    sdl_context* g_sdl = nullptr;

    // Platform Hooks
    PROC sdl_init() -> fresult
    {   PROFILE_SCOPE_FUNCTION();
        TYON_LOG( "Initialization Start for Platform SDL" );
        g_sdl = memory_allocate<sdl_context>( 1 );

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
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS );
         // SDL_INIT_GAMEPAD

        // SECTION: TTF Font system setup
        // Setup default font
        sdl::TTF_Init();

        // TODO: Load font as asset
        ui_font& font = g_ui->default_font;
        file* noto_sans_file = entity_allocate( &g_library->files );
        *noto_sans_file = file_load_binary( "data/fonts/noto_sans/NotoSans-Regular.ttf" );
        SDL_IOStream* noto_sans_io = SDL_IOFromMem(
            noto_sans_file->memory.data, noto_sans_file->memory.size );
        sdl::TTF_Font* noto_sans = sdl::TTF_OpenFontIO( noto_sans_io, true, 16 );
        sdl::TTF_SetFontHinting( noto_sans, sdl::TTF_HINTING_LIGHT_SUBPIXEL );

        g_sdl->default_font = noto_sans;
        if (g_sdl->default_font)
        {   TYON_LOG( "Created default font" );
        }
        log_error_result( "Creating SDL TTF Default font", g_sdl->default_font == nullptr );

        g_sdl->text_engine = sdl::TTF_CreateSurfaceTextEngine();
        log_error_result( "Creating SDL TTF_TextEngine", g_sdl->text_engine == nullptr );

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
        sdl::TTF_CloseFont( g_sdl->default_font );
        sdl::TTF_Quit();
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD );
        SDL_Quit();

        g_sdl->~sdl_context();
        return true;
    }

    PROC sdl_window_open( window* arg ) -> fresult
    {   PROFILE_SCOPE_FUNCTION();
        TYON_LOG( "Opening Window using SDL platform" );

        sdl_window& platform_window = g_sdl->windows.push_tail({});
        arg->id = uuid_generate();
        platform_window.id = arg->id;

        // TODO: Temporarily hardcoded to Vulkan window type
        platform_window.handle = SDL_CreateWindow(
            arg->title.c_str(),
            i32(arg->size.x),
            i32(arg->size.y),
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
            limit<i32>(arg->icon.size.x), limit<i32>(arg->icon.size.y), SDL_PIXELFORMAT_RGBA32,
            arg->icon.data, limit<i32>( arg->icon.stride_bytes() ));
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

        // TODO: Temporary
        SDL_StartTextInput( platform_window.handle );


        return true;
    }

    PROC sdl_window_close( window* arg ) -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        sdl_window* platform_window = entity_search_id_array( &g_sdl->windows, arg->id ).match;
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
                    SDL_KeyboardEvent e = x_event.key;
                    /** NOTE: Normal inputs should be disabled when in text input mode
                        to prevent silly double triggers */
                    // NOTE: A failed lock is a missed events, we absolutely cannot miss events.
                    std::unique_lock _lock { g_ui->actions_lock };
                    g_ui->actions_bound.map_procedure( [key_event_ = x_event.key](ui_temp_action& arg){
                        bool scancode_match = (arg.keyscan == key_event_.scancode);
                        // TODO: Make generic for multiple hotkeys
                        auto layer_result = entity_search_id( &g_ui->action_layers, arg.action_layer );
                        // HACK: Hardcoded text layer
                        bool active_custom_layer = (layer_result.match_found &&
                                                    layer_result.match->inactive == false);
                        // Is layer is unset its presumed its in the default global layer set
                        bool active_global_layer = (arg.action_layer.valid() == false);
                        bool active_layer = (active_custom_layer || active_global_layer);
                        if (scancode_match && active_layer)
                        {   arg.triggered = true; }
                    });

                    // TODO HACK: Hadcoded text input backspace logic
                    if (g_ui->text_input_on)
                    {
                        bool backspace = (e.scancode == SDL_SCANCODE_BACKSPACE);
                        if (backspace)
                        {
                            // Backspace input handling
                            fstring& input = g_ui->console_input;
                            /** Find the start of the last codepoint and subtract
                                that to know how much string length should be left

                                NOTE: Doesn't handle strange characters like invisible
                                ones, needs to handle that */
                            i64 size = g_ui->console_input.size() ;
                            cstring last_char = (input.data() + size);
                            cstring back_one_utf8 = last_char;
                            u32 read_char = SDL_StepBackUTF8( input.data(), &last_char );
                            i64 final_size = back_one_utf8 - input.data();
                            input.resize( final_size );
                        }
                    }
                    break;
                }
                case SDL_EVENT_KEY_UP:
                {
                    // NOTE: A failed lock is a missed events, we absolutely cannot miss events.
                    std::unique_lock _lock { g_ui->actions_lock };
                    g_ui->actions_bound.map_procedure( [key_event_ = x_event.key](ui_temp_action& arg){
                        if (arg.keyscan == key_event_.scancode && arg.toggle_state)
                        {   arg.triggered = false;
                            TYON_LOGF( "Action released: '{}'", arg.name );
                        }
                    });
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                {
                    SDL_MouseButtonEvent e = x_event.button;
                    // Set any actions to triggered
                     // NOTE: A failed lock is a missed events, we absolutely cannot miss events.
                    std::unique_lock _lock { g_ui->actions_lock };
                    g_ui->actions_bound.map_procedure( [button_event = e](ui_temp_action& arg) {
                        if (arg.sdl_mouse_button == button_event.button)
                        {
                            arg.triggered = true;
                        }
                    });
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    SDL_MouseButtonEvent e = x_event.button;
                    // NOTE: A failed lock is a missed events, we absolutely cannot miss events.
                    std::unique_lock _lock { g_ui->actions_lock };
                    g_ui->actions_bound.map_procedure( [e](ui_temp_action& arg){
                        if (arg.sdl_mouse_button == e.button && arg.toggle_state)
                        {   arg.triggered = false;
                            TYON_LOGF( "Action released: '{}'", arg.name );
                        }
                    });
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                {
                    SDL_MouseWheelEvent e = x_event.wheel;
                    g_ui->input.mouse_scroll = v2_f32 { e.x, e.y };
                    TYON_LOGF( "SDL 2-Axis Mouse Scroll: {} {}", e.x, e.y );
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
                    TYON_LOGF( "Mouse motion: [{} {}] Relative Motion: [{} {}]",
                               e.x, e.y, e.xrel, e.yrel );
                    break;
                }
                case SDL_EVENT_TEXT_EDITING:
                {
                    // NOTE: Aparently this is for character construction like CJK languages?
                    break;
                }
                case SDL_EVENT_TEXT_INPUT:
                {
                    SDL_TextInputEvent e = x_event.text;
                    if (g_ui->text_input_on && g_ui->console_input_on)
                    {
                        g_ui->console_input += e.text;
                    }
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

    PROC sdl_render_text( ui_drawable* arg ) -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        text_drawable& props = arg->text;
        SDL_Color sdl_white = { 255, 255, 255, 255 };
        bool text_ok = false;
        bool text_changed = (props.text != props.previous_text);
        i32 width {};
        i32 height {};

        bool skip_rendering = (text_changed == false);
        if (skip_rendering)
        {   return false; }

        props.sdl_text = sdl::TTF_CreateText(
            g_sdl->text_engine, g_sdl->default_font, props.text.data(), props.text.size() );
        text_ok = bool(props.sdl_text);
        TYON_LOG( "Rendering text" );
        ERROR_GUARD( props.sdl_text, "Major issue if false" );

        if (props.wrapped && text_ok)
        {
            i32 wrap_pixels = 0;
            // NOTE: zero means wrap on newline
            bool wrap_ok = TTF_SetTextWrapWidth( props.sdl_text, wrap_pixels );
        }

        // Have to do this after changing settings like wrap length
        bool size_ok = sdl::TTF_GetTextSize( props.sdl_text, &width, &height );
        // Use BGRA format to save round trip convertion in Vulkan
        /** NOTE: We're going to be confining the text to within a predefined
            bounding box generally limited to the size of the widget, if it goes
            over the bounds, oh well, it gets clipped, but its generally a bug
            if this does happen. */
        props.surface = SDL_CreateSurface(
            i32(props.bounding_box.x), i32(props.bounding_box.y), SDL_PIXELFORMAT_BGRA8888 );

        if (props.surface && text_ok)
        {
            props.rendered_size = { f32(width), f32(height) };
            text_ok = sdl::TTF_DrawSurfaceText( props.sdl_text, 0, 0, props.surface );
            // Update text state
            props.previous_text = props.text;
        }

        if (props.surface)
        {
            ERROR_GUARD( props.surface->format == SDL_PIXELFORMAT_BGRA8888,
                         "Our internal API must have changed." );
            image<rgba> surface_view;
            surface_view.data = raw_pointer(props.surface->pixels);
            surface_view.size = { props.surface->w, props.surface->h };
            surface_view.stride_bytes_ = props.surface->pitch;
            surface_view.format = color_format::bgra8;

            push_allocator _al1 { g_thread->scratch };
            arg->image_.image = image_packed_from_simd( surface_view );
            arg->image_.write_timestamp = time_now_ns();
            // NOTE: We're going to move the image into the correct format
            // straight up to save on formatting performance
            arg->image_.image.format = e_color_format::bgra8;
            /* NOTE: We destroyed the surface here previously but it's
             * convenient to keep it around for now */
        }
        return false;
    }

}
