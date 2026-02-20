
#pragma once

namespace tyon
{

#define VULKAN_LOG( ... ) TYON_BASE_LOG( "Vulkan", __VA_ARGS__ );
#define VULKAN_LOGF( FORMAT_, ... ) TYON_BASE_LOGF( "Vulkan", FORMAT_, __VA_ARGS__ );
#define VULKAN_ERROR( ... ) TYON_BASE_ERROR( "Vulkan Error", __VA_ARGS__ );
#define VULKAN_ERRORF( FORMAT_, ... ) TYON_BASE_ERRORF( "Vulkan Error", FORMAT_, __VA_ARGS__ );

enum class e_vulkan_shader_debug : i32
{
    none = 0,
    any = 1,
    vertex_weighted = 2,
    triangle_mosaic = 3
};

struct vulkan_mesh_shader_push
{
    matrix local_space = matrix::one();
    v4_f32 base_color = v4_f32 { 0.4, 0.4, 0.4, 1.0 };
    e_vulkan_shader_debug debug_mode = e_vulkan_shader_debug::triangle_mosaic;
};

struct vulkan_shader
{
    uid id;
    fstring name = "unnamed";
    fstring entry_point = "main";
    file code;
    bool code_binary = false;
    VkShaderModule platform_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits stage_flag {};
};

struct vulkan_swapchain
{
    uid id;
    fstring name = "unnamed";
    VkSwapchainKHR platform_swapchain;
    VkFramebuffer platform_framebuffer;
    array<VkFence> frame_end_fences;
    /** Vulkan dependant size of presentable surface, often close to window size.
    ultra pedantic about timing and exact size on most platforms. */
    VkExtent2D present_size;
    // Internal size actually being used,vs in the intended size
    VkExtent2D vk_present_size;
    i32 n_images = 0;
    bool initialized = false;
    resource_arena resources;
};

struct vulkan_pipeline
{
    uid id;
    fstring name = "unnamed";
    array<vulkan_shader> shaders;
    vulkan_swapchain* swapchain = nullptr;

    VkPipeline platform_pipeline {};
    VkPipelineLayout platform_layout {};
    VkDescriptorPool vk_resource_pool {};
    VkDescriptorSetLayout platform_descriptor_layout {};
    VkRenderPass platform_render_pass {};

};

struct vulkan_device_memory_entry
{
    VkBuffer buffer {};
    // Identifying position
    i64 position {};
    i64 size {};
    i64 alignment {};
    VkBufferUsageFlags usage_flag;
};

/* Devie memory managed by the platform layer */
struct vulkan_memory
{
    uid id;
    // Arguments
    // Name of the device memory region
    fstring name = "unnamed";
    i64 size {};
    VkMemoryPropertyFlags access_flags;
    // State
    VkDeviceMemory memory;
    VkSharingMode sharing_mode;

    linked_list<vulkan_device_memory_entry> used;
    linked_list<vulkan_device_memory_entry> free;
    i64 head_size {};
};

struct vulkan_buffer
{
    uid id;
    // Arguments
    fstring name;
    i64 size = 0;
    VkBufferUsageFlags type = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;

    // State
    VkBuffer buffer {};
    // Associated memory allocated from device
    vulkan_device_memory_entry memory;
};

struct vulkan_mesh
{
    uid id;
    vulkan_buffer vertex_buffer;
    vulkan_buffer vertex_indexes_buffer;
    vulkan_buffer color_buffer;
};

struct vulkan_image
{
    uid id;
    uid associated_image;
    VkImage platform_image;
};


struct vulkan_mesh_draw_args
{
    u32 n_vertexes = 0;
    u32 n_instances = 1;
    u32 first_vertex = 0;
    u32 first_instance = 0;
};

struct vulkan_frame
{
    // The index of the frame drawn since program start
    i64 draw_index = -1;
    // Provided by vkAcquireNextImageKHR
    i32 inflight_index = -1;
    frame_general_uniform uniform;
    vulkan_buffer general_uniform_buffer;
    /* VkDeviceMemory general_uniform_memory; */
    raw_pointer general_uniform_data;
    /* VkDescriptorPool descriptor_resource_pool; */
    VkDescriptorSet vk_resource;
    array<mesh*> draw_queue_mesh;
    array< render_image*> draw_queue_image;
};

struct vulkan_context
{
    // Primary Vulkan instance of to interface with
    VkInstance instance;
    // Primary device to do work on
    VkPhysicalDevice device;
    // Logical device which is reserved logical resoruces from physical device
    VkDevice logical_device;
    // Primary Window surface to draw to
    VkSurfaceKHR surface;
    VkCommandPool command_pool;
    array<VkCommandBuffer> commands;
    /** Views describe how to interpret VkImage's, VkImages are related to
        textures and framebuffers */
    array<VkImageView> swapchain_image_views;
    array<VkFramebuffer> swapchain_framebuffers;
    array<VkImage> swapchain_images;
    i32 graphics_queue_family = -1;
    i32 present_queue_family = -1;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkDescriptorPool common_resource_pool;

