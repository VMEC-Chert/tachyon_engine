
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
    FORWARD struct ui_drawable;

    struct sdl_window
    {
        uid id;
        fstring name;
        SDL_Window* handle = nullptr;
    };

    struct sdl_context
    {
        window* main_window = nullptr;
        /** This is a text rendering subsystem that does font atlas
         * caching. This is required for performant text rendering. */
        sdl::TTF_TextEngine* text_engine;
        array<sdl_window> windows;
        sdl::TTF_Font* default_font = nullptr;
        f32 default_font_size = 16;
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

    PROC sdl_render_text( ui_drawable* arg ) -> fresult;

    extern sdl_context* g_sdl;

}

#endif // VMEC_SDL_ON
