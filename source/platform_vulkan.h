
#pragma once

namespace tyon
{

#define VULKAN_LOG( ... ) TYON_BASE_LOG( "Vulkan", __VA_ARGS__ );
#define VULKAN_LOGF( FORMAT_, ... ) TYON_BASE_LOGF( "Vulkan", FORMAT_, __VA_ARGS__ );
#define VULKAN_ERROR( ... ) TYON_BASE_ERROR( "Vulkan Error", __VA_ARGS__ );
#define VULKAN_ERRORF( FORMAT_, ... ) TYON_BASE_ERRORF( "Vulkan Error", FORMAT_, __VA_ARGS__ );

FORWARD struct vulkan_memory;

enum class e_vulkan_shader_debug : i32
{
    none = 0,
    any = 1,
    vertex_weighted = 2,
    triangle_mosaic = 3
};

enum class e_vulkan_memory_object : i32
{
    none,
    any,
    buffer,
    image
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
    VkDescriptorSetLayout platform_descriptor_layout {};
    VkRenderPass platform_render_pass {};
    VkSampler base_sampler {};

};

struct vulkan_heap
{
    /** Arbitrary index provided by vulkan */
    i32 index;
    VkMemoryHeapFlags heap_flags;
};

struct vulkan_memory_entry
{
    i64 block {};
    /* Where in the list the entry belongs to
       NOTE: This is a performance optimization index */
    i64 index {};
    // Identifying position in the block this belongs to
    i64 position {};
    /** Position before alignment */
    i64 reserved_position = false;
    /** Bytes allocated to an actual object, 0 when free */
    i64 size {};
    /** Size including extra implimentation bytes like alignment and redzones */
    i64 reserved_size {};
    i64 alignment = 1;
    /** What type of object we are storing in this memory entry

     No object tpye means a free entry with no associated device. */
    e_vulkan_memory_object type;
};

using vulkan_memory_node = linked_list<vulkan_memory_entry>::t_node;

struct vulkan_memory_block_args
{
    i64 size = 0;
    i32 memory_type_index = -1;
    VkMemoryPropertyFlags memory_flags = 0;
};

struct vulkan_memory_block
{
    i64 index = 0;
    VkDeviceMemory memory;
    i64 size = 0;
    i32 memory_type_index = 0;
    i32 heap_index = 0;
    /** What type  of memory access we  need.

        NOTE: This is actually EXTREMELY important because each "device heap" in
        exposed by Vulkan has different access flags, and that by extension
        limits how much memory you can allocate, and also whether you can map
        the memory directly, or need to use staging buffers. The staging memory
        is generally quite small */
    VkMemoryPropertyFlags memory_flags = 0;
    bool host_mappable = false;

    linked_list< vulkan_memory_entry > entries;
    array< i64  > free_entries;
    i64 largest_entry = 0;
};

/** Metadata about a Vulkan object type like it's memory type, alignment, preferred heap, etc*/
struct vulkan_object_memory_info
{
    e_vulkan_memory_object type;
    std::bitset<32> memory_type_bits;
    i64 alignment = 0;
};

struct vulkan_memory_info
{
    bool pcie_bar_memory_limitation = true;
    i64 pcie_bar_heap_size = true;
};

/* Devie memory allocator */
struct vulkan_memory
{
    uid id;
    // Arguments
    // Name of the device memory region
    fstring name = "unnamed";
    i64 size {};
    /** How many bytes to add after each entry as a early corruption check/debugging tool */
    i64 redzone_bytes = 64;
    i64 device_block_size = 256_MiB;
    /** The fast staging heap is often really small ~256 MiB so the block size must be smaller.
     NOTE: Really should be a bit smaller than the staging block size for robustness */
    i64 staging_block_size = 64_MiB;
    /** Host accessible memory, behaves similar to unified memory. We usually have a lot of this. */
    i64 unified_block_size = 256_MiB;

