

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
        test_status_bar->type = e_ui_drawable::mesh;
        test_status_bar->widget = test_status_widget->id;
        test_status_widget->transform.translation.z = 528 - 12;
        g_ui->tmp_bar = test_status_widget->id;

        entity_init( test_status_bar );
        entity_init( test_status_widget );

        g_ui->test_image.image.data = memory_allocate<rgba>( 400 * 400 );
        g_ui->test_image.image.size = { 400, 400 };
        g_ui->test_image.id = uuid_generate();
        // TODO: Doesn't preserve aspect ratio
        g_ui->test_image.draw_region.size = v2_f32{ 200, 200 };
        memset( g_ui->test_image.image.data, 0xFF, g_ui->test_image.image.size_bytes() );
        g_ui->test_image.write_timestamp = time_now_ns();

        // SECTION: Create some text test
        fstring quick_brown_fox = "The quick brown fox jumped over the lazy dog.";
        fstring quick_brown_fox_lower = "the quick brown fox jumped over the lazy dog";
        fstring quick_brown_fox_upper = "THE QUICK BROWN FOX JUMPED OVER THE LAZY DOG";
        ui_drawable* test_text = entity_allocate<ui_drawable>();
        auto text_widget = entity_allocate<ui_widget>();
        test_text->text.text = quick_brown_fox;
        test_text->widget = text_widget->id;
        entity_init<ui_drawable>( test_text );
        sdl_render_text( test_text );
        test_text->image_.name = "quick_brown_fox";
        test_text->image_.draw_box.size = { f32(test_text->image_.image.size.x),
                                            f32(test_text->image_.image.size.y) };
        test_text->image_.draw_box.position = { 500.0, 500.0 };
        test_text->image_.id = uuid_generate();

        entity_init( test_text );
        entity_init( text_widget );
        // g_render->permanent_draw_queue_image.push_tail( &test_text->image_ );

        TYON_LOG( "UI Initialized" );

        return true;
    }

    PROC ui_tick() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        ui_tick_start();

        entity_tick_all<ui_drawable>();
        // Test code
        // g_render->draw_queue_image.push_tail( &g_ui->test_image );

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

    PROC ui_action_create( ui_temp_action* arg ) -> uid
    {
        // Synchronize first
        std::unique_lock _lock { g_ui->actions_lock, std::defer_lock };
        if (_lock.try_lock_for( 1ms ) == false) { TYON_ERROR( "Mutex timeout" ); return false; }

        arg->id = uuid_generate();
        g_ui->actions_bound.push_tail( *arg );
        return arg->id;
    }

    PROC ui_action_triggered( uid action ) -> fresult
    {
        // Synchronize first. We use polling mostly we don't care to wait if updates aren't ready.
        std::unique_lock _lock { g_ui->actions_lock, std::try_to_lock };
        if ( ! _lock) { return false; }

        bool result_triggered = false;
        g_ui->actions_bound.map_procedure( [action, &result_triggered](ui_temp_action& arg){
            if (action == arg.id && arg.triggered)
            {   result_triggered = true;

                // This function does not transition state if it's a toggle state
                if (arg.toggle_state == false)
                {   arg.triggered = false; }
                TYON_LOGF( "Action triggered, keycode {}, name {}",
                           SDL_GetScancodeName( arg.keyscan ), arg.name );
            }
        });
        return result_triggered;
    }

}
