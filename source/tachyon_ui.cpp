

namespace tyon
{
    ui_context* g_ui = nullptr;
    PROC ui_init() -> fresult
    {
        PROFILE_SCOPE_FUNCTION();
        if (g_ui != nullptr)
        {   TYON_ERROR( "ui_context is already initialized" );
            return false;
        }
        g_ui = memory_allocate<ui_context>(1);
        // TODO: Load font as asset
        ui_font& font = g_ui->default_font;
        file* noto_sans_file = entity_allocate<file>();
        *noto_sans_file = file_load_binary( "data/fonts/noto_sans/NotoSans-Regular.ttf" );
        SDL_IOStream* noto_sans_io = SDL_IOFromMem(
            noto_sans_file->memory.data, noto_sans_file->memory.size );
        sdl::TTF_Font* noto_sans = sdl::TTF_OpenFontIO( noto_sans_io, true, 16 );
        sdl::TTF_SetFontHinting( noto_sans, sdl::TTF_HINTING_LIGHT_SUBPIXEL );

        font.platform_font = noto_sans;
        font.size_points = 16.0;

        entity_type_register<ui_drawable_widget>();
        ui_drawable_widget* test_status_bar = entity_allocate<ui_drawable_widget>();
        test_status_bar->drawable.geometry = {
            .name = "test_status_bar",
            .vertexes = geometry_rectangle( vec2 {1920.0, 24.0} )
        };
        mesh_init( &test_status_bar->drawable.geometry );
        TYON_LOG( "UI Initialized" );

        return true;
    }

    PROC ui_tick() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        ui_tick_start();

        entity_tick_all<ui_drawable_widget>();
        // Test code
        // if (ui_point_box_collision())

        ui_tick_end();
    }

    PROC ui_destroy() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        TYON_LOG( "UI Destroyed" );
    }

    PROC ui_tick_start() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        g_ui->frame.input = g_ui->input;
    }

    PROC ui_tick_end() -> void
    {
        PROFILE_SCOPE_FUNCTION();
    }

    PROC ui_point_box_collision( v2_f32 point, v2_f32 box_pos, v2_f32 box_size ) -> bool
    {
        // Clip means "inside of x extent"
        // ie "point is inside left extent"
        v2_f32 halfsize = box_size / 2.0f;
        bool left_clip  = (point.x > box_pos.x - halfsize.x);
        bool right_clip = (point.x < box_pos.x + halfsize.x);
        bool up_clip    = (point.y < box_pos.y + halfsize.y);
        bool down_clip  = (point.y > box_pos.y + halfsize.y);
        return (left_clip && right_clip && up_clip && down_clip);
    }

}
