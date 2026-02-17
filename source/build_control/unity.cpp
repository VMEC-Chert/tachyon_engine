
#include "../include_core.h"
#include "../file.cpp"
#include "../global.cpp"
#include "../math.cpp"
#include "../platform_sdl.cpp"
#include "../platform_linux_x11.cpp"
#include "../platform_vulkan.cpp"
#include "../tachyon_ui.cpp"
#include "../tachyon_render.cpp"
#include "../time.cpp"

#if (TYON_LIB_MERGED_UNITY)
    #include "../../external/tachyon_lib/source/build_control/tachyon_lib_unity_core.cpp"
#endif // TYON_ENGINE_UNITY_BUILD

// #include "external/lua/onelua.c"

#if (TYON_ENGINE_MAIN_STANDALONE)
    // Main include
    #include "../main.cpp"
#endif TYON_ENGINE_UNITY_BUILD