    array< vulkan_object_memory_info > object_infos;
    array< vulkan_memory_block > blocks;
};

struct vulkan_transfer
{
    /** Which memory entry/buffer out of the transfer context list we're bound to */
    i64 buffer_index = -1;
    /** The offset to where in the transfer buffer we are */
    i64 position = 0;
    /** Size in bytes of the amount we want to transfer */
    i64 size = 0;
    /** The position in the buffer to start at using an offset from the start if
     * we are doing a partial transfer, this fine to leave 0 */
    i64 buffer_offset = 0;
    /** What object the transfer will be copied into */
    e_vulkan_memory_object destination = e_vulkan_memory_object::none;
    VkBuffer destination_buffer = VK_NULL_HANDLE;
    uid destination_image_;

};

struct vulkan_transfer_buffer
{
    uid buffer;
    /** How large is the backing buffer (convenience variable) */
    i64 size = 0;
    /** How much of the buffer has been used for pending transfers so far */
    i64 head_size = 0;
    raw_pointer mapped_data;
    bool mapped = false;
};

struct vulkan_transfer_context
{
    time_duration new_buffer_fail_reset_time = 10s;
    /** How much memory to allocate for each buffer as a chunk, should be less than 256 MiB usually */
    i64 staging_buffer_size = 63_MiB;
    i64 redzone_bytes = 64;

    /** Flag specifies the last attempt to create a new buffer failed so we can
     * avoid spamming failures */
    bool new_buffer_fail_flag = 0;
    time_periodic new_buffer_fail_reset_timer {new_buffer_fail_reset_time};
    array< vulkan_transfer > transfer_queue;
    array< vulkan_transfer_buffer > buffers;
};

struct vulkan_allocate_args
{
    i64 size = 0;
    // NOTE: Vulkan uses 64-bit alignment so we'll use that too
    i64 alignment = 0;
    i32 memory_type_index = -1;
    VkMemoryPropertyFlags memory_flags;
    /** Optional arg */
    i32 force_heap_index = -1;
    /** Optional, used for CPU transfers, prefers to be on the fast PCIe BAR memory */
    bool transfer_buffer = false;
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
    vulkan_memory_entry memory;
    /** Optional */
    VkMemoryPropertyFlags memory_flags;
    /** Optional, used for CPU transfers, prefers to be on the fast PCIe BAR memory */
    bool transfer_buffer = false;
};

struct vulkan_mesh
{
    uid id;
    ftransform transform;
    // Number of faces
    i32 faces_n = 0;
    // Number of vertecies
    i32 vertexes_n = 0;
    // Number of vertex indices
    i32 vertex_indexes_n = 0;
    vulkan_buffer vertex_buffer;
    vulkan_buffer vertex_indexes_buffer;
    vulkan_buffer color_buffer;
};

struct vulkan_image
{
    uid id;
    uid associated_image;
    VkImage platform_image;
    v2_f32 size;
    time_monotonic_ns update_timestamp = 0;
    vulkan_memory_entry memory;
    vulkan_buffer staging_buffer;
    /** Current tracked layout */
    VkImageLayout platform_layout;
    VkImageView platform_view;
};


struct vulkan_mesh_draw_args
{
    u32 n_vertexes = 0;
    u32 n_instances = 1;
    u32 first_vertex = 0;
    u32 first_instance = 0;
};

enum class e_vulkan_draw : i32
{
    none = 0,
    any = 1,
    mesh = 2,
    image = 3
};

/** This structure insists that no referenced data changes after it's created */
struct vulkan_draw_command
{
    /** What kind of draw command is requested */
    e_vulkan_draw type = e_vulkan_draw::none;
    vulkan_pipeline* pipeline;
    // TODO: Testing to see if it makes sense to copy the handle
    VkDescriptorSet platform_set;
    i32 resource_index {};
    /** Vulkan DescriptorSet index allocated out of vulkan_resources */
    i32 set_index {};
    vulkan_mesh* draw_mesh {};
    vulkan_image* draw_image {};
};

/** Manages many descriptor sets related to a pipeline*/
struct vulkan_resources
{
    uid pipeline;
    array<VkDescriptorSet> sets;
    array<VkDescriptorSetLayout> set_layouts;
    i32 sets_used {};
    i32 set_count {};
};

/** Manages per-frame descriptor sets

 This has to be it's own struct because we may have multiple types of descriptor
 that each need a configurable pool of descriptors to pool out of. */
struct vulkan_frame_resources
{
    array<vulkan_resources> resources;
};

struct vulkan_frame
{
    // The index of the frame drawn since program start
    i64 draw_index = -1;
    /** Provided by vkAcquireNextImageKHR */
    i32 inflight_index = -1;
    frame_general_uniform uniform;
    vulkan_buffer general_uniform_buffer;
    VkCommandBuffer command {};
    VkFence end_fence {};
    // NOTE: Need staging buffer for most objects, revisit this later if it's faster
    // raw_pointer general_uniform_data;
    /** All resources */
    vulkan_frame_resources resources;
    array<mesh*> draw_queue_mesh;
    array< render_image*> draw_queue_image;
    array< vulkan_draw_command > draw_queue_command;
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
    vulkan_pipeline ui_blit_pipeline;

