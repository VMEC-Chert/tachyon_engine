
#pragma once

#ifndef TYON_LIB_MERGED_UNITY
    #define TYON_LIB_MERGED_UNITY 1
#endif

#ifndef TYON_ENGINE_MAIN_STANDALONE
    #define TYON_ENGINE_MAIN_STANDALONE 1
#endif

#ifndef TYON_X11_ON 
    #define TYON_X11_ON REFLECTION_PLATFORM_LINUX
#endif // TYON_X11_ON

#ifndef TYON_WAYLAND_ON
    #define TYON_WAYLAND_ON REFLECTION_PLATFORM_LINUX
#endif // TYON_WAYLAND_ON

// Platform Dependencies
#if (TYON_X11_ON)
    #include "../external/xorg_proto/include/X11/X.h"
    #include "../external/xorg_xlib/include/X11/Xlib.h"
    #include "../external/xorg_xlib/include/X11/Xutil.h"
    #include "../external/xorg_proto/include/X11/Xatom.h"
    #include "../external/xorg_proto/include/X11/keysym.h"
    #include "../external/xorg_proto/include/X11/Xmd.h"

    #include "../external/xorg_proto/include/GL/glxproto.h"
    #include "../external/opengl/api/GL/glcorearb.h"
    #include "../external/mesa/include/GL/glx.h"
    #include "../external/opengl/api/GL/glext.h"
    #include "../external/opengl/api/GL/glxext.h"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"

// Dumb fix for C++ conflict
#define VK_VMEC_APPLY_MODULE_FIX 1

/* #define module vk_module */
// Enable Assistance with loading function pointers from hpp
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

/* NOTE: I really don't use the hpp or even intend to use it because I don't
   want to learn additional semantics or spend time integrating it into my memory
   systems. */
// #include <vulkan/vulkan.hpp>

#if (REFLECTION_PLATFORM_WINDOWS)
#include <vulkan/vulkan_win32.h>
#endif // REFLECTION_PLATFORM_WINDOWS

// Wayland an X11 are not mutually exclusive.
#if (TYON_WAYLAND_ON)
#include <vulkan/vulkan_wayland.h>
#endif // TYON_WAYLAND_ON

#if (TYON_X11_ON)
    #include <vulkan/vulkan_xlib.h>
#endif // TYON_X11_ON

#include <vulkan/vk_enum_string_helper.h>
/* #undef vk_module */
#pragma clang diagnostic pop

// -- stdlib / stl --
#include "include_stl.h"

/** --Vendored Dependencies-- */
#include "include_tracy.h"

/* NOTE: Sometimes you may want to manage certain dependencies seperately from
   this repository.  If this is the case you can set this to 1 and the
   dependencies will use standard search paths instead. */
#ifndef TYON_COMPILER_MANAGED_INCLUDES
    #define TYON_COMPILER_MANAGED_INCLUDES 0
#endif

#if (TYON_COMPILER_MANAGED_INCLUDES)
    #include <include_tachyon_lib_core.h>
#else
    #include "../external/tachyon_lib/source/include_tachyon_lib_core.h"
#endif // TYON_COMPILER_MANAGED_INCLUDES

#include "core.hpp"

#include "global.h"
#include "time.hpp"
#include "matrix.hpp"
#include "math.hpp"
#include "file.hpp"

// Old interface header
#include "renderer_interface.hpp"

// New headers
#include "platform_common.h"
#include "platform_sdl.h"
#include "platform_linux_x11.h"
#include "tachyon_render.h"
#include "platform_vulkan.h"
#include "tachyon_ui.h"

// Experimental module support
// import platform_vulkan;