    /* VkPipelineLayout pipeline_layout; */

    // Primary render pass
    VkRenderPass render_pass;
    // Primary graphics pipeline, associated with render pass
    VkPipeline pipeline;
    vulkan_pipeline mesh_pipeline;
    vulkan_pipeline ui_mesh_pipeline;

    // Ungrouped threading primitives
    VkFence frame_begin_fence;
    VkFence frame_acquire_fence;
    /* VkFence frame_end_fence; */

    VkSemaphore queue_submit_semaphore;
    VkSemaphore frame_end_semaphore;

    VkAllocationCallbacks allocator_callback {};
    // A pointer to the callback, may be null to turn it off
    VkAllocationCallbacks* vk_allocator = nullptr;
    i32 frames_inflight_count = 3;
    vulkan_swapchain swapchain;

    array<mesh*> tmp_meshes;
    array<vulkan_mesh> meshes;
    array<vulkan_image> images;
    array<vulkan_frame> frames_inflight;
    vulkan_memory device_memory;
    i32 mesh_debug_mode_cycle = 0;
    e_vulkan_shader_debug mesh_debug_mode = e_vulkan_shader_debug::none;

    mesh* draw_mesh;

    // Test Data
    VkDeviceMemory vertex_memory;
    VkBuffer test_triangle_buffer {};
    mesh test_triangle;
    // Different version for window sized viewports
    mesh test_ui_triangle;
    mesh test_ui_square;
    mesh test_teapot;
    mesh test_whale;
    // OpenGL coordinates
    array<f32> test_triangle_data = {
        0.f, 0.f, 1.f, // TODO: Remove temporary colour
        -0.5f, -0.4330127019f, 0.0f,
        0.f, 1.f, 0.f,
        0.5f, -0.4330127019f, 0.0f,
        1.f, 0.f, 0.f,
        0.0f,  0.4330127019f, 0.0f
    };

    array<f32> test_square_data = {
        0.f, 0.f, 1.f, // TODO: Remove temporary colour
        -0.5f, -0.5, 0.0f,
        0.f, 1.f, 0.f,
        0.5f, -0.5, 0.0f,
        1.f, 0.f, 0.f,
        0.5f, 0.5, 0.0f,

        0.f, 0.f, 1.f, // TODO: Remove temporary colour
        -0.5f, -0.5, 0.0f,
        1.f, 0.f, 0.f,
        0.5f, 0.5, 0.0f,
        0.f, 1.f, 0.f,
        -0.5f, 0.5, 0.0f,
    };


    // Configurables
    VkFormat swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
    /** This should be enough to fit 1 very large object, like 1 million verticies
        1 200 000 × 4 × 3 × 4 = 54.931 MiB

        The rationale being the GPU might be pedantic about where things can be
        stored so memory is chunked this way to account for that. TODO: Needs
        testing how reasonable this is. */
    i64 device_memory_chunk_size = 256_MiB;


    // Platform Independant State
    memory_heap_allocator default_allocator;
    i_allocator* allocator = nullptr;
    /* Top level resource manager for vulkan, needs to be ordered after allocator for
     because Vulkan resoruces are allocated out of the allocator*/
    resource_arena resources;

    bool initialized = false;
    i64 frames_started = 0;
    i64 frames_completed = 0;
};

PROC vulkan_allocator_create_callbacks( i_allocator* allocator );

PROC vulkan_label_object( u64 handle, VkObjectType type, fstring name ) -> void;

PROC vulkan_swapchain_init( vulkan_swapchain* arg, VkSwapchainKHR reuse_swapchain )
    -> fresult;

PROC vulkan_buffer_create(
    fstring name,
    i64 size,
    VkBufferUsageFlags type,
    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE
)   -> vulkan_buffer;

PROC vulkan_memory_allocate( vulkan_memory* arg ) -> fresult;

PROC vulkan_mesh_init( mesh* arg) -> fresult;

PROC vulkan_image_init( render_image* arg ) -> fresult;

/** Create a VkMemory object */
PROC vulkan_memory_init( vulkan_memory* arg ) -> fresult;

PROC vulkan_memory_suballocate_buffer( vulkan_memory* arg, vulkan_buffer* buffer ) -> fresult;

PROC vulkan_memory_suballocate_image( vulkan_memory* arg, vulkan_image* image ) -> fresult;

PROC vulkan_init_pipelines() -> void;

PROC vulkan_init() -> fresult;

PROC vulkan_destroy() -> void;

PROC vulkan_tick() -> void;

PROC vulkan_draw() -> void;

extern vulkan_context* g_vulkan;

}
