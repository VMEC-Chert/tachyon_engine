

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

        // Register all entity types
        entity_type_register<ui_widget>();
        entity_type_register<ui_drawable>();

        // Create a test widget
        ui_drawable* test_status_bar = entity_allocate<ui_drawable>();
        ui_widget* test_status_widget = entity_allocate<ui_widget>();
        test_status_bar->geometry = {
            .name = "test_status_bar",
            .vertexes = geometry_rectangle( vec2 {1920.0, 24.0} )
        };
        test_status_bar->widget = test_status_widget->id;
        test_status_widget->transform.translation.z = 528 - 12;
        g_ui->tmp_bar = test_status_widget->id;

        entity_init( test_status_bar );
        entity_init( test_status_widget );
        TYON_LOG( "UI Initialized" );

        return true;
    }

    PROC ui_tick() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        ui_tick_start();

        entity_tick_all<ui_drawable>();
        // Test code
        // if (ui_point_box_collision())
        f32 mouse_x = 0.0f;
        f32 mouse_y = 0.0f;
        SDL_MouseButtonFlags flags = SDL_GetMouseState( &mouse_x, &mouse_y );
        // auto* drawable = entity_search<ui_drawable>( g_ui->tmp_bar ).copy_default(nullptr);
        auto* widget = entity_search<ui_widget>( g_ui->tmp_bar ).copy_default(nullptr);
        // box_2d widget_bounds = mesh_bounding_box_2d( &drawable->geometry );
        box_2d widget_bounds = widget->bounding_box;
        bool collide = ui_point_box_collision( g_ui->input.mouse_window,
                                               widget_bounds.position, widget_bounds.size );
        // Debug Tracing
        // TYON_LOG( widget_bounds.position.x, widget_bounds.position.y,
                  // widget_bounds.size.x, widget_bounds.size. y);
        if (collide)
        {   TYON_LOG( "mouse hover" );
        }

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

    PROC ui_widget_construct_tree() -> widget_tree
    {
        widget_tree tree;
        auto& widget_list = entity_get_context<ui_widget>()->list;
        i_allocator* allocator = g_thread->scratch;

        // +1 for the root node
        tree.size = widget_list.size() + 1;
        tree.data = allocator->allocate<widget_tree::node>( widget_list.size() );

        widget_tree::node root;
        root.value = nullptr;
        root.children.change_allocation( allocator, 2 ); // 2 is an arbtrary number
        tree.data[ tree.size - 1 ] = root;

        widget_tree::node* x_node = nullptr;
        ui_widget* x_widget = nullptr;
        // SECTION: Start populating the tree
        for (int i=0; i < widget_list.size(); ++i)
        {
            // TODO: Finish me
        }

        return tree;
    }

    PROC ui_compute_screen_transform() -> transform_3d
    {
        return {};
    }

}
