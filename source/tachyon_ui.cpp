

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

        /** Statically size array to prevent reallocation invalidation bugs */
        g_ui->widgets.entities.change_allocation( 1000 );
        g_ui->drawables.entities.change_allocation( 1000 );
        g_ui->action_layers.entities.change_allocation( 1000 );

        // SECTION: Initialize keybinds and actions
        ui_action_layer* text_input = entity_allocate( &g_ui->action_layers );
        text_input->name = "text_input";
        g_ui->layer_text_input = text_input->id;

        ui_temp_action command_console_open;
        command_console_open.name = "command_console_open";
        command_console_open.keyscan = SDL_SCANCODE_GRAVE;
        command_console_open.action_layer = g_ui->layer_text_input;
        g_ui->command_console_open = ui_action_create( &command_console_open );

        // Create a test widget
        ui_drawable* test_status_bar = entity_allocate<ui_drawable>( &g_ui->drawables );
        ui_widget* test_status_widget = entity_allocate<ui_widget>( &g_ui->widgets );
        test_status_bar->geometry = {
            .name = "test_status_bar",
            .vertexes = geometry_rectangle( vec2 {1920.0, 24.0} )
        };
        test_status_bar->inactive = true;
        test_status_bar->type = e_ui_drawable::mesh;
        test_status_bar->widget = test_status_widget->id;
        test_status_widget->transform.translation.z = 528 - 12;
        g_ui->tmp_bar = test_status_widget->id;

        ui_drawable_init( test_status_bar );
        // entity_init( test_status_widget );

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
        ui_drawable* test_text = entity_allocate<ui_drawable>( &g_ui->drawables);
        auto text_widget = entity_allocate<ui_widget>( &g_ui->widgets );
        test_text->text.text = quick_brown_fox;
        test_text->widget = text_widget->id;
        test_text->type = e_ui_drawable::text;
        test_text->image_.name = "quick_brown_fox";
        test_text->text.bounding_box = { 1920.0, 50.0 };
        test_text->image_.draw_box.position = { 500.0, 0.0 };

        ui_drawable_init( test_text );
        sdl_render_text( test_text );
        test_text->image_.id = uuid_generate();

        // Initialize graphical command console stuff
        ui_console_init( &g_ui->console );

        command command_2 = {
            .type = e_command::property,
            .name = "debug_test_text",
            .description = "",
            .aliases = { "help" },
            .on_trigger = [test_text](command* arg) {
                test_text->text.text = arg->property.value.get_string().value;
                TYON_LOG( "Set test test" );
            },
            .property { .value_type = e_primitive::string_ },
        };
        g_command->c_log_debug_break = command_add( &command_2 );

        ui_drawable_init( test_text );
        // ui_widget_init( text_widget );

        TYON_LOG( "UI Initialized" );

        return true;
    }

    PROC ui_tick() -> void
    {
        PROFILE_SCOPE_FUNCTION();
        ui_tick_start();
        sdl->tick();

        // TYON_LOG( g_ui->input.mouse_scroll.y );

        // Update console commands
        auto console_s = entity_search_id( &g_ui->drawables, g_ui->console.input_drawable );
        if (console_s.match_found)
        {   console_s.match->text.text = g_ui->console_input; }

        // entity_tick_all( &g_ui->widgets, ui_widget_tick );
        entity_tick_all( &g_ui->drawables, ui_drawable_tick );
        // Test code
        // g_render->draw_queue_image.push_tail( &g_ui->test_image );

        // if (ui_point_box_collision())
        f32 mouse_x = 0.0f;
        f32 mouse_y = 0.0f;
        SDL_MouseButtonFlags flags = SDL_GetMouseState( &mouse_x, &mouse_y );
        // auto* drawable = entity_search<ui_drawable>( g_ui->tmp_bar ).copy_default(nullptr);
        auto* widget = entity_search_id( &g_ui->widgets, g_ui->tmp_bar ).match;
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

        if (ui_action_triggered( g_ui->command_console_open ))
        {   ui_console_open( &g_ui->console, !g_ui->console_input_on ); }

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
        g_ui->input = {};
        g_ui->frame.input = g_ui->input;
    }

    PROC ui_tick_end() -> void
    {
        PROFILE_SCOPE_FUNCTION();
    }

    PROC ui_drawable_init( ui_drawable* arg ) -> fresult
    {
        if (arg->widget.valid() == false)
        {   TYON_ERROR( "Drawable has no associated widget, did you forget to set it?" );
            return false;
        }
        if (arg->type == e_ui_drawable::none)
        {   TYON_ERRORF( "Drawable '{}' given no type {}", arg->name, arg->id );
            return false;
        }
        mesh_init( &arg->geometry );
        // NOTE: Well, this render image needs a uid but it isn't attached to
        // any enttiy list I'm not sure if this is the right way to be doing
        // this interface but images have no associated resource so it's not
        // important right now?
        arg->image_.id = uuid_generate();

        return false;
    }

    PROC ui_drawable_destroy( ui_drawable* arg ) -> void
    {
    }

    PROC ui_drawable_tick( ui_drawable* arg ) -> void
    {
        if (arg->inactive)
        {   return; }

        // SECTION: Regenerate appropriate variables
        ui_widget* widget = entity_search_id( &g_ui->widgets, arg->widget ).match;
        if (widget == nullptr)
        {   TYON_ERROR( "Failed to find base widget associated with drawable widget" );
            return;
        }

        // If we inherit inactive then this drawable is inactive too so we skip the tick entirely.
        bool inactive_inherited = widget->inactive;
        if (inactive_inherited || arg->inactive) { return; }
        // TODO: Calculate depth based on widget hierarchy match widget, treat it as widget-local
        arg->image_.depth = arg->depth;

        // TODO: Cache this result for high poly geometry
        // NOTE: Cache what???
        switch (arg->type)
        {
            case e_ui_drawable::mesh:
            {
                widget->bounding_box = mesh_bounding_box_2d( &arg->geometry );

                // TODO: Construct transform from widget hierarchy
                arg->geometry.transform = widget->transform;

                // TODO: 2D draw plane depth, not sure if this should be seperated for true 3D objects
                // TODO: Propagate depth for parent hierarchy
                arg->geometry.transform.translation.x = (-arg->depth / g_ui->depth_constant);

                // Queue the drawable for drawing
                g_render->draw_queue_mesh.push_tail( &arg->geometry );
            }
            case e_ui_drawable::text:
            {
                // Update bounding box if relevant
                // NOTE: Broken API currently
                // arg->text.bounding_box = widget->bounding_box.size;

                sdl_render_text( arg );
                // NOTE: We're kind of just borrowing usage of the image here, dual purpose
                g_render->draw_queue_image.push_tail( &arg->image_ );
            }
            default: break;
        }
    }

    PROC window_destroy( window* arg ) -> void
    {
        auto sdl = sdl_create_platform_subsystem();
        sdl.window_close( arg );
        *arg = {};
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
        auto& widget_list = g_ui->widgets.entities;
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

    PROC ui_action_command_trigger_callback( command* context, uid action_id ) -> void
    {
        auto action_s = entity_search_id_array( &g_ui->actions_bound, action_id );
        if (action_s.match_found)
        {
            ui_temp_action* x_action = action_s.match;
            // Toggle state if it's a toggle state
            if (x_action->toggle_state)
            {   x_action->triggered = !x_action->triggered;
            }
            else
            {   x_action->triggered = true; }
            TYON_LOGF( "Command triggered action {}", x_action->name );

        }
    }

    PROC ui_action_create( ui_temp_action* arg ) -> uid
    {
        // Synchronize first
        std::unique_lock _lock { g_ui->actions_lock, std::defer_lock };
        if (_lock.try_lock_for( 1ms ) == false) { TYON_ERROR( "Mutex timeout" ); return false; }

        arg->id = uuid_generate();
        g_ui->actions_bound.push_tail( *arg );

        // Make action invokable from command console
        command command_1 = {
            .type = e_command::execute,
            .name = arg->name,
            .description = "Not provided.",
            .aliases = {},
        };
        command_1.on_trigger = [id = arg->id](command* arg) {
            ui_action_command_trigger_callback( arg, id ); };
        command_add( &command_1 );

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
                // NOTE: Don't log toggle stage actions, on transitions, its juts noisy
                if (arg.toggle_state == false)
                {   arg.triggered = false;
                    TYON_LOGF( "Action triggered, keycode {}, name {}",
                               SDL_GetScancodeName( arg.keyscan ), arg.name );
                }

            }
        });
        return result_triggered;
    }

    PROC ui_console_init( ui_console* arg ) -> fresult
    {
        ui_widget* console_widget = entity_allocate<ui_widget>( &g_ui->widgets );
        ui_drawable* input = entity_allocate<ui_drawable>( &g_ui->drawables );
        arg->input_drawable = input->id;
        arg->root_widget = console_widget->id;

        console_widget->name = "console_root";
        // Start console invisible/inactive
        console_widget->inactive = true;
        // Bind drawable to widget
        input->widget = console_widget->id;

        input->text.text = "";
        input->type = e_ui_drawable::text;
        input->name = input->image_.name = "console_input";
        // Beeeeeeg box
        // NOTE: wait why is the surface size determined by bounding box, isn't this dumb?
        input->text.bounding_box = { 960.0, 720.0 };
        input->image_.draw_box.position = { 0.0, 0.0 };

        auto* background_drawable = ui_drawable_create_box(
            "console_background", arg->root_widget, {960.0f, 720.0f}, 0, 10 );
        background_drawable->widget = arg->root_widget;
        arg->background_drawable = background_drawable->id;
        ui_drawable_init( input );
        ui_drawable_init( background_drawable );
        // ui_wdiget_init( console_widget );

        return false;
    }

    PROC ui_console_open( ui_console* context, bool open_else_close ) -> void
    {
        auto console_result = entity_search_id( &g_ui->widgets, g_ui->console.root_widget );
        tyon::g_ui->console_input_on = open_else_close;
        g_ui->text_input_on = open_else_close;
        if (console_result.match_found)
        {   console_result.match->inactive = (! open_else_close);  }
        TYON_LOGF( "console_input_on: {}", g_ui->console_input_on );
    }

    PROC ui_drawable_create_box(
        fstring name, uid parent, v2 size, v2 position, i32 depth ) -> ui_drawable*
    {
        ui_drawable* result = nullptr;

        ui_drawable* box_drawable = entity_allocate<ui_drawable>( &g_ui->drawables );
        ui_widget* box_widget = entity_allocate<ui_widget>( &g_ui->widgets );

        box_drawable->name = name;
        box_widget->name = name;
        box_drawable->geometry = {
            .name = name,
            .vertexes = geometry_rectangle( vec2 {size.x, size.y} )
        };
        box_drawable->type = e_ui_drawable::mesh;
        box_drawable->widget = box_widget->id;
        box_widget->transform.translation.y = position.x;
        box_widget->transform.translation.z = position.y;
        box_drawable->depth = 10;

        result = box_drawable;
        return result;
    }
}