    // Ungrouped threading primitives
    VkFence frame_begin_fence;
    VkFence frame_acquire_fence;

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
    // TODO: Need to fill these with actual buffers
    array<vulkan_buffer> buffers;
    array<vulkan_frame> frames_inflight;
    vulkan_memory device_memory;
    i32 mesh_debug_mode_cycle = 0;
    e_vulkan_shader_debug mesh_debug_mode = e_vulkan_shader_debug::none;

    vulkan_transfer_context transfer;

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
    VkFormat swapchain_image_format = VK_FORMAT_B8G8R8A8_SRGB;
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
    bool restart_vulkan = false;
    bool device_lost = false;
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

/** Helper function to calculate vertex buffer size based on the current function

    NOTE: PLEASE use this function instead of calculating it manually to prevent
    horrible memroy issues. */
PROC vulkan_vertex_buffer_size( i64 vertexes ) -> i64;

PROC vulkan_mesh_init( mesh* arg) -> fresult;

PROC vulkan_image_init( render_image* arg ) -> fresult;

/** Initiaize a device memory manager */
PROC vulkan_memory_init( vulkan_memory* arg ) -> fresult;

PROC vulkan_memory_get_block( vulkan_memory context, i64 block_index ) -> vulkan_memory_block&;

/** Returns the index of the best memory type to use for this type. Monad
 * returns with an error if it's bad */
PROC vulkan_memory_find_best_type_index(
    std::bitset<32> valid_type_bits, VkMemoryPropertyFlags preferred_flags ) -> monad<i32>;

PROC vulkan_memory_allocate_block( vulkan_memory* context, vulkan_memory_block_args* arg ) -> fresult;

/** This is supposed to be used as a convenience function internally for a
    function like vulkan_memory_allocate_buffer(). It performs suballocation
    from device memory as an internal memory manager but without a backing
    type. */
PROC vulkan_memory_allocate_untyped( vulkan_memory* context, vulkan_allocate_args args )
-> monad<vulkan_memory_entry>;

PROC vulkan_memory_allocate_buffer( vulkan_memory* arg, vulkan_buffer* buffer ) -> fresult;

PROC vulkan_memory_allocate_image( vulkan_memory* arg, vulkan_image* image ) -> fresult;

PROC vulkan_init_pipelines() -> void;

/** Does nothing at present. You SHOULD call it anyway */
PROC vulkan_transfer_init( vulkan_transfer_context* arg ) -> fresult;

PROC vulkan_transfer_find_suitible_buffer( vulkan_transfer_context* context, i64 size )
-> search_result<vulkan_transfer_buffer>;

PROC vulkan_transfer_queue_buffer(
    vulkan_transfer_context* context,
    vulkan_buffer* buffer,
    i64 size,
    i64 buffer_offset
) -> monad< dynamic_span<void> >;

/** Do setup to prepare image so it is present and ready to draw on the GPU

    Images like most Vulkan need to be prepared and transfered in a special way
    before it can be drawn for render, this function serves to do anything
    related to that, transfering data, generating internal state, etc. */
PROC vulkan_image_prepare( render_image* arg ) -> fresult;

PROC vulkan_init() -> fresult;

PROC vulkan_destroy() -> void;

PROC vulkan_tick() -> void;

PROC vulkan_start_frame() -> void;

PROC vulkan_command_setup( vulkan_frame* frame ) -> void;

PROC vulkan_command_draw( vulkan_frame* frame ) -> void;

/** Allocate a new resource (descriptor set) just for this frame */
PROC vulkan_draw_command_acquire_resource(
    vulkan_draw_command* arg, vulkan_frame* frame, vulkan_pipeline* pipeline ) -> fresult;

extern vulkan_context* g_vulkan;

}

PROC format_as( VkResult arg ) -> tyon::fstring;
