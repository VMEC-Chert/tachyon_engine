
#pragma once

#define VMEC_SDL_ON 1
#if VMEC_SDL_ON

#if (REFLECTION_COMPILER_MINGW)
    #define SDL_MAIN_HANDLED 1
#endif // REFLECTION_COMPILER_MINGW

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>


namespace sdl
{
    #include <SDL3_ttf/SDL_ttf.h>
}

namespace tyon
{
    struct sdl_window
    {
        uid id;
        fstring name;
        SDL_Window* handle = nullptr;
    };

    struct sdl_context
    {
        window* main_window = nullptr;
        array<sdl_window> windows;
    };

    // Platform Hooks
    PROC sdl_init() -> fresult;
    PROC sdl_tick() -> fresult;
    PROC sdl_destroy() -> fresult;
    PROC sdl_window_open( window* arg ) -> fresult;
    PROC sdl_window_close( window* arg ) -> fresult;

    // Internal
    PROC sdl_event_process() -> void;

    PROC sdl_create_platform_subsystem() -> platform_subsystem;

    extern sdl_context* g_sdl;

    template<>
    struct entity_type_definition<sdl_window>
    {
        using t_entity = sdl_window;
        using t_context = entity_type_context<t_entity>;
        static constexpr cstring name = "tyon::sdl_window";
        static constexpr u128 id = uuid("691c9fd6-0f7c-495f-8b7f-af6e4ee7ef48");

        PROC allocate() -> void
        {}
        PROC init( t_entity* arg ) -> fresult
        {
            return false;
        }
        PROC destroy( t_entity* arg ) -> void
        {
            // Don't bother trying to close the window here, the associated tyon::window will do it
            *arg = {};
        }

        PROC tick( t_entity* arg ) -> void
        {}

        static PROC context_tick( void* context ) -> void
        {}

        static PROC destroy_all( void* context ) -> void
        {}

    };

}

#endif // VMEC_SDL_ON
