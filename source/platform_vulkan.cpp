
#include "include_core.h"

// TODO: Need to setup uniform so that we can chuck worldspce things into the test teapot

namespace dyn
{
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
}

// NOTE: This may be called across an API boundry so on Windows it needs dllexport decleration
// TODO: Need to figure out if this is ACTUALLY necessary
// NOTE: Very likely yes.
VKAPI_ATTR
auto  VKAPI_CALL vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data ) -> VkBool32
{
    PROFILE_SCOPE_FUNCTION();
    using namespace tyon;
    fstring type_name;
    switch (message_type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            type_name = "General"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            type_name = "Validation"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            type_name = "Performance"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
            type_name = "Device Address Binding"; break;
        default:
            type_name = "unknown_message_type";
    }

    fstring _category = fmt::format( "Vulkan {}", type_name );
    cstring category = _category.c_str();
    switch (message_severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            TYON_BASE_LOGF( category, "[Verbose] {}", callback_data->pMessage );
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            TYON_BASE_LOGF( category, "[Info] {}", callback_data->pMessage );
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            TYON_BASE_LOGF( category, "[Warning] {}", callback_data->pMessage );
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            TYON_BASE_ERRORF( category, "{}", callback_data->pMessage );
            if (global->debugger_mode)
            {   TYON_BREAK();
            }
            break;
        default:
            TYON_BASE_LOGF( "Vulkan unknown_debug_category", "{}", callback_data->pMessage );
    }
    return VK_FALSE;
}

namespace tyon
{
vulkan_context* g_vulkan = nullptr;
const bool vulkan_config_trace_allocations = false;

using vulkan_bool = u32;

PROC vulkan_allocator_create_callbacks( i_allocator* allocator )
{
    PROFILE_SCOPE_FUNCTION();
    VkAllocationCallbacks result = {};
    result.pUserData = allocator;

    result.pfnAllocation = [] (
        void* context,
        size_t bytes,
        size_t alignment,
        VkSystemAllocationScope scope
    )
    {
        PROFILE_SCOPE_FUNCTION();
        i_allocator* impl = ptr_cast<i_allocator*>( context );
        void* result_ = impl->allocate_raw( bytes, alignment );
        if constexpr (vulkan_config_trace_allocations)
        {
            VULKAN_LOGF( "Allocated {} bytes in driver", bytes );
            VULKAN_LOGF( "\tAllocation Scope: {} ", string_VkSystemAllocationScope(scope) );
            VULKAN_LOGF( "Allocation Address: {} ", result_ );
        }
        return result_;
    };

    result.pfnReallocation = [] (
        void* context,
        void* original,
        size_t bytes,
        size_t alignment,
        VkSystemAllocationScope scope
    )
    {   i_allocator* impl = ptr_cast<i_allocator*>( context );
        /* Stupid API convention mandated by the Vulkan spec

           OFFICIAL DOCUMENTATION: If pOriginal is NULL, then pfnReallocation must behave equivalently
           to a call to PFN_vkAllocationFunction with the same parameter values
           (without pOriginal).

           If size is zero, then pfnReallocation must behave equivalently to a
           call to PFN_vkFreeFunction with the same pUserData parameter value,
           and pMemory equal to pOriginal. */
        void* result_ = nullptr;
        if (original)
        {   if (bytes == 0) {   impl->deallocate( original ); }
            else { result_ = impl->allocate_relocate( original, bytes ); }
        }
        else { result_ = impl->allocate_raw( bytes, bytes ); }

        if constexpr (vulkan_config_trace_allocations)
        {
            VULKAN_LOGF( "Reallocated {:>10} bytes in driver", bytes );
            VULKAN_LOGF( "Allocation Scope: {:>10} ", string_VkSystemAllocationScope(scope) );
            VULKAN_LOGF( "Re-allocation Address: {:>10} ", original );
        }
        if (result_ == nullptr)
        {   TYON_BREAK();
        }
        return result_;
    };

    result.pfnFree = [] (
        void* context,
        void* address
    )
    {
        /* OFFICIAL DOCUMENTATION: pMemory  may be NULL,  which the callback  must handle
           safely.  If pMemory  is non-NULL,  it  must be  a pointer  previously
           allocated by pfnAllocation or pfnReallocation. The application should
           free this memory. */
        i_allocator* impl = ptr_cast<i_allocator*>( context );
        // Don't need to bother anyone with logs or calls if tihs occurs
        if (context == nullptr) {   return; }
        impl->deallocate( address );
        if constexpr (vulkan_config_trace_allocations)
        {
            VULKAN_LOGF( "Deallocated data in driver    Address: {:>10} ", address );
        }
    };

    // Allocation Notification
    result.pfnInternalAllocation = [](
        void* context,
        size_t bytes,
        VkInternalAllocationType type,
        VkSystemAllocationScope scope
    )
    {
        if constexpr (vulkan_config_trace_allocations)
        {
            VULKAN_LOG( "Internal Allocation Event" );
            VULKAN_LOGF( "\tAllocator Address: {:>10}", (void*)(context) );
            VULKAN_LOGF( "\tAllocation Type: {:>10} ", string_VkInternalAllocationType(type) );
            VULKAN_LOGF( "\tAllocated Bytes: {:>10} ", string_VkSystemAllocationScope(scope) );
        }
    };

    result.pfnInternalFree = [](
        void* context,
        size_t bytes,
        VkInternalAllocationType type,
        VkSystemAllocationScope scope
    )
    {
        if constexpr (vulkan_config_trace_allocations)
        {
            VULKAN_LOG( "Internal Deallocation Event" );
            VULKAN_LOGF( "\tAllocator Address: {:>10}", (void*)(context) );
            VULKAN_LOGF( "\tAllocation Type: {:>10} ", string_VkInternalAllocationType(type) );
            VULKAN_LOGF( "\tAllocated Bytes: {:>10} ", string_VkSystemAllocationScope(scope) );
        }
    };
    return result;
}

PROC vulkan_label_object( u64 handle, VkObjectType type, fstring name ) -> void
{
    PROFILE_SCOPE_FUNCTION();
    // NOTE: This is the depreceated version of the struct/function, this crashed when I used it.
    // VkDebugMarkerObjectNameInfoEXT name_args {};
    char* s = memory_allocate_raw( name.size() + 1 );
    name.copy( s, name.size(), 0 );
    VkDebugUtilsObjectNameInfoEXT name_args {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = s,
    };
    dyn::vkSetDebugUtilsObjectNameEXT( g_vulkan->logical_device, &name_args );
}

template <typename t_return, typename t_source>
PROC as( t_source arg ) -> t_return;

/** Narrowing cast for VkExtent2D */
template<>
PROC as( v2 arg ) -> VkExtent2D
{
    return VkExtent2D { u32(arg.x), u32(arg.y) };
}

PROC vulkan_extent_2d_cast( v2 arg ) -> VkExtent2D
{
    return VkExtent2D { static_cast<u32>(arg.x), u32(arg.y) };
}

PROC vulkan_shader_init( vulkan_shader* arg ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    ERROR_GUARD( arg->id.valid() == false,
                 "Using init on a object that's already initialized can't possible make sense." );
    ERROR_GUARD( arg->code_binary, "Code provided must be binary SPIR-V currently" );
    arg->code = file_load_binary( arg->code.filename );
    if (arg->code.file_loaded == false)
    {   VULKAN_ERRORF( "Failed to load SPIR-V code Name: '{}' File: '{}'", arg->name, arg->code.filename );
        return false;
    }

    arg->id = uuid_generate();

    VkShaderModuleCreateInfo shader_args{};
    shader_args.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_args.codeSize = arg->code.memory.size;
    shader_args.pCode = arg->code.memory.data;

    VkShaderModule& platform_module = arg->platform_module;
    auto module_ok = vkCreateShaderModule(
        g_vulkan->logical_device, &shader_args, g_vulkan->vk_allocator, &platform_module );
    if (module_ok != VK_SUCCESS)
    {
        VULKAN_ERRORF( "Failed to create shader module: {}", arg->name );
        return false;
    }
    vulkan_label_object(
        (u64)platform_module, VK_OBJECT_TYPE_SHADER_MODULE, arg->name + "_shader" );
    fstring name = arg->name;
    g_vulkan->resources.push_cleanup( [name, platform_module]{
        VULKAN_LOG( "Destroying shader module:", name );
        vkDestroyShaderModule(
            g_vulkan->logical_device, platform_module, g_vulkan->vk_allocator ); } );

    VULKAN_LOGF( "Created shader module: {}", arg->name );
    return true;
}

// Mesh specific pipeline initialization
PROC vulkan_pipeline_mesh_init( vulkan_pipeline* arg ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    if (arg->id.valid())
    {   VULKAN_ERRORF("{} Using init on a object that's already initialized doesn't make any sense.",
                      arg->name );
        return false;
    }
    if (arg->shaders.size() <= 0)
    {   VULKAN_ERRORF( "{} Tried to create a shader pipeline with no shaders attached.",
                       arg->name );
        return false;
    }

    arg->id = uuid_generate();
    if (arg->swapchain == nullptr)
    {   VULKAN_LOGF( "No swapchain provided to pipeline '{}', using global swapchain", arg->name );
        arg->swapchain = &g_vulkan->swapchain;
    }
    if (arg->swapchain == nullptr)
    {   VULKAN_ERRORF( "No valid swapchain usable for pipeline, failed to create mesh pipeline {}",
                       arg->name );
        return false;
    }

    array<VkPipelineShaderStageCreateInfo> stages;
    stages.change_allocation( arg->shaders.size() );
    for (i64 i=0; i < arg->shaders.size(); ++i)
    {
        auto& x_shader = arg->shaders[i];
        stages.push_tail( VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = x_shader.stage_flag,
                .vk_module = x_shader.platform_module,
                .pName = x_shader.entry_point.c_str(),
            });
    }

    /* SECTION: Setup shader input locations as "vertex-binding"

     Here we use a pre-defined shader input/buffer format that will be passed
     along to the shader. The data will later by bound using `vkCmdBindVertexBuffers`

     Format:
     0 - Vertex Normal
     1 - Vertex Positions
     2 - Texture Interpolated Diffuse Colour
    */
    array<VkVertexInputBindingDescription> bindings {
        /** NOTE: If we're only using 1 buffer to represent multiple values we only need 1 binding */
        {   // Vertex Binding
            .binding = 0, // vertex attribute binding/slot. leave as 0
            .stride = 4 * 6, // 1 v3 color, 1 v3 vertex
            // Not sure what this is. think it means pulling from instance wide stuffs?
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    array<VkVertexInputAttributeDescription> vertex_attributes {
        // {
        //     .location = 0, // shader specific binding location
        //     .binding = 0,
        //     // This uses the color format for some strange reason. This is a 32-bit vec3
        //     .format = VK_FORMAT_R32G32B32_SFLOAT,
        //     .offset = 0, // 3 32-bit normals to vertex data
        // },
        {
            .location = 1, // shader specific binding location
            .binding = 0,
            // This uses the color format for some strange reason. This is a 32-bit vec3
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset =  4 * 3, // 3 32-bit normals to vertex data
        }
    };

    // Mesh vertex input args
    VkPipelineVertexInputStateCreateInfo vertex_args {};
    vertex_args.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_args.pVertexBindingDescriptions =  bindings.data;
    vertex_args.vertexBindingDescriptionCount = bindings.size();
    vertex_args.pVertexAttributeDescriptions = vertex_attributes.data;
    vertex_args.vertexAttributeDescriptionCount = vertex_attributes.size();

    // Mesh rendering settings
    VkPipelineInputAssemblyStateCreateInfo input_args {};
    input_args.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_args.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_args.primitiveRestartEnable = VK_FALSE;

    // Rasterizer settings
    VkPipelineRasterizationStateCreateInfo raster_args {};
    raster_args.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_args.polygonMode = VK_POLYGON_MODE_FILL;
    // raster_args.polygonMode = VK_POLYGON_MODE_LINE;
    raster_args.lineWidth = 1.0f;
    raster_args.cullMode = VK_CULL_MODE_BACK_BIT;
    // TODO: TEST no cull mode
    raster_args.cullMode = VK_CULL_MODE_NONE;
    raster_args.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_args.depthBiasEnable = VK_FALSE;
    raster_args.depthBiasConstantFactor = 0.0f;         // Optional
    raster_args.depthBiasClamp = 0.0f;                  // Optional
    raster_args.depthBiasSlopeFactor = 0.0f;            // Optional

    VkViewport viewport_config {
        // Upper left coordinates
        .x = 0,
        .y = 0,
        .width = float(arg->swapchain->vk_present_size.width),
        .height = float(arg->swapchain->vk_present_size.height),
        // Configurable viewport depth, can configurable but usually between 0 and 1
        .minDepth = 0.0,
        .maxDepth = 1.0
    };

    // Only render into a certain portion of the viewport with scissors
    VkRect2D scissor_config {
        VkOffset2D { 0, 0 },
        arg->swapchain->vk_present_size
    };

    VkPipelineViewportStateCreateInfo viewport_args {};
    viewport_args.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_args.pViewports = &viewport_config;
    // viewportCount and scissorCount must be the same
    viewport_args.viewportCount = 1;
    viewport_args.pScissors = &scissor_config;
    viewport_args.scissorCount = 1;

    VkPipelineMultisampleStateCreateInfo multisample_args {};
    multisample_args.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_args.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_args.sampleShadingEnable = true;
    multisample_args.minSampleShading = 0.2f;
    // wut is this
    multisample_args.pSampleMask = nullptr;
    multisample_args.alphaToCoverageEnable = false;
    multisample_args.alphaToOneEnable = false;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = (
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo color_blend_args{};
    color_blend_args.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_args.logicOpEnable = VK_FALSE;
    color_blend_args.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blend_args.attachmentCount = 1;
    color_blend_args.pAttachments = &color_blend_attachment;
    color_blend_args.blendConstants[0] = 0.0f; // Optional
    color_blend_args.blendConstants[1] = 0.0f; // Optional
    color_blend_args.blendConstants[2] = 0.0f; // Optional
    color_blend_args.blendConstants[3] = 0.0f; // Optional

    /* NOTE: A pipeline can be set to have some of it's state become dynamic
       after creation.  Which may be a performance benefit for tasks its
       relevant to. This setion describes what state can be dynamic instead of
       static.*/
    array<VkDynamicState> dynamic_states_selected = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_args {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = cast<u32>(dynamic_states_selected.size()),
        .pDynamicStates = dynamic_states_selected.data,
    };

    /** Official Documentation: Descriptor Set

        An object that resource descriptors are written into via the API, and that can be bound to a
        command buffer such that the descriptors contained within it can be accessed from shaders.
        Represented by a VkDescriptorSet object.
    */

    array<VkDescriptorSetLayoutBinding> resource_descriptors = {
        {
            // Generic data uniform
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            // if it's an array of resources we're sending to the shader.
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };
    VkDescriptorSetLayoutCreateInfo descriptor_layout_args {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0x0,
        .bindingCount = u32(resource_descriptors.size()),
        .pBindings = resource_descriptors.data,
    };

    VkPushConstantRange push_constant_range {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        // Data start offset
        .offset = 0,
        .size = sizeof(  vulkan_mesh_shader_push )
    };

    /* Pipeline Layout: "An object defining the set of resources (via a
       collection of descriptor set layouts) and push constants used by
       pipelines that are created using the layout. Used when creating a
       pipeline and when binding descriptor sets and setting push constant
       values. Represented by a VkPipelineLayout object." */
    VkPipelineLayoutCreateInfo layout_args {};
    layout_args.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_args.pSetLayouts = &arg->platform_descriptor_layout;
    layout_args.setLayoutCount = 1;
    layout_args.pushConstantRangeCount = 1;
    layout_args.pPushConstantRanges = &push_constant_range;

    auto descriptor_layout_ok = vkCreateDescriptorSetLayout(
        g_vulkan->logical_device,
        &descriptor_layout_args,
        g_vulkan->vk_allocator,
        &arg->platform_descriptor_layout
    );
    g_vulkan->resources.push_cleanup( [resource_layout = arg->platform_descriptor_layout] {
        VULKAN_LOG( "Destroying descriptor layout set" );
        vkDestroyDescriptorSetLayout(
            g_vulkan->logical_device,
            resource_layout,
            g_vulkan->vk_allocator
        );
    });

    auto layout_bad = vkCreatePipelineLayout(
        g_vulkan->logical_device,
        &layout_args,
        g_vulkan->vk_allocator,
        &arg->platform_layout
    );
    if (layout_bad)
    {   VULKAN_ERROR( "Faled to create pipeline layout" );
        return false;
    }
    g_vulkan->resources.push_cleanup( [arg] {
        VULKAN_LOG( "Destroying pipeline layout" );
        vkDestroyPipelineLayout( g_vulkan->logical_device, arg->platform_layout,
                                 g_vulkan->vk_allocator );
    });

    /* SECTION: Pipeline creation args
     *
     * Now We have all the pipeline information set we can assemble it into
     * creation args */
    VkGraphicsPipelineCreateInfo pipeline_args {};
    pipeline_args.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_args.pStages = stages.data;
    pipeline_args.stageCount = u32(stages.size());
    pipeline_args.pVertexInputState = &vertex_args;
    pipeline_args.pInputAssemblyState = &input_args;
    pipeline_args.pViewportState = &viewport_args;
    pipeline_args.pRasterizationState = &raster_args;
    pipeline_args.pMultisampleState = &multisample_args;
    // pipeline_args.pDepthStencilState = nullptr; // Optional
    pipeline_args.pColorBlendState = &color_blend_args;
    pipeline_args.pDynamicState = &dynamic_state_args;
    pipeline_args.layout = arg->platform_layout;
    pipeline_args.renderPass = g_vulkan->render_pass;
    pipeline_args.subpass = 0;
    // pipeline_args.basePipelineHandle = VK_NULL_HANDLE; // Optional
    // pipeline_args.basePipelineIndex = -1; // Optional

    // Provide pipeline cache here if relevant
    auto pipeline_ok = vkCreateGraphicsPipelines(
        g_vulkan->logical_device,
        VK_NULL_HANDLE,
        1,
        &pipeline_args,
        g_vulkan->vk_allocator,
        &arg->platform_pipeline );
    if (pipeline_ok)
    {   VULKAN_ERROR( "Failed to create graphics pipeline" ); return false; }
    VULKAN_LOG( "Created graphics pipeline" );
    g_vulkan->resources.push_cleanup( [pipeline = arg->platform_pipeline] {
        VULKAN_LOG( "Destroying graphics pipeline" );
        vkDestroyPipeline( g_vulkan->logical_device, pipeline,
                           g_vulkan->vk_allocator );
    });

    return false;
}

PROC vulkan_swapchain_init( vulkan_swapchain* arg, VkSwapchainKHR reuse_swapchain )
    -> fresult
{
    PROFILE_SCOPE_FUNCTION();

    TracyCZoneN( zone_1, "Zone 1", true );
    ERROR_GUARD( arg->id.valid() == false,
                 "Using init on a object that's already initialized can't possible make sense." );
    ERROR_GUARD( arg->initialized == false, "Called init on an already initialized swapchain" );
    auto self = g_vulkan;
    arg->id = uuid_generate();

    VkAllocationCallbacks allocator = vulkan_allocator_create_callbacks(
        g_vulkan->allocator );

    if  (g_vulkan->surface)
    {
        /* We need to know the capabilities of the surface associated with the physical device
           So we retreive those capabilities */
        VkSurfaceCapabilitiesKHR surface_capabilities {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            g_vulkan->device, g_vulkan->surface, &surface_capabilities );
        arg->vk_present_size = surface_capabilities.currentExtent;
        VkExtent2D min = surface_capabilities.minImageExtent;
        VkExtent2D max = surface_capabilities.maxImageExtent;
        VkExtent2D current = surface_capabilities.currentExtent;

        VULKAN_LOGF(
            "Present surface/image extent min: {} {} max: {} {} current {} {}",
            min.width, min.height, max.width, max.height, current.width, current.height
        );

        if (current.width == u32(-1) || current.height == u32(-1) )
        {   VULKAN_LOG( "Found weird current surface size, we will try to request an appropriate size" );
            arg->vk_present_size.width = std::clamp( arg->present_size.width, min.width, max.width );
            arg->vk_present_size.height = std::clamp( arg->present_size.height, min.height, max.height );
        }

        TracyCZoneEnd( zone_1 );
        TracyCZoneN( zone_2, "Zone 2", true );
        // Also diagnostics for device formats
        array<VkSurfaceFormatKHR> surface_formats;
        u32 n_surfaces {};
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            g_vulkan->device, g_vulkan->surface, &n_surfaces, nullptr );
        surface_formats.resize( n_surfaces );
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            g_vulkan->device, g_vulkan->surface, &n_surfaces, surface_formats.data );

        TracyCZoneEnd( zone_2 );
    }

    TracyCZoneNC( zone_3, "Zone 3", 0xA04040, true );

    // --  Create swapchan for presentation of images to windows --

    /* NOTE: noooooooooo, my platform doesn't support swapchain maintainence
       extension. The spec was made after my current nVIDIA driver */
    // VkSwapchainPresentScalingCreateInfoKHR swapchain_present_scaling_args {
    //     .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR
    // };

    VkSwapchainCreateInfoKHR swapchain_args {};
    swapchain_args.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_args.minImageCount = g_vulkan->frames_inflight_count;
    swapchain_args.surface = g_vulkan->surface;
    swapchain_args.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    // The format must match the physical surface formats
    swapchain_args.imageFormat = self->swapchain_image_format;
    swapchain_args.imageExtent = arg->vk_present_size;
    swapchain_args.imageArrayLayers = 1; // More than 1 if a stereoscopic application
    // TRANSFER_DST bit is useful for copying images directly into the framebuffer
    swapchain_args.imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    swapchain_args.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_args.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    swapchain_args.queueFamilyIndexCount = 2;
    u32 family_indexes[] = { u32(self->graphics_queue_family), u32(self->present_queue_family) };
    swapchain_args.pQueueFamilyIndices = family_indexes;
    swapchain_args.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_args.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_args.oldSwapchain = reuse_swapchain;

    auto swapchain_ok = vkCreateSwapchainKHR(
        g_vulkan->logical_device,
        &swapchain_args,
        g_vulkan->vk_allocator,
        &arg->platform_swapchain
    );
    if (swapchain_ok != VK_SUCCESS)
    {
        VULKAN_ERRORF( "Failed to initialize swapchain {}", string_VkResult(swapchain_ok) );
        return false;
    }
    VULKAN_LOG( "Initialized presentation swapchain" );
    arg->initialized = true;

    TracyCZoneEnd( zone_3 );
    TracyCZoneNC( zone_4, "Zone 4", 0xA040A0, true );

    VkSurfaceCapabilitiesKHR surface_capabilities_2 {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        g_vulkan->device, g_vulkan->surface, &surface_capabilities_2 );

    array<VkImage>& swapchain_images = g_vulkan->swapchain_images;
    u32 n_swapchain_images = 0;
    vkGetSwapchainImagesKHR( g_vulkan->logical_device, arg->platform_swapchain,
                             &n_swapchain_images, nullptr );
    swapchain_images.resize( n_swapchain_images );
    vkGetSwapchainImagesKHR( g_vulkan->logical_device, arg->platform_swapchain,
                             &n_swapchain_images, swapchain_images.data );

/** Views describe how to interpret VkImage's, VkImages are related to
    textures and framebuffers */
    array<VkImageView>& swapchain_image_views = g_vulkan->swapchain_image_views;
    array<VkFramebuffer>& swapchain_buffers = g_vulkan->swapchain_framebuffers;
    array<VkResult> view_errors;
    array<VkResult> framebuffer_errors;
    array<VkResult> fence_errors;
    swapchain_image_views.resize( n_swapchain_images );
    swapchain_buffers.resize( n_swapchain_images );

    view_errors.resize( n_swapchain_images );
    framebuffer_errors.resize( n_swapchain_images );
    fence_errors.resize( n_swapchain_images );

    // Don't forget to setup object arrays as well
    arg->frame_end_fences.resize( n_swapchain_images );
    TracyCZoneEnd( zone_4 );
    TracyCZoneNC( zone_5, "Zone 5", 0xA0A040, true );
    for (i32 i = 0; i < n_swapchain_images; i++)
    {
        // Make synchronization primitives
        VkFenceCreateInfo fence_args {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            // Start signalled
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        fence_errors[i] = vkCreateFence(
            g_vulkan->logical_device, &fence_args, g_vulkan->vk_allocator, arg->frame_end_fences.address(i) );
        vulkan_label_object( (u64)arg->frame_end_fences[i], VK_OBJECT_TYPE_FENCE,
                             fmt::format( "{}_frame_end_fence_{}", arg->name, i ) );


        VkImageViewCreateInfo view_args{};
        view_args.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_args.image = swapchain_images[i];
        view_args.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_args.format = swapchain_args.imageFormat;
        view_args.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_args.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_args.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_args.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_args.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_args.subresourceRange.baseMipLevel = 0;
        view_args.subresourceRange.levelCount = 1;
        view_args.subresourceRange.baseArrayLayer = 0;
        view_args.subresourceRange.layerCount = 1;
        view_errors[i] = vkCreateImageView(
            g_vulkan->logical_device, &view_args, g_vulkan->vk_allocator, &swapchain_image_views[i] );
        vulkan_label_object( (u64)swapchain_image_views[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                             fmt::format( "{}_swapchain_image_view_{}", arg->name, i ) );

        VkImageView attachments[] = {
            swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebuffer_args{};
        framebuffer_args.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_args.renderPass = self->render_pass;
        framebuffer_args.attachmentCount = 1;
        framebuffer_args.pAttachments = attachments;
        framebuffer_args.width = arg->vk_present_size.width;
        framebuffer_args.height = arg->vk_present_size.height;
        framebuffer_args.layers = 1;

        framebuffer_errors[i] = vkCreateFramebuffer(
            g_vulkan->logical_device, &framebuffer_args,
            g_vulkan->vk_allocator, &swapchain_buffers[i] );
        vulkan_label_object( (u64)swapchain_buffers[i], VK_OBJECT_TYPE_FRAMEBUFFER,
                             fmt::format( "{}_swapchain_buffer_{}", arg->name, i ) );
        ERROR_GUARD(view_errors[i] == VK_SUCCESS , "view creation error" );
        ERROR_GUARD(framebuffer_errors[i] == VK_SUCCESS , "fraembuffer creation error" );
    }
    TracyCZoneEnd( zone_5 );
    ERROR_GUARD( arg->id.valid(), "All entities have an  UUID" );
    ERROR_GUARD( arg->platform_swapchain, "Function ended with null swapchain handle" );
    return false;
}

PROC vulkan_swapchain_destroy( vulkan_swapchain* arg ) -> void
{
    PROFILE_SCOPE_FUNCTION();
    VkAllocationCallbacks allocator = vulkan_allocator_create_callbacks(
        g_vulkan->allocator );

    // Can't destroy resources that are still in use
    vkDeviceWaitIdle( g_vulkan->logical_device );
    vkDestroyFramebuffer(
        g_vulkan->logical_device,
        arg->platform_framebuffer,
        g_vulkan->vk_allocator
    );
    // TODO: Switch size to using n_images
    for (i32 i=0; i < g_vulkan->swapchain_image_views.size(); ++i)
    {
        vkDestroyFramebuffer(
            g_vulkan->logical_device, g_vulkan->swapchain_framebuffers[i], g_vulkan->vk_allocator );
        // TODO: Make sure this are associated with swapchain instead later for
        // multi-swapchain support
        vkDestroyImageView(
            g_vulkan->logical_device, g_vulkan->swapchain_image_views[i], g_vulkan->vk_allocator );
        vkDestroyFence( g_vulkan->logical_device, arg->frame_end_fences[i], g_vulkan->vk_allocator );
    }
    // Lazy destroy swapcarch vmhain because the handle still needs to be reused by next swapchain
    auto swapchain = arg->platform_swapchain;
    g_vulkan->resources.push_cleanup( [=]
    {
        vkDestroySwapchainKHR(
            g_vulkan->logical_device,
            swapchain,
            g_vulkan->vk_allocator
        );
    });
    *arg = vulkan_swapchain {};
}

PROC vulkan_buffer_create(
    fstring name,
    i64 size,
    VkBufferUsageFlags type,
    VkSharingMode sharing_mode
)   -> vulkan_buffer
{
    PROFILE_SCOPE_FUNCTION();
    vulkan_buffer result = {
        .name = name,
        .size = size,
        .type = type,
        .sharing_mode = sharing_mode
    };

    VkBufferCreateInfo buffer_args {};
    buffer_args.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_args.size = u32(size);
    buffer_args.usage = type;
    buffer_args.sharingMode = sharing_mode;

    auto buffer_bad = vkCreateBuffer(
        g_vulkan->logical_device, &buffer_args, g_vulkan->vk_allocator, &result.buffer );
    if (buffer_bad)
    {   VULKAN_ERRORF( "Failed to create buffer '{}'", name );
        return result;
    }
    result.id = uuid_generate(); // Valid objects have a non-zero UUID
    vulkan_label_object( (u64)result.buffer, VK_OBJECT_TYPE_BUFFER, name );
    g_vulkan->buffers.push_tail( result );

    // Set cleanup code
    VkBuffer _buffer = result.buffer;
    g_vulkan->resources.push_cleanup( [_buffer] {
        vkDestroyBuffer( g_vulkan->logical_device, _buffer, g_vulkan->vk_allocator );
    });

    // TODO: return pointer or uid
    return result;
}

PROC vulkan_memory_find_best_type_index(
    std::bitset<32> valid_type_bits, VkMemoryPropertyFlags preferred_flags ) -> monad<i32>
{
    monad<i32> result;
    static bool first_run = true;
    // Find all of the memory properties to search through
    VkPhysicalDeviceMemoryProperties memory_props {};
    vkGetPhysicalDeviceMemoryProperties( g_vulkan->device, &memory_props );

    i32 best_index = -1;
    i64 best_heap_size = 0;
    /** How many flags are missing */
    i32 best_missing_flag_n = 32;
    std::bitset<32> best_flag = ~u32(0);
    /** If the current memory type being used an exact match of preferred flags */
    i32 x_missing_flag_n = 32;
    for (i32 i=0; i < memory_props.memoryTypeCount; ++i)
    {
        VkMemoryType x_memory_type = memory_props.memoryTypes[ i ];
        /** The heap associated with the index */
        VkMemoryHeap x_heap = memory_props.memoryHeaps[ x_memory_type.heapIndex ];
        std::bitset<32> memory_type = x_memory_type.propertyFlags;

        // Reset these variables before we start working on them
        x_missing_flag_n = 32;

        const std::bitset<32> preferred = preferred_flags;
        const bool flags_different = absolute( preferred.count() - memory_type.count() );
        // Flags not present in type using bitwise trick
        x_missing_flag_n = (preferred & (~memory_type)).count();

        // Print heap statistics
        if (first_run)
        {
            VULKAN_LOGF( "{:<40}Memory Type Index: {} Property Flags: {:b}",
                         "", i, x_memory_type.propertyFlags );
            VULKAN_LOGF(
                "Heap Stats: Heap Index: [{}] Heap Size : [{}] Heap Flags: [{:b}]",
                x_memory_type.heapIndex, x_heap.size, x_heap.flags
            );
        }

        bool more_than_one_exact_match = (x_missing_flag_n == 0) && (best_missing_flag_n == 0) &&
            (best_index != -1);
        if (more_than_one_exact_match)
        {   VULKAN_LOGF( "Found more than one suitible memory type {} and {} for memory type bits {:b}",
                     i, best_index, preferred_flags );
        }

        bool less_missing_flags = (x_missing_flag_n < best_missing_flag_n);
        bool bigger_heap = (x_missing_flag_n == best_missing_flag_n) && (x_heap.size > best_heap_size);
        if (less_missing_flags)
        {   best_index = i;
            best_missing_flag_n = x_missing_flag_n;
            best_flag = memory_type;
            best_heap_size = x_heap.size;
        }
    }
    if (best_index == -1)
    {   VULKAN_ERROR( "No valid memory type found! typeBits {} preferred_flags {}" )
        result.error = true;
    }
    first_run = false;
    result.value = best_index;
    return result;
}

PROC vulkan_memory_allocate_block( vulkan_memory* context, vulkan_memory_block_args* arg ) -> fresult
{
    if (arg->memory_type_index < 0)
    {   VULKAN_ERROR( "Called allocate block with negative memory_type_index" );
        return false;
    }
    if (arg->size <= 0)
    {   VULKAN_ERROR( "Called allocate block with no valid size" );
        return false;
    }

    VkMemoryAllocateInfo memory_args {};
    memory_args.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_args.pNext = nullptr;
    memory_args.allocationSize = arg->size;
    memory_args.memoryTypeIndex = arg->memory_type_index;

    vulkan_memory_block* new_block = &context->blocks.push_tail({});

    // Actually allocate the block
    auto memory_bad = vkAllocateMemory(
    g_vulkan->logical_device, &memory_args, g_vulkan->vk_allocator, &new_block->memory);

    if (memory_bad)
    {   VULKAN_ERRORF( "Failed to allocate general memory object: {}",
                       string_VkResult( memory_bad ) );
        context->blocks.pop_tail();
        return false;
    }

    /* SECTION: Successful allocation, we can fill in the block data, null
       handle or 0 size is an invalid block */
    new_block->index = context->blocks.size() - 1;
    new_block->size = arg->size;
    new_block->memory_type_index = arg->memory_type_index;
    new_block->memory_flags = arg->memory_flags;
    new_block->host_mappable = (arg->memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    new_block->largest_entry = arg->size;
    // Give a fairly generous amount of entries, we will use these alot
    // TODO: This is kind of a hack to try to evade pointer invalidation issues, this needs to
    // be fixed in a more sophisticated way
    new_block->entries.nodes = {};
    new_block->entries.nodes.resize( 4096 );

    vulkan_memory_entry start_entry = {
        .block = (context->blocks.size() - 1),
        .index = 0,
        .position = 0,
        // NOTE: Size is only the actively used bytes, unallocated entries have size as 0
        .size = 0,
        .reserved_size = arg->size,
        .alignment = 1,
        .type = e_vulkan_memory_object::none
    };
    // Add entry to the list and free list
    auto start_node = new_block->entries.push_tail( start_entry );
    new_block->free_entries.push_tail( start_node->index );

    vulkan_label_object( (u64)new_block->memory, VK_OBJECT_TYPE_DEVICE_MEMORY, "device_memory_block" );
    VULKAN_LOGF( "Allocated device memory block. Size: {:>10}", new_block->size );
    return true;
}


PROC vulkan_memory_init( vulkan_memory* arg ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    /* SECTION: Create vertex buffer for describing a mesh

       Here we create device memory to hold the data describing a mesh, like
       vertecies and texture data. The buffer is created per mesh and is
       completely seperate from the pipeline, which can service any number of mesh.

       As per the Vulkan Specification Glossary- "memory", is a handle to the
       actual physical memory or a memory allocation we are talking about.

       A "buffer", is "a resource that represents a linear array of data in
       device memory. Represented by a VkBuffer" object. A memory object must be
       bound to a buffer to be used properly- even for alternate types like
       VkImage, you cannot actually write to it without first writing through a
       memory mapped buffer.
    */
    u32 buffer_shared_bits = ~0;
    auto check_buffer_memory_requirements = [&buffer_shared_bits] (
        VkBufferUsageFlagBits buffer_type
    ) -> monad<VkMemoryRequirements>
    {
        monad<VkMemoryRequirements> result;
        vulkan_object_memory_info info;
        vulkan_buffer buffer = vulkan_buffer_create(
            "requirements_buffer_check", 32, buffer_type );
        VkMemoryRequirements requirements;
        if (buffer.buffer)
        {   vkGetBufferMemoryRequirements(
                g_vulkan->logical_device, buffer.buffer, &requirements );
            // We can can just clean up the buffers immediately after getting the requirements.
            vkDestroyBuffer( g_vulkan->logical_device, buffer.buffer, g_vulkan->vk_allocator );

            // Print reported memory types
            fstring_view name = string_VkBufferUsageFlagBits( buffer_type );
            info.memory_type_bits = requirements.memoryTypeBits;

            /* Check if all requirements are the same This will collapse on 0 if
               atleast  1  bit  isn't   identical  across  all  buffer's  memory
               requirement types.

               NOTE: This isn't a very good way to do things but we can simplify
               logic a  lot if all  buffer types are the  same. We will  have to
               check again if it's coherent with other types too ie Vkimage*/
            buffer_shared_bits &= requirements.memoryTypeBits;
            VULKAN_LOGF( "memoryTypeBits {} '{}'", info.memory_type_bits, name );
        }
        else
        {   VULKAN_LOG( "Failed to even create a buffer" );
            result.error = true;
        }
        result.value = requirements;
        return result;
    };

    VULKAN_LOG( "Testing buffers for memory type compatability" );
    // TODO: Need to do image requirements and other stuff too
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT );
    monad<VkMemoryRequirements> transfer_destination_requirements = check_buffer_memory_requirements(
        VK_BUFFER_USAGE_TRANSFER_DST_BIT );

    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT );
    check_buffer_memory_requirements(
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT );
    // NOTE: Everything after VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT is not tested.

    /* NOTE: Is HOST_COHERENT actually slower than DEVICE_LOCAL?

       NOTE: Somewhat, it's a  bit of a mystery how it's  handled under the hood
       which  leads people  to suggest  it's a  bit slower  than you  can do  by
       hand. Which is true, but what's also slow is getting the staging CPU->GPU
       copy wrong. What is also worth  considering is there is a small "staging"
       region accessible  throuh the PCIe  BAR. A max of  around 256 Mib,  it is
       only  increased  by  resizable  BAR,   but  this  is  supposedly  faster,
       HOST_VISIBLE, and DEVICE_LOCAL at the same time. So it is a very valuable
       chunk of memory to have access to. */

    /* NOTE: Allocate  a TRANSFER_DST buffer  memory type as the  default block,
     * most objects you need  will transfer a transfer command to  it So it only
     * makes sense for it  to be the default. It can be  deallocated later if it
     * isn't a  good fit,  it would  be really  nice to  have a  block allocated
     * upfront instead  of waiting way down  the line until we  start writing to
     * memory objects. */

    if (transfer_destination_requirements.error == false)
    {
        i32 memory_type = vulkan_memory_find_best_type_index(
            transfer_destination_requirements.value.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ).copy_default( -1 );

        vulkan_memory_block_args block_args = {
            .size = arg->device_block_size,
            .memory_type_index = memory_type,
            .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        // Allocate some area for staging buffer too
        VkMemoryPropertyFlags staging_flags = (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                 VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        i32 staging_memory_type = vulkan_memory_find_best_type_index(
            transfer_destination_requirements.value.memoryTypeBits,
            staging_flags
        ).copy_default( -1 );

        vulkan_memory_block_args staging_block_args = {
            .size = arg-> staging_block_size,
            .memory_type_index = staging_memory_type,
            .memory_flags = staging_flags
        };

        vulkan_memory_allocate_block( arg, &block_args );
        vulkan_memory_allocate_block( arg, &staging_block_args );
    }

    return true;
}

PROC vulkan_memory_get_block( vulkan_memory* context, vulkan_memory_entry* entry )
-> vulkan_memory_block&
{
    return context->blocks[ entry->block ];
}

PROC vulkan_memory_allocate_untyped( vulkan_memory* context, vulkan_allocate_args args )
    -> monad<vulkan_memory_entry>
{
    monad<vulkan_memory_entry> result;
    result.error = true;
    i32 i_attempts = 0;
    i32 attempts_limit = 3;
    for (; i_attempts < attempts_limit; ++i_attempts)
    {
        // NOTE: We need extra bytes for internal things like alignment and redzone so we use this
        i64 target_size = (args.size + context->redzone_bytes);
        // File suitible block
        search_result<vulkan_memory_block> block_search = context->blocks.linear_search(
            [args, target_size] ( vulkan_memory_block& block ) {
                bool enough_space = (block.largest_entry >= target_size);
                bool suitible_memory_type = (block.memory_type_index == args.memory_type_index);
                return (enough_space && suitible_memory_type);
            });
        vulkan_memory_block* target_block = block_search.match;

        // Find free entry in list
        i64 list_size = (target_block ? target_block->free_entries.size() : 0);
        vulkan_memory_node* x_entry = nullptr;
        bool space_found = false;
        for (i64 i=0; i < list_size; ++i)
        {
            // Walk in backwards order to take advantage of free entries being added to the end
            const i64 i_inverse = (list_size - 1 - i);
            const i64 entry_index = target_block->free_entries[ i_inverse ];
            x_entry = target_block->entries.nodes.address( entry_index );
            if (x_entry->value.reserved_size >= target_size)
            {   ERROR_GUARD( x_entry->value.type == e_vulkan_memory_object::none,
                             "A filled type implies we're trying to used an already used entry" );
                space_found = true;
                break;
            }
        }
        if (space_found == false)
        {
            // No space, try to allocate a new block
            vulkan_memory_block_args block_args {
                // NOTE: Need to use smaller blocks if using small PCIe BAR staging memory
                .size = (args.transfer_buffer ? context->staging_block_size :
                         context->device_block_size),
                .memory_type_index = args.memory_type_index,
                .memory_flags = args.memory_flags
            };
            vulkan_memory_allocate_block( context, &block_args );
            continue;
        }
        /* Space found, split the entry into 2, one unallocated and one allocated entry
           NOTE: Keep the original allocation in it's original index for free list convenient
           and insert the new entry before it

           NOTE: I messed up here previously and tried */
        vulkan_memory_node* space_entry = x_entry;
        vulkan_memory_node* new_entry = target_block->entries.insert_before( x_entry, {});
        // Copy the old data over and then we'll write over it
        vulkan_memory_entry old_entry = space_entry->value;

        i64 aligned_size = target_size + memory_padding( args.alignment, old_entry.position );

        // Move the space to the next node
        space_entry->value = {
            .block = old_entry.block,
            .index = space_entry->value.index,
            // Move the position up by the allocated space
            .position = (old_entry.reserved_position + aligned_size),
            // Position before alignment
            .reserved_position = (old_entry.reserved_position + aligned_size),
            // Allocated size is still 0
            .size = 0,
            // NOTE: Internal used bytes after alignment and redzone
            // NOTE: Shrink the reserved space by the total used space
            .reserved_size = ( old_entry.reserved_size - aligned_size),
            .alignment = 1
        };
        // TODO: Don't know how to handle alignment here, this is an offset so what is "aligned"?
        // Is new Vulkan memory objects it just auto aligned to 64 bytes always?
        new_entry->value = {
            .block = old_entry.block,
            .index = new_entry->index,
            .position = memory_align( old_entry.position, args.alignment ),
            .reserved_position = old_entry.position,
            // The actual size allocated for the object
            .size = args.size,
            .reserved_size = aligned_size,
            .alignment = args.alignment
        };
        result.value = new_entry->value;
        result.error = false;
        // Exit the loop
        break;
    }
    if (result.error)
    {   VULKAN_ERROR( "Failed to suballocate untyped memory from device memory" );
    }
    return result;
}

PROC vulkan_memory_allocate_buffer( vulkan_memory* arg, vulkan_buffer* buffer ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    if (buffer == nullptr || buffer->id.valid() == false) { return false; }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements( g_vulkan->logical_device, buffer->buffer, &requirements );

    // Default to device local memory if not specified
    VkMemoryPropertyFlags memory_flags = (buffer->memory_flags ? buffer->memory_flags :
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    i32 best_index = vulkan_memory_find_best_type_index(
        requirements.memoryTypeBits, memory_flags ).copy_default(-1);
    vulkan_allocate_args allocate_args {
        .size = i64(requirements.size),
        .alignment = i64(requirements.alignment),
        .memory_type_index = best_index,
        .memory_flags = memory_flags,
        .transfer_buffer = buffer->transfer_buffer
    };
    ERROR_GUARD( best_index >= 0, "Failed to find suitible memory type" );

    monad<vulkan_memory_entry> entry_ok = vulkan_memory_allocate_untyped( arg, allocate_args );
    auto entry = entry_ok.value;
    if (entry_ok.error)
    {   VULKAN_ERROR( "Failed to create device memory entry" );
        return false;
    }
    vulkan_memory_block& target_block = arg->blocks[ entry.block ];

    // Lookup original memory entry and fill in the type
    vulkan_memory_entry& original_entry =
    target_block.entries.nodes[ entry.index ].value;
    // HACK: TODO: copy the local copy of the entry whilst fixing the other stuff
    original_entry = entry;
    // NOTE: We can just lookup the buffer type from the buffer list
    original_entry.type = e_vulkan_memory_object::buffer;
    buffer->memory = original_entry;

    vkBindBufferMemory(
        g_vulkan->logical_device,
        buffer->buffer,
        target_block.memory,
        entry.position
    );
    VULKAN_LOGF( "Bound buffer '{}' to memory block: {} position: {} size: {} reserved size: {}",
                 buffer->name, entry.block, entry.position, entry.size, entry.reserved_size );
    VULKAN_LOGF( "    Buffer Host Mappable: {} Memory Flags: {:b}",
                 target_block.host_mappable, target_block.memory_flags );
    return true;
}

PROC vulkan_memory_allocate_image( vulkan_memory* arg, vulkan_image* image ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements( g_vulkan->logical_device, image->platform_image, &requirements );

    i32 best_index = vulkan_memory_find_best_type_index(
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ).copy_default(-1);
    vulkan_allocate_args allocate_args {
        .size = i64(requirements.size),
        .alignment = i64(requirements.alignment),
        .memory_type_index = best_index,
        .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    monad<vulkan_memory_entry> entry_ok = vulkan_memory_allocate_untyped( arg, allocate_args );
    auto entry = entry_ok.value;
    if (entry_ok.error)
    {   VULKAN_ERROR( "Failed to create device memory entry" );
        return false;
    }
    vulkan_memory_block& target_block = arg->blocks[ entry.block ];

    // Lookup original memory entry and fill in the type
    vulkan_memory_entry& original_entry =
    target_block.entries.nodes[ entry.index ].value;
    // NOTE: We can just lookup the buffer type from the buffer list
    original_entry.type = e_vulkan_memory_object::image;
    image->memory = original_entry;

    // Bind it into the subregion in the device memory
    VkResult bind_bad = vkBindImageMemory(
        g_vulkan->logical_device,
        image->platform_image,
        target_block.memory,
        entry.position
    );

    return (bind_bad == VK_SUCCESS);
}

PROC vulkan_vertex_buffer_size( i64 vertexes ) -> i64
{
    return (sizeof(v3) * vertexes * 2);
}

PROC vulkan_mesh_init( mesh* arg ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    // Initialize the mesh if it hasn't been done so already but don't worry if it's already been done.
    bool init_ok = (arg->id.valid() || mesh_init( arg ));
    bool mesh_uninitialized = (init_ok == false);
    if(mesh_uninitialized)
    {   return false;
    }

    vulkan_mesh* vk_mesh = &g_vulkan->meshes.push_tail( {} );
    vk_mesh->id.uuid = arg->id.uuid;
    // TODO: What is the buffer type of this supposed to be????
    // vk_mesh->color_buffer = vulkan_buffer_create(
        // arg->name, arg->vertexes_n * sizeof(v3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    // );

    if (arg->vertexes_n)
    {   // Space require for both normals and vertexes in the same buffer,
        i64 total_size = (arg->vertexes_n * sizeof(v3) *2);
        // NOTE: We need TRANSFER_DST_BIT to allow for transfering memory to the GPU
        vk_mesh->vertex_buffer = vulkan_buffer_create(
             arg->name, total_size,
             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
         );
        vulkan_memory_allocate_buffer( &g_vulkan->device_memory, &vk_mesh->vertex_buffer );
    }
    if (arg->vertex_indexes_n)
    {   vk_mesh->vertex_indexes_buffer = vulkan_buffer_create(
            arg->name, arg->vertex_indexes_n * sizeof(i32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        );
        vulkan_memory_allocate_buffer( &g_vulkan->device_memory, &vk_mesh->vertex_indexes_buffer );
    }
    // Shares buffer with vertexies
    // if (arg->vertex_colors.size() > 0)
    // {   vulkan_memory_suballocate_buffer( g_vulkan->device_memory, vk_mesh->color_buffer );
    // }


    /** Transform buffers into compatible format
        NOTE: We're now copying directly into the mapped range and skipping intermediaries. */
    if ( vk_mesh->vertex_buffer.buffer == VK_NULL_HANDLE)
    {   return false;
    }
    /** NOTE: We're doing transfer operations instead of direct memory maps now
        NOTE: This is not a Vulkan operation, the actual operation is a map + a cmdCopyBufferXXX */
    i64 vertex_buffer_size = vulkan_vertex_buffer_size( arg->vertexes_n );
    auto queue_bad = vulkan_transfer_queue_buffer(
        &g_vulkan->transfer, &vk_mesh->vertex_buffer, vertex_buffer_size, 0 );
    ERROR_GUARD( (! queue_bad.error), "Can't really draw anything if this happens" );
    if (queue_bad.error)
    {   VULKAN_ERROR( "Failed to queue a mesh transfer to the GPU" );
        return false;
    }
    raw_pointer data = queue_bad.value.data;
    v3* vertex_readhead = arg->vertexes.data;
    i64 vertex_offset = (sizeof(v3));
    i64 vertex_stride = (sizeof(v3) * 2);
    raw_pointer writehead = data;
    for (int i_vertex = 0; i_vertex < arg->vertexes_n; ++i_vertex)
    {
        // TODO: Fill in normals
        writehead = data + (vertex_stride * i_vertex);
        // copy it into the current triangle position
        vertex_readhead = arg->vertexes.address( i_vertex );
        // memory_copy<v3>( writehead + 0, normal_readhead, 1 );
        memory_copy<v3>( writehead + vertex_offset, vertex_readhead, 1 );
    }

    VULKAN_LOGF( "Initialized vulkan_mesh Name: {}    UUID: {}", arg->name, arg->id );
    return true;
}

PROC vulkan_image_init( render_image* arg ) -> fresult
{
    VkImageCreateInfo image_args {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        /** RGBA Follows endianness. This is allowed for the transfer image but not the swapchain image

        NOTE: Seriously? The driver just ignores this format and makes you swizzle the
        channels manually anyway, so we have to use BGRA format for blitting */
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .extent = VkExtent3D { u32(arg->image.size.x), u32(arg->image.size.y), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        /* NOTE: We use our images as both transfer source and destination */
        .usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vulkan_image* image = &g_vulkan->images.push_tail({});
    image->associated_image = arg->id;
    image->layout = image_args.initialLayout;
    VkResult image_bad = vkCreateImage(
        g_vulkan->logical_device, &image_args, g_vulkan->vk_allocator, &image->platform_image );
    if (image_bad)
    {
        VULKAN_ERROR( "Failed to create image for drawing: {}",string_VkResult(image_bad) );
        return false;
    }

    vulkan_label_object( u64(image->platform_image), VK_OBJECT_TYPE_IMAGE, "image_" + arg->name );

    bool suballocate_ok = vulkan_memory_allocate_image( &g_vulkan->device_memory, image );
    // Create a host mappable staging/transfer buffer (try to use faster(*) BAR memory
    // * not sur if this is actualy faster or not
    image->size = { f32(arg->image.size.x), f32(arg->image.size.y) };
    image->staging_buffer = vulkan_buffer_create(
        "image_transfer", arg->image.size_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT );
    image->staging_buffer.memory_flags =
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    vulkan_memory_allocate_buffer( &g_vulkan->device_memory, &image->staging_buffer );

    image->id = uuid_generate();
    VULKAN_LOGF( "Intialized render image '{}' with id {}", arg->name, arg->id );
    return suballocate_ok;
}

PROC vulkan_frame_init( vulkan_frame* arg, vulkan_pipeline* pipeline ) -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    VkDescriptorSetAllocateInfo descriptor_resource_args {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pipeline->vk_resource_pool,
        // Only allocating 1 frame at a time
        .descriptorSetCount = 1,
        .pSetLayouts = &pipeline->platform_descriptor_layout
    };
    vkAllocateDescriptorSets(
        g_vulkan->logical_device, &descriptor_resource_args, &arg->vk_resource );

    arg->general_uniform_buffer = vulkan_buffer_create(
        "frame_general_uniform",
        sizeof( frame_general_uniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );
    fresult allocate_ok = vulkan_memory_allocate_buffer(
        &g_vulkan->device_memory, &arg->general_uniform_buffer );
    if (allocate_ok == false)
    {   return false;
    }

    vulkan_memory_block& uniform_block = vulkan_memory_get_block(
        &g_vulkan->device_memory, &arg->general_uniform_buffer.memory );
    /** WARNING: Please don't try to unmap memory suballocated from a buffer it
        will immediately invalidate every buffer associated with the device
        memory object. */
    return true;
}

PROC vulkan_init_pipelines() -> void
{
    PROFILE_SCOPE_FUNCTION();
    // Create shaders of pipeline
    {
        vulkan_shader vertex_shader {};
        vulkan_shader fragment_shader {};

        vertex_shader.name = "test_triangle";
        vertex_shader.code.filename = "data/shaders/test_utah_teapot.vert.spv";
        vertex_shader.code_binary = true;
        vertex_shader.stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
        fragment_shader.name = "test_triangle";
        fragment_shader.code.filename = "data/shaders/test_utah_teapot.frag.spv";
        fragment_shader.code_binary = true;
        fragment_shader.stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;

        vulkan_shader_init( &vertex_shader );
        vulkan_shader_init( &fragment_shader );

        vulkan_pipeline& pipeline = g_vulkan->mesh_pipeline;
        pipeline.shaders.push_tail( vertex_shader );
        pipeline.shaders.push_tail( fragment_shader );
        vulkan_pipeline_mesh_init( &g_vulkan->mesh_pipeline );
    }
    {
        vulkan_shader vertex_shader {};
        vulkan_shader fragment_shader {};

        vertex_shader.name = "ui_mesh_vertex";
        vertex_shader.code.filename = "data/shaders/ui_mesh_shader.vert.spv";
        vertex_shader.code_binary = true;
        vertex_shader.stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
        fragment_shader.name = "ui_mesh_fragment";
        fragment_shader.code.filename = "data/shaders/ui_mesh_shader.frag.spv";
        fragment_shader.code_binary = true;
        fragment_shader.stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;

        vulkan_shader_init( &vertex_shader );
        vulkan_shader_init( &fragment_shader );

        vulkan_pipeline& pipeline = g_vulkan->ui_mesh_pipeline;
        pipeline.shaders.push_tail( vertex_shader );
        pipeline.shaders.push_tail( fragment_shader );
        vulkan_pipeline_mesh_init( &g_vulkan->ui_mesh_pipeline );
    }
}

PROC vulkan_transfer_init( vulkan_transfer_context* arg ) -> fresult
{
    return true;
}

PROC vulkan_transfer_find_suitible_buffer( vulkan_transfer_context* context, i64 size )
-> search_result<vulkan_transfer_buffer>
{
    search_result<vulkan_transfer_buffer> result;
    search_result<vulkan_transfer_buffer> search = context->buffers.linear_search(
        [size]( vulkan_transfer_buffer&     arg ) {
            i64 bytes_left = (arg.size - arg.head_size);
            return (bytes_left > size);
        });
    // Copy search result to return even if it's nullptr
    result = search;

    if (context->new_buffer_fail_reset_timer.triggered()) { context->new_buffer_fail_flag = false; }
    if ((! search.match_found) && (! context->new_buffer_fail_flag))
    {   // No suitible transfer buffer, we'll try to make a new one
        vulkan_buffer new_buffer = vulkan_buffer_create(
            "buffer_transfer", context->staging_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        );
        // NOTE: Typical flags for BAR accessible staging buffer, not relevant for iGPUs.

        new_buffer.memory_flags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        new_buffer.transfer_buffer = true;
        fresult allocate_ok = vulkan_memory_allocate_buffer( &g_vulkan->device_memory, &new_buffer );

        if (new_buffer.id.valid() == false || allocate_ok == false)
        {   VULKAN_ERRORF( "Failed to create transfer buffer with size: {:>10}",
                           context->staging_buffer_size );
            context->new_buffer_fail_flag = false;
            result.match_found = false;
            return result;
        }

        // Must have suceeded, we can push a new buffer
        vulkan_transfer_buffer* new_transfer = &context->buffers.push_tail({});
        new_transfer->buffer = new_buffer.id;
        new_transfer->size = new_buffer.size;
        new_transfer->head_size = 0;

        /* NOTE: By the time we've reached here we've almost certainly got a new
           buffer so we can leave set the monad to false */
        result.match = new_transfer;
        result.match_found = true;
        result.index = context->buffers.tail_index();

        // Map the entire buffer for use
        void* map_data = nullptr;
        VkMemoryMapFlags map_flags = 0x0;
        vulkan_memory_block& block =  vulkan_memory_get_block(
            &g_vulkan->device_memory, &new_buffer.memory );
        VkResult map_bad = vkMapMemory(
            g_vulkan->logical_device,
            block.memory,
            new_buffer.memory.position,
            VK_WHOLE_SIZE,
            map_flags,
            &map_data
        );

        // Copy the map pointer and mark if we mapped successfully
        new_transfer->mapped_data = map_data;
        new_transfer->mapped = (map_bad == VK_SUCCESS);
        ERROR_GUARD( new_transfer->mapped, "Can't continue if we have a map failure" );
    }
    return result;
}

PROC vulkan_transfer_queue_buffer(
    vulkan_transfer_context* context,
    vulkan_buffer* buffer,
    i64 size,
    i64 buffer_offset
) -> monad< dynamic_span<void> >
{
    monad< dynamic_span<void> > result;
    auto transfer_search = vulkan_transfer_find_suitible_buffer( context, size );
    vulkan_transfer_buffer* transfer_buffer = transfer_search.match;
    if (transfer_buffer == nullptr)
    {   result.error = true;
        return result;
    }

    vulkan_transfer* new_transfer = &context->transfer_queue.push_tail({
            .buffer_index = transfer_search.index,
            .position = transfer_buffer->head_size,
            .size = size,
            .buffer_offset = buffer_offset,
            .destination = e_vulkan_memory_object::buffer,
            .destination_buffer = buffer->buffer,
            .destination_image_ = 0
    });
    // Increase transfer buffer used size by size of the buffer
    // TODO: Does this need to be an aligned transfer?
    transfer_buffer->head_size += new_transfer->size + context->redzone_bytes;
    // VULKAN_LOG( "Queued Buffer GPU trasnfer" )
    // VULKAN_LOGF( "transfer_buffer_id: {} position: {} size: {} destination offset: {}",
    //              new_transfer->buffer_index, new_transfer->position,
    //              new_transfer->size, new_transfer->buffer_offset );

    // Return the pointer to the tranfer's location in the mapped buffer
    result.value.data = transfer_buffer->mapped_data + new_transfer->position;
    result.value.size = new_transfer->size;
    result.error = (transfer_buffer->mapped != true);
    return result;
}

PROC vulkan_transfer_queue_image(
    vulkan_transfer_context* context,
    vulkan_image* image,
    i64 size,
    i64 buffer_offset
) -> monad< dynamic_span<void> >
{
    monad< dynamic_span<void> > result;
    auto transfer_search = vulkan_transfer_find_suitible_buffer( context, size );
    vulkan_transfer_buffer* transfer_buffer = transfer_search.match;
    if (transfer_buffer == nullptr)
    {   result.error = true;
        return result;
    }

    vulkan_transfer* new_transfer = &context->transfer_queue.push_tail({
            .buffer_index = transfer_search.index,
            .position = transfer_buffer->head_size,
            .size = size,
            .buffer_offset = buffer_offset,
            .destination = e_vulkan_memory_object::image,
            .destination_buffer = VK_NULL_HANDLE,
            .destination_image_ = image->id
    });
    // Increase transfer buffer used size by size of the buffer
    // TODO: Does this need to be an aligned transfer?
    transfer_buffer->head_size += new_transfer->size + context->redzone_bytes;
    // VULKAN_LOG( "Queued Image GPU transfer" )
    // VULKAN_LOGF( "transfer_buffer_id: {} position: {} size: {} destination offset: {}",
    //              new_transfer->buffer_index, new_transfer->position,
    //              new_transfer->size, new_transfer->buffer_offset );

    // Return the pointer to the tranfer's location in the mapped buffer
    result.value.data = transfer_buffer->mapped_data + new_transfer->position;
    result.value.size = new_transfer->size;
    result.error = (transfer_buffer->mapped != true);
    return result;
}

PROC vulkan_image_prepare( render_image* arg ) -> fresult
{
    render_image* current_image = arg;

    // Find the associated vulkan image
    auto image_result = g_vulkan->images.linear_search( [=]( vulkan_image& arg_ ) {
        return (arg_.associated_image == current_image->id) && arg_.id.valid(); } );
    vulkan_image* vk_draw_image = image_result.match;

    bool no_vulkan_image = (current_image->id.valid() && image_result.match_found == false);
    if (no_vulkan_image)
    {
        // Recreate and search again
        vulkan_image_init( current_image );
        image_result = g_vulkan->images.linear_search( [current_image]( vulkan_image& arg ) {
            return (arg.associated_image == current_image->id) && arg.id.valid(); } );
        vk_draw_image = image_result.match;
        /* TODO: This could cause run away resource usage if we have a bad/corrupted vulkan_image list/id
           need to investigate mitigations for this */
    }

    if (vk_draw_image)
    {
        // Update image if it's  dirty
        bool dirty_buffer = (current_image->write_timestamp > vk_draw_image->update_timestamp);
        bool update_image = dirty_buffer;
        // update_image = true; // DEBUG: Force update every time
        if (update_image)
        {
            auto queue_bad = vulkan_transfer_queue_image(
                &g_vulkan->transfer, vk_draw_image, current_image->image.size_bytes(), 0 );
            dynamic_span<void> image_stage = queue_bad.value;
            if (! queue_bad.error)
            {
                // Copy limited to span size
                memory_copy_raw(
                    image_stage.data, current_image->image.data, current_image->image.size_bytes() );

                // Need to copy into GPUs preferred BGRA format. We'll just borrow the staging memory.
                image<rgba> gpu_image = current_image->image;
                gpu_image.data = raw_pointer(image_stage.data);
                (void)image_color_reorder_inplace<bgra>( gpu_image );
                vk_draw_image->update_timestamp = time_now_ns();
            }
        }
    }
    return true;
}

PROC vulkan_init() -> fresult
{
    PROFILE_SCOPE_FUNCTION();
    fresult result = false;
    g_vulkan = memory_allocate<vulkan_context>( 1 );

    g_vulkan->allocator = &g_vulkan->default_allocator;

    auto self = g_vulkan;
    auto& instance = g_vulkan->instance;
    VkInstanceCreateInfo instance_args = {};
    VkApplicationInfo app_info = {};
    i32& graphics_queue_family = self->graphics_queue_family;
    i32& present_queue_family = self->present_queue_family;

    g_vulkan->allocator_callback = vulkan_allocator_create_callbacks(
        g_vulkan->allocator );

    // Use command line argument to see if to disable custom allocator
    auto allocator_arg = g_library->cmdline_arguments.linear_search(
        []( cmdline_argument& arg ) {
            return arg.name == "vulkan_disable_custom_allocator"; });
    if (allocator_arg.match_found == false)
    {   g_vulkan->vk_allocator = &g_vulkan->allocator_callback; }

    defer_procedure _exit = [&result] {
        if (result)
        { VULKAN_LOG( "Vulkan Initialized" ); }
        else
        {
            VULKAN_ERROR( "Vulkan initialization failure, cleaning up resources." );
            g_vulkan->resources.~resource_arena();
        }
    };

    // Enumerate intance layers
    u32 n_layers = 0;
    vkEnumerateInstanceLayerProperties( &n_layers, nullptr );

    array<VkLayerProperties> layers;
    layers.resize( n_layers );
    VkResult enumerate_layer_ok = vkEnumerateInstanceLayerProperties( &n_layers, layers.data );

    u32 available_extensions_n = 0;
    array<VkExtensionProperties> available_extensions;
    vkEnumerateInstanceExtensionProperties( nullptr, &available_extensions_n, nullptr );
    available_extensions.resize( available_extensions_n );
    vkEnumerateInstanceExtensionProperties(
        nullptr, &available_extensions_n, available_extensions.data );


    VULKAN_LOG( "Enumerating Available Instance Layers:" );
    // TODO: Enumerate layer-specific extensions here
    layers.map_procedure( []( VkLayerProperties& arg ) {
        VULKAN_LOGF( "    {} {}.{}.{}",
                     arg.layerName,
                     VK_API_VERSION_MAJOR( arg.specVersion ),
                     VK_API_VERSION_MINOR( arg.specVersion ),
                     VK_API_VERSION_PATCH( arg.specVersion ),
                     arg.description
        );
    });
    VULKAN_LOG( "" );
    VULKAN_LOG( "Enumerating Available Instance Extensions:" );
    available_extensions.map_procedure( []( VkExtensionProperties& arg ) {
        VULKAN_LOGF( "    {} {}.{}.{}",
                     arg.extensionName,
                     VK_API_VERSION_MAJOR( arg.specVersion ),
                     VK_API_VERSION_MINOR( arg.specVersion ),
                     VK_API_VERSION_PATCH( arg.specVersion )
        );
    });
    VULKAN_LOG( "" );

    // -- Setup extensions and layers --
    array<cstring> enabled_layers = {
        "VK_LAYER_KHRONOS_validation", // debug validaiton layer
    };
    array<cstring> enabled_extensions = {
        // Surface for common window and compositing tasks
        VK_KHR_SURFACE_EXTENSION_NAME,

        // message callback and debug stuff
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        // Dependency for 'vkDebugMarkerSetObjectNameEXT'
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        // Promoted to debug_utils, might still required for older versions, but not modern nvidia
        // VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
    };
    /* NOTE: The instance will refuse to load if it doesn't support the enabled extensions
       So we need to make extra sure it's actually supported before we make enable the extension */
    if (g_render->window_platform == e_window_platform::x11)
    {   // xlib windowing extension
        enabled_extensions.push_tail( "VK_KHR_xlib_surface" );
    }
    else if (g_render->window_platform == e_window_platform::wayland)
    {   // Wayland Windowing
        enabled_extensions.push_tail( "VK_KHR_wayland_surface" );
    }
    else if (g_render->window_platform == e_window_platform::windows)
    {   // win32 Windowing
        enabled_extensions.push_tail( "VK_KHR_win32_surface" );
    }

    app_info.pApplicationName = "Tachyon Engine";
    app_info.applicationVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
    app_info.pEngineName = "Tachyon Engine";
    app_info.engineVersion = VK_MAKE_API_VERSION( 0, 0, 1, 0 );
    app_info.apiVersion = VK_API_VERSION_1_2;

    VULKAN_LOG( "Enabling Vulkan Layers:" );
    enabled_layers.map_procedure( []( cstring arg ) {
        VULKAN_LOGF( "    {}", arg );
    });
    VULKAN_LOG( "" );
    VULKAN_LOG( "Enabling Vulkan Instance Extensions:" );
    enabled_extensions.map_procedure( []( cstring arg ) {
        VULKAN_LOGF( "    {}", arg );
    });
    VULKAN_LOG( "" );

    VkDebugUtilsMessengerCreateInfoEXT messenger_args {};
    messenger_args.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_args.messageSeverity = (
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    );
    messenger_args.messageType = (
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
    );
    messenger_args.pfnUserCallback = vulkan_debug_callback;

    instance_args.pApplicationInfo = &app_info;
    instance_args.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_args.enabledLayerCount = enabled_layers.size();
    instance_args.ppEnabledLayerNames = enabled_layers.data;
    instance_args.enabledExtensionCount = enabled_extensions.size();
    instance_args.ppEnabledExtensionNames = enabled_extensions.data;


    VkResult instance_ok = vkCreateInstance(
        &instance_args, g_vulkan->vk_allocator, &g_vulkan->instance );
    if (instance_ok != VK_SUCCESS)
    {
        VULKAN_ERROR( "Failed to create Vulkan instance", string_VkResult( instance_ok ) );
        return false;
    }
    VULKAN_LOG( "Created Vulkan instance" );
    g_vulkan->resources.push_cleanup( []{
        VULKAN_LOG( "Destroying Vulkan instance" );
        vkDestroyInstance( g_vulkan->instance, g_vulkan->vk_allocator );
        g_vulkan->instance = VK_NULL_HANDLE;
    });

    // need to do this to get validation layer callbacks aparently?
    auto dyn_vkCreateDebugUtilsMessengerEXT = PFN_vkCreateDebugUtilsMessengerEXT(
        vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" ) );
    dyn::vkDestroyDebugUtilsMessengerEXT = PFN_vkDestroyDebugUtilsMessengerEXT(
        vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" ) );

    VkDebugUtilsMessengerEXT debug_messenger {};
    dyn_vkCreateDebugUtilsMessengerEXT(
        instance, &messenger_args, g_vulkan->vk_allocator, &debug_messenger );
    if (debug_messenger)
    {   g_vulkan->resources.push_cleanup( [debug_messenger]
        {   dyn::vkDestroyDebugUtilsMessengerEXT(
                g_vulkan->instance, debug_messenger, g_vulkan->vk_allocator );
        } );
    }

    // For debug naming
    dyn::vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
    vkGetInstanceProcAddr( g_vulkan->instance, "vkSetDebugUtilsObjectNameEXT" );

    // -- Setup default window surfaces --
    if (g_sdl)
    {
        auto sdl = tyon::sdl_create_platform_subsystem();
        bool surface_ok = sdl.vulkan_surface_create(
            g_sdl->main_window,
            g_vulkan->instance,
            g_vulkan->vk_allocator,
            &g_vulkan->surface
        );
        if (surface_ok)
        {   g_vulkan->resources.push_cleanup( [] {
                vkDestroySurfaceKHR(
                    g_vulkan->instance, g_vulkan->surface, g_vulkan->vk_allocator );
            });
        }
    }
    else
    { VULKAN_LOG( "Vulkan surface could not be created for platform SDL" ); }

    // -- Device Enumeration and Selection --
    array<VkPhysicalDevice> devices;
    u32 n_devices = 0;
    vkEnumeratePhysicalDevices( instance, &n_devices, nullptr );
    devices.resize( n_devices );
    vkEnumeratePhysicalDevices( instance, &n_devices, devices.data );

    if (n_devices <= 0)
    {
        VULKAN_ERROR( "No physical devices found. Bailing Vulkan initialization" );
        return false;
    }
    for (i32 i=0; i < devices.size(); ++i)
    {
        VkPhysicalDevice x_device = devices[i];
        VkPhysicalDeviceProperties props;
        i32 x_graphics_family = -1;
        i32 x_present_family = -1;
        bool suitible = false;
        bool dedicated_graphics = false;
        vkGetPhysicalDeviceProperties( x_device, &props );
        VULKAN_LOGF( "Enumerated physical device: {} | {:x}:{:x}",
                     props.deviceName, props.vendorID, props.deviceID );
        /* The driver version is literally trash. It's a vendor specific bitmask
         * so you could never hope to get it a coherent number without a complex
         * codepath figuring out which bitmask to use for each card and also
         * somehow finding all the different vendor bitmasks in history. */
        VULKAN_LOGF(
            "    Vulkan API version: {}.{}.{}", VK_VERSION_MAJOR(props.apiVersion),
            VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion) );
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            dedicated_graphics = true;
            VULKAN_LOG( "    Device Type: Discrete GPU");
        }

            // Enumerate Device Extensions
        // TODO: Search through instance layers
        array<cstring> layer_names;
        array<VkExtensionProperties> device_extensions;
        u32 extensions_n = 0;
        vkEnumerateDeviceExtensionProperties(
            x_device, nullptr, &extensions_n, nullptr );
        device_extensions.resize( extensions_n );
        vkEnumerateDeviceExtensionProperties(
            x_device, nullptr, &extensions_n, device_extensions.data );
        VULKAN_LOG( "Enumerating Device Extensions: " );
        bool swapchain_khr_support = false;
        device_extensions.map_procedure( [&swapchain_khr_support] (VkExtensionProperties& arg) {
            if (arg.extensionName == "VK_KHR_swapchain"s)
            {   swapchain_khr_support = true;
            }
            VULKAN_LOGF( "    {} {}.{}.{}",
                         arg.extensionName,
                         VK_API_VERSION_MAJOR( arg.specVersion ),
                         VK_API_VERSION_MINOR( arg.specVersion ),
                         VK_API_VERSION_PATCH( arg.specVersion ))});
        VULKAN_LOG( "" );
        VULKAN_LOGF( "VK_KHR_swapchain support: {}", swapchain_khr_support );

        array<VkQueueFamilyProperties> families;
        u32 n_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( x_device, &n_families, nullptr );
        families.resize( n_families );
        vkGetPhysicalDeviceQueueFamilyProperties( x_device, &n_families, families.data );
        VULKAN_LOG( "Queue Family Count: ", n_families );

        /* Seriously... Why. you have to reference the queue family by an
         * arbitrary index. you get whilst looping through */
        bool graphics_queue_unfulfilled = true;
        bool present_queue_unfulfilled = true;
        for (int i_queue=0; i_queue < families.size(); ++i_queue)
        {
            VkBool32 present_support = false;
            bool graphics_support = (families[ i_queue ].queueFlags & VK_QUEUE_GRAPHICS_BIT);
            vkGetPhysicalDeviceSurfaceSupportKHR(
                x_device, i_queue, g_vulkan->surface, &present_support );
            VULKAN_LOGF( "    Queue Family Index {}", i_queue );
            VULKAN_LOGF( "        Graphics Support: {}", graphics_support );
            VULKAN_LOGF( "        Presentation Support: {}", bool(present_support) );

            /* HACK TODO: Hardcoded to select graphics first because 3080 is
               setup to have graphics queue first But we can't actually assume
               that so this needs to be fixed */
            bool both_queues_unfulfilled = (graphics_queue_unfulfilled && present_queue_unfulfilled);
            if (both_queues_unfulfilled && graphics_support && present_support)
            {   x_graphics_family = i_queue;
                x_present_family = i_queue;
            }
            else if (graphics_support && graphics_queue_unfulfilled)
            {   x_graphics_family = i_queue;
            }
            else if (present_support && present_queue_unfulfilled)
            {   x_present_family = i_queue;
            }
            // TODO: Check does present family also need graphics family??
            // Do this at the end in case it's the last family it will not be run
            graphics_queue_unfulfilled = (x_graphics_family < 0);
            present_queue_unfulfilled = (x_present_family < 0);
        }
        bool suitible_queues = (x_graphics_family >= 0 && x_present_family >= 0);
        if (suitible_queues)
        {   suitible = true;
            VULKAN_LOG("        Device has a suitible graphics and present queue family." );

        }
        else
        {   VULKAN_LOG("        Failed to find suitible graphics or present queue family for device" );
        }

        // Use llvmpipe exclusively if selected, otherwise prefer dedicated
        // graphics, otherwise use anything
        fstring device_name = props.deviceName;
        // std::for_each( device_name.begin(), device_name.end(), [](char x) { return std::tolower(x); } );
        std::ranges::for_each( device_name, [](char x) { return std::tolower(x); } );
        bool detected_llvmpipe = (device_name.find( "llvmpipe") != std::string::npos);
        bool prefer_dedicated = (global->graphics_llvmpipe == false && dedicated_graphics);
        bool prefer_llvm = (global->graphics_llvmpipe == true && detected_llvmpipe);
        bool more_preferred_device = (prefer_llvm || prefer_dedicated);
        bool select_device = (
            suitible && (more_preferred_device || g_vulkan->device == VK_NULL_HANDLE));

        if (global->graphics_llvmpipe)
        {   VULKAN_LOG( "llvmpipe requested on the command line, "
                        "lavapipe for Vulkan is available. Trying to use lavapipe." );
        }
        if (select_device)
        {
            g_vulkan->device = x_device;
            g_vulkan->graphics_queue_family = x_graphics_family;
            g_vulkan->present_queue_family = x_present_family;
            VULKAN_LOGF( "Selected primary graphics device: '{}'", props.deviceName );
            VULKAN_LOGF( "    Graphics Queue: {} Present Queue: {}",
                         graphics_queue_family, present_queue_family );
        }
        VULKAN_LOG( "" );
    }
    if (g_vulkan->device == VK_NULL_HANDLE)
    {   VULKAN_ERROR( "Could not find a suitible graphics device for some reason."
                      "Vulkan initialization failed. " );
        return false;
    }

    // -- Create Device and Device Queue --

    // Create some 'queue arg' structs and then pass it to the 'logical device' creation struct
    VkQueue& graphics_queue = g_vulkan->graphics_queue;
    VkQueue& present_queue = g_vulkan->present_queue;
    f32 queue_priority = 1.0f;
    array<VkDeviceQueueCreateInfo> queues;

    VkDeviceQueueCreateInfo graphics_queue_args{};
    graphics_queue_args.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_args.queueFamilyIndex = graphics_queue_family;
    graphics_queue_args.queueCount = 1;
    graphics_queue_args.pQueuePriorities = &queue_priority;
    queues.push_tail( graphics_queue_args );

    VkDeviceQueueCreateInfo present_queue_args{};
    present_queue_args.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_queue_args.queueFamilyIndex = present_queue_family;
    present_queue_args.queueCount = 1;
    present_queue_args.pQueuePriorities = &queue_priority;
    // NOTE: I don't think we need this all the time if we can use the same queue family twice
    // queues.push_tail( present_queue_args );

    VkPhysicalDeviceFeatures device_features {
        // for multisampling support
        .sampleRateShading = true,
        // .logicOp = true
    };

    // Query features supported by the device
    VkPhysicalDeviceFeatures supported_features {};
    vkGetPhysicalDeviceFeatures( g_vulkan->device, &supported_features );

    // Setup the final logical device args struct
    VkDeviceCreateInfo device_args = {};
    device_args.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_args.pQueueCreateInfos = queues.data;
    device_args.queueCreateInfoCount = queues.size();
    device_args.pEnabledFeatures = &device_features;

    array<cstring> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, // Presentation swapchain extension
        // Interferes with debugging
        // VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, // Multiple pixels per fragment
    };
    device_args.ppEnabledLayerNames = enabled_layers.data;
    device_args.enabledLayerCount = enabled_layers.size();
    device_args.ppEnabledExtensionNames = device_extensions.data;
    device_args.enabledExtensionCount = device_extensions.size();

    VULKAN_LOG( "Enabling Vulkan Instance Extensions:" );
    device_extensions.map_procedure( []( cstring arg ) {
        VULKAN_LOGF( "    {}", arg );
    });
    VULKAN_LOG( "" );

    // Actually create logical device
    auto device_ok = vkCreateDevice(
        g_vulkan->device,
        &device_args,
        g_vulkan->vk_allocator,
        &g_vulkan->logical_device
    );
    if (device_ok) { TYON_ERROR( "Device creation error" ); return false; }

    g_vulkan->resources.push_cleanup( []{
        VULKAN_LOGF( "Destroy Logical Device 0x{:x}", u64(g_vulkan->logical_device) );
        vkDestroyDevice( g_vulkan->logical_device, g_vulkan->vk_allocator );
        g_vulkan->logical_device =  VK_NULL_HANDLE;
    } );
    // NOTE: the first graphics queue family often supports present as well
    // So we can make a present queue from the same family.
    vkGetDeviceQueue( g_vulkan->logical_device, graphics_queue_family, 0, &graphics_queue );
    vkGetDeviceQueue( g_vulkan->logical_device, graphics_queue_family, 0, &present_queue );

    // Create Threading Primitives
    VkFenceCreateInfo fence_args {};
    fence_args.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    bool fence_ok = true;
    fence_ok &= VK_SUCCESS == vkCreateFence(
        g_vulkan->logical_device, &fence_args, g_vulkan->vk_allocator, &g_vulkan->frame_acquire_fence );
    // Signal the begin fence to prevent it hanging
    fence_args.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    fence_ok &= VK_SUCCESS == vkCreateFence(
        g_vulkan->logical_device, &fence_args, g_vulkan->vk_allocator, &g_vulkan->frame_begin_fence );
    if (fence_ok == false) { return false; }
    vulkan_label_object(
        (u64)self->frame_begin_fence, VK_OBJECT_TYPE_FENCE, "frame_begin_fence" );
    vulkan_label_object(
        (u64)self->frame_acquire_fence, VK_OBJECT_TYPE_FENCE, "frame_acquire_fence" );

    VkSemaphoreCreateInfo semaphore_args {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    bool semaphore_ok = true;
    semaphore_ok &= VK_SUCCESS == vkCreateSemaphore(
        self->logical_device, &semaphore_args, g_vulkan->vk_allocator,
        &self->frame_end_semaphore );
    semaphore_ok &= VK_SUCCESS == vkCreateSemaphore(
        self->logical_device, &semaphore_args, g_vulkan->vk_allocator,
        &self->queue_submit_semaphore );
    vulkan_label_object(
        (u64)self->frame_end_semaphore, VK_OBJECT_TYPE_SEMAPHORE, "frame_end_semaphore" );
    vulkan_label_object(
        (u64)self->queue_submit_semaphore, VK_OBJECT_TYPE_SEMAPHORE, "queue_submit_semaphore" );

    VkCommandPool& command_pool = g_vulkan->command_pool;
    VkCommandPoolCreateInfo pool_args {};
    pool_args.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_args.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_args.queueFamilyIndex = graphics_queue_family;
    VkResult pool_ok = vkCreateCommandPool(
        g_vulkan->logical_device, &pool_args, g_vulkan->vk_allocator, &command_pool );
    if (pool_ok)
    {   VULKAN_ERROR( "Failed to create command pool" );
        return false;
    }
    g_vulkan->resources.push_cleanup( [] {
        VULKAN_LOG( "Destroying command pool" );
        vkDestroyCommandPool(
            g_vulkan->logical_device, g_vulkan->command_pool, g_vulkan->vk_allocator );
    } );

    // Create a command buffer
    VkCommandBufferAllocateInfo command_args{};
    command_args.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_args.commandPool = command_pool;
    command_args.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_args.commandBufferCount = self->frames_inflight_count;

    self->commands.resize( self->frames_inflight_count );
    VkResult command_buffer_bad = vkAllocateCommandBuffers(
        g_vulkan->logical_device, &command_args, g_vulkan->commands.data );
    if (command_buffer_bad)
    {
        VULKAN_ERROR( "Failed to allocate command buffers" );
        return false;
    }
    g_vulkan->resources.push_cleanup( [] {
        vkFreeCommandBuffers(
            g_vulkan->logical_device, g_vulkan->command_pool,
            g_vulkan->frames_inflight_count, g_vulkan->commands.data
        );
    } );

    // Initialize test mesh data
    g_vulkan->test_triangle = mesh {
        .name = "test_triangle",
        // Opengl Coordinates
        // .vertexes = {{-0.5f, -0.4330127019f, 0.0f},
        //              {0.5f, -0.4330127019f, 0.0f },
        //              {0.0f,  0.4330127019f, 0.0f }},
        // Z up Unreal coordinates
        .vertexes = {{-0.5f, 0.0f, -0.4330127019f },
                     {0.5f, 0.0f,  -0.4330127019f },
                     {0.0f,  0.0f, 0.4330127019f  }},

        .vertex_colors = {{ 0.f, 0.f, 1.f, 0.f },
                          { 0.f, 1.f, 0.f, 0.f },
                          { 1.f, 0.f, 0.f, 0.f }}
    };

    g_vulkan->test_ui_triangle = g_vulkan->test_triangle;
    g_vulkan->test_ui_triangle.name = "test_ui_triangle";
    g_vulkan->test_ui_triangle.transform.rotation.z = 0.25 * 6.283185;

    g_vulkan->test_ui_square = mesh {
        .name = "test_ui_square",
        .vertexes = geometry_rectangle( {1.0f, 1.0f } )
    };
    mesh* test_ui_circle = memory_allocate<mesh>(1);
    *test_ui_circle = mesh {
        .name = "test_ui_circle",
        .vertexes = geometry_circle( 500/2, 16 ),
    };

    // file teapot_file = file_load_binary( "data/geometry/utah_teapot.stl" );
    // file whale_file = file_load_binary( "data/geometry/articulated_whale_shark.stl" );
    // fmesh teapot = read_stl_file( teapot_file.filename );
    // fmesh whale = read_stl_file( whale_file.filename );
    fmesh teapot;
    fmesh whale;
    g_vulkan->test_teapot = {
        .name = "test_utah_teapot",
        .vertexes {},
        .vertex_normals {},
        .faces_n = i32(teapot.face_count),
        .vertexes_n = i32(teapot.vertex_count),
        .vertex_indexes_n = i32(teapot.index_count)
    };

    g_vulkan->test_whale = {
        .name = "test_articulated_whale",
        .vertexes {},
        .vertex_normals {},
        .faces_n = i32(whale.face_count),
        .vertexes_n = i32(whale.vertex_count),
        .vertex_indexes_n = i32(whale.index_count)
    };

    {
        mesh& teapot_ = g_vulkan->test_teapot;
        mesh& whale_ = g_vulkan->test_whale;
        teapot_.vertexes.resize( teapot.vertex_count );
        teapot_.vertex_indexes.resize( teapot.vertex_count );
        whale_.vertexes.resize( whale.vertex_count );
        whale_.vertex_indexes.resize( whale.vertex_count );
        for (int i_vertex=0; i_vertex < teapot.vertex_count; ++i_vertex)
        {   teapot_.vertexes[ i_vertex ] = teapot.vertex_buffer[ i_vertex *2 +1 ];
            teapot_.vertex_normals[ i_vertex ] = teapot.vertex_buffer[ i_vertex *2 ];
        }
        for (int i_vertex=0; i_vertex < whale.vertex_count; ++i_vertex)
        {   whale_.vertexes[ i_vertex ] = whale.vertex_buffer[ i_vertex *2 +1 ];
            whale_.vertex_normals[ i_vertex ] = whale.vertex_buffer[ i_vertex *2 ];
        }
    }

    /* NOTE: I  previous tried  to use  HOST_COHERENT /  HOST_VISIBLE memory
     * here but it's  not actually very well supported,  especially in older
     * Vulkan  versions. For  supported  Vulkan versions  unified memory  is
     * limited  to a  very  small  ~256 MiB  region  for technical  regions,
     * something to  do with the  PCIe address space or  something, machines
     * with  resizable BAR  enabled can  take  advantage of  the entire  CPU
     * address space and skip using device-only memory. My relatively recent
     * machine doesn't  support this so  I figure it's reasonable  to assume
     * it's  not a  good idea  to rely  on this.  But we  can use  it as  an
     * optimization path later for simplifying control flow complexity.

     TL;DR we are using DEVICE_LOCAL memory and uploading through a staging buffer.

    NOTE: It's no longer applicable to apply the access flags to the whole
    device allocator.  This is done on a per-block basis. */
    g_vulkan->device_memory = {
        .name = "global_device_memory",
        // -1 means max memory size
        .size = 1_GiB,
    };
    vulkan_memory_init( &g_vulkan->device_memory );

    // Don't need to initialize mesh, vulkan_mesh does it automatically.
    // Needs to run after vulkan_memory_init.
    vulkan_mesh_init( &g_vulkan->test_triangle );
    vulkan_mesh_init( &g_vulkan->test_ui_triangle );
    vulkan_mesh_init( &g_vulkan->test_ui_square );
    vulkan_mesh_init( &g_vulkan->test_teapot );
    vulkan_mesh_init( &g_vulkan->test_whale );
    vulkan_mesh_init( test_ui_circle );
    g_vulkan->tmp_meshes = {
        &g_vulkan->test_triangle,
        &g_vulkan->test_ui_triangle,
        &g_vulkan->test_ui_square,
        test_ui_circle,
        &g_vulkan->test_teapot,
        &g_vulkan->test_whale
    };


    /* SECTION: Render Pass - "An object that represents a set of
       framebuffer attachments and phases of rendering using those
       attachments. Represented by a VkRenderPass object."

       A render pass describes framebuffers, each pipeline draws to one
       framebuffer, each pipeline could have its own or use a shared
       framebuffer. */

    // Create a render pass, will be passed to pipeline
    VkAttachmentDescription color_attachment {};
    color_attachment.format = self->swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Only need one color attachment?
    array<VkAttachmentReference> color_attachment_refs {
        VkAttachmentReference {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
    };

    // sub-pass first
    VkSubpassDescription sub_pass {};
    sub_pass.pColorAttachments = color_attachment_refs.data;
    sub_pass.colorAttachmentCount = 1;

    VkRenderPass& render_pass = g_vulkan->render_pass;
    VkRenderPassCreateInfo pass_args{};
    pass_args.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_args.attachmentCount = 1;
    pass_args.pAttachments = &color_attachment;
    pass_args.subpassCount = 1;
    pass_args.pSubpasses = &sub_pass;

    auto pass_ok = vkCreateRenderPass(
        g_vulkan->logical_device, &pass_args, g_vulkan->vk_allocator, &render_pass );
    if (pass_ok)
    {   VULKAN_ERROR( "Failed to create render pass" ); return false; }
    VULKAN_LOG( "Created render pass" );
    g_vulkan->resources.push_cleanup( [] {
        VULKAN_LOG( "Destroying render pass" );
        vkDestroyRenderPass( g_vulkan->logical_device, g_vulkan->render_pass,
                             g_vulkan->vk_allocator );
    });

    /* Create swapchain before pipeline so we can pass present surface current extent
       But after render pass... because it gets passed in the swapchain */
    g_vulkan->swapchain.name = "version_0";
    g_vulkan->swapchain.present_size = as<VkExtent2D>( g_render->ui_camera.sensor_size );
    vulkan_swapchain_init( &g_vulkan->swapchain, VK_NULL_HANDLE );

    /* NOTE Descriptor pools are fixed size, so they need to be created on
     * demand. But some shaders use common resources, so we'll bind some common
     * ones up front, and pass it to the pipeline, and then do per-pipeline
     * resources later on where it's relevant
     *
     * TODO: Need to figure out if we can use descriptor resources from 2
     * different pools in the same shader */
    VkDescriptorPoolSize descriptor_size {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = u32(g_vulkan->frames_inflight_count)
    };

    VkDescriptorPoolCreateInfo descriptor_pool_args {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0x0,
        .maxSets = u32(g_vulkan->frames_inflight_count),
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_size
    };

    VkDescriptorPool frame_descriptor_pool;
    VkResult descriptor_bad = vkCreateDescriptorPool(
        g_vulkan->logical_device,
        &descriptor_pool_args,
        g_vulkan->vk_allocator,
        &frame_descriptor_pool
    );
    if (descriptor_bad)
    {   VULKAN_ERRORF( "Failed to create descriptor resource pool {}",
                       string_VkResult( descriptor_bad ) );
    }
    else
    {
        g_vulkan->resources.push_cleanup( [frame_descriptor_pool] {
            VULKAN_LOG( "Destroying descriptor set layout" );
            vkDestroyDescriptorPool(
                g_vulkan->logical_device, frame_descriptor_pool, g_vulkan->vk_allocator ); });
    }


    // Create primary generic pipeline
    g_vulkan->mesh_pipeline.vk_resource_pool = frame_descriptor_pool;
    vulkan_init_pipelines();

    /* Create per-frame data, we may have more than one frame going at once and per-frame resources
     NOTE: Dependant on descriptor resource data and pipeline data, must run afterwards */
    g_vulkan->frames_inflight.resize( g_vulkan->frames_inflight_count );
    g_vulkan->frames_inflight.map_procedure( []( vulkan_frame& arg ) {
        vulkan_frame_init( &arg, &g_vulkan->mesh_pipeline ); });

    result = true;
    g_vulkan->initialized = true;
    return result;
}

PROC vulkan_destroy() -> void
{
    PROFILE_SCOPE_FUNCTION();
    if (g_vulkan == nullptr || g_vulkan->initialized == false) { return; }
    // Wait for device before attempting to cleanup
    auto device_wait_bad = vkDeviceWaitIdle( g_vulkan->logical_device );
    auto present_wait_bad = vkQueueWaitIdle( g_vulkan->present_queue );
    auto graphics_wait_bad = vkQueueWaitIdle( g_vulkan->graphics_queue );

    vulkan_swapchain_destroy( &g_vulkan->swapchain );
    // Might be needed one day, but not right now
    // g_vulkan->resources.run_cleanup();
    g_vulkan->~vulkan_context();
    g_vulkan = nullptr;
}

PROC vulkan_tick() -> void
{
    PROFILE_SCOPE_FUNCTION();
    if (g_vulkan == nullptr) { return; }

    bool restart_vulkan = g_vulkan->restart_vulkan || g_vulkan->device_lost;
    if (restart_vulkan)
    {
        VULKAN_LOG( "Restarting Vulkan context" );
        vulkan_destroy();
        vulkan_init();
    }

    // New tick setup

    vulkan_draw();
}

PROC vulkan_draw() -> void
{
    PROFILE_SCOPE_FUNCTION();
    if (g_vulkan == nullptr) { return; }
    // -- Function Setup
    auto self = g_vulkan;
    auto& swapchain = self->swapchain;
    if (g_vulkan->initialized == false) { return; }
    i64 current_frame_i = g_vulkan->frames_started;

    // -- Pre-Draw Start Setup and Reset Tasks
    // Clear acquire fence if we skipped the last frame
    TracyCZoneN( zone_new_frame, "Vulkan Acquire new Frame", true );
    vkResetFences( self->logical_device, 1, &g_vulkan->frame_acquire_fence );

    // Set draw commands
    u32 image_index {};
    u32 inflight_frame_i {};
    auto acquire_bad = vkAcquireNextImageKHR(
        g_vulkan->logical_device,
        g_vulkan->swapchain.platform_swapchain,
        0,
        VK_NULL_HANDLE,
        g_vulkan->frame_acquire_fence,
        &image_index
    );
    inflight_frame_i = image_index;

    if (acquire_bad == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // static time_periodic resize_delay( 16ms );
        // if (resize_delay.triggered() == false)
        // { return; }

        // Reset fence before using again VUID-vkResetFences-pFences-01123
        vkResetFences( self->logical_device, 1, &g_vulkan->frame_acquire_fence );

        /* if This code is reached there's a pretty good chance the widow was
           resized or tampered with and it invalidated the swapchain.

           We have to wait for all for all frames to finish before we're allowed
           to regenerate the swapchain. */
        vkWaitForFences(
            g_vulkan->logical_device,
            swapchain.frame_end_fences.size(),
            swapchain.frame_end_fences.data,
            true,
            3'000'000'000
        );
        VkSwapchainKHR reuse_swapchain = g_vulkan->swapchain.platform_swapchain;
        vulkan_swapchain_destroy( &g_vulkan->swapchain );
        g_vulkan->swapchain.name = fmt::format( "version_{}", current_frame_i );
        g_vulkan->swapchain.vk_present_size = as<VkExtent2D>( g_render->ui_camera.sensor_size );
        vulkan_swapchain_init( &g_vulkan->swapchain, reuse_swapchain );

        acquire_bad = vkAcquireNextImageKHR(
            g_vulkan->logical_device,
            g_vulkan->swapchain.platform_swapchain,
            0,
            VK_NULL_HANDLE,
            g_vulkan->frame_acquire_fence,
            &image_index
        );
    }
    TracyCZoneEnd( zone_new_frame );

    // New new frame needs to be rendered yet, do something else
    if (acquire_bad == VK_NOT_READY)
    {   VULKAN_LOG( "No frame ready to begin from vkAcquireNextImageKHR 'VK_NOT_READY'" );
        return;
    }
    else if (acquire_bad)
    {   VULKAN_ERRORF( "Failed to acquire next presentable image '{}'",
                       string_VkResult(acquire_bad) );
        return;
    }

    VkFence frame_end_fence = swapchain.frame_end_fences[ image_index ];
    // Wait on 'frame_acquire_fence' before proceeding to reset the fence
    auto end_timeout = vkWaitForFences(
        g_vulkan->logical_device, 1, &frame_end_fence, true, 1'0000'000'000 );
    vkResetFences( self->logical_device, 1, &frame_end_fence );
    if (end_timeout == VK_TIMEOUT)
    {   VULKAN_ERRORF( "Huge hitch waiting on frame index {}", image_index ); return; }

    /* Wait for frame acquire before proceeding to resetting command buffer This
       sort of halts when the next frame is not completed or blocked so no more
       work can be done */
    auto frame_timeout = vkWaitForFences(
        g_vulkan->logical_device, 1, &self->frame_acquire_fence, true, 16'666'666 );
    if (frame_timeout == VK_TIMEOUT)
    {   VULKAN_ERRORF( "Frame: {}] | Missed frame!", current_frame_i );
        return;
    }
    else if (frame_timeout == VK_ERROR_DEVICE_LOST)
    {   VULKAN_ERROR( "Something really horrible happened, "
                      "device was lost waiting on frame end, 'VK_DEVICE_LOST'" );
        g_vulkan->device_lost = true;
        return;
    }
    else if (frame_timeout == VK_SUCCESS)
    {
        // VULKAN_LOGF( "Frame: {} | Completed Frame.", current_frame_i );
        if (g_render->display_ready == false) { g_render->display_ready = true; }
        FrameMarkEnd( "Vulkan Inflight Frame" );
    }
    FrameMarkStart( "Vulkan Inflight Frame" );

    VkCommandBuffer command_buffer = self->commands[ image_index ];
    vkResetCommandBuffer( command_buffer, 0x0 );

    // -- Get started on new frame --
    TracyCZoneN( zone_setup_frame, "Vulkan Setup Frame", true );
    ++g_vulkan->frames_started;

    // SECTION: Set up some per frame data
    vulkan_frame* current_frame = g_vulkan->frames_inflight.address( image_index );
    vulkan_pipeline* current_pipeline = &g_vulkan->ui_mesh_pipeline;

    /* NOTE: We can have multiple frames inflight so we need to copy a seperate
       draw queue for each frame */
    current_frame->draw_queue_mesh.reset();
    current_frame->draw_queue_image.reset();
    current_frame->draw_queue_mesh = g_render->draw_queue_mesh;
    current_frame->draw_queue_image = g_render->draw_queue_image;

    current_frame->draw_index = current_frame_i;
    current_frame->inflight_index = inflight_frame_i;
    // TODO: Change this if we go back to a 3D pipeline, this was meant for UI rendering
    current_frame->uniform.camera = (matrix_camera_view( g_render->ui_camera.transform ) *
                                     g_render->ui_camera.create_orthographic_projection());

    // SECTION: Iterate through the draw images and see if any need updating before drawing
    // NOTE: We have to do this after aquiring a new frame or we keep on issuing endless commands on VK_TIMEOUT
    for (i32 i=0; i < g_render->draw_queue_image.size(); ++i)
    {
        render_image* current_image = g_render->draw_queue_image[i];
        vulkan_image_prepare( current_image );
    }

    // Setup Uniform
    frame_general_uniform* current_uniform = &current_frame->uniform;
    // current_uniform->epoch = tyon::g_program_epoch;
    current_uniform->time_since_epoch = time_elapsed_seconds();

     // (take copy on purpose for temporary camera data)
    frame_general_uniform uniform_copy = *current_uniform;
    uniform_copy.camera = current_uniform->camera.unreal_to_vulkan().transpose();

    auto uniform_queue_bad = vulkan_transfer_queue_buffer(
        &g_vulkan->transfer,
        &current_frame->general_uniform_buffer,
        sizeof(frame_general_uniform),
        0
    );
    if (! uniform_queue_bad.error)
    {
        memory_copy<frame_general_uniform>( uniform_queue_bad.value.data, &uniform_copy, 1 );
    }

    // Update the descriptor resource associated with the uniform
    VkDescriptorBufferInfo resource_buffer_info {
        .buffer = current_frame->general_uniform_buffer.buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    VkWriteDescriptorSet resource_write_args {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = current_frame->vk_resource,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &resource_buffer_info,
        .pTexelBufferView = nullptr
    };
    TracyCZoneEnd( zone_setup_frame );

    // Finalize the copy. No error return.
    vkUpdateDescriptorSets (g_vulkan->logical_device, 1, &resource_write_args, 0, nullptr );

    // Start writing draw commands to command buffer
    VkCommandBufferBeginInfo begin_args {};
    begin_args.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_args.flags = 0; // Optional
    begin_args.pInheritanceInfo = nullptr; // Optional
    // Start recording commands into the command buffer

    TracyCZoneN( zone_record_commands_frame, "Vulkan Frame Record Commands", true );
    VkResult command_ok = vkBeginCommandBuffer( command_buffer, &begin_args );

    // -- Start recording into first subpass.--
    // This is started after beginning a command buffer.
    // SECTION: Start recording memory transfers before we start any rendering

    vulkan_transfer* x_transfer = nullptr;
    vulkan_transfer_buffer* x_transfer_buffer = nullptr;
    for (i64 i=0; i < g_vulkan->transfer.transfer_queue.size(); ++i)
    {
        x_transfer = g_vulkan->transfer.transfer_queue.address(i);
        x_transfer_buffer = g_vulkan->transfer.buffers.address( x_transfer->buffer_index );
        search_result<vulkan_buffer> buffer_search = g_vulkan->buffers.linear_search(
            [id = x_transfer_buffer->buffer](vulkan_buffer& arg) {
                return arg.id == id; });
        VkBuffer staging_buffer = buffer_search.match->buffer;

        switch (x_transfer->destination)
        {
            case e_vulkan_memory_object::buffer:
            {
                VkBufferCopy copy_region {
                    .srcOffset = u64(x_transfer->position),
                    .dstOffset = u64(x_transfer->buffer_offset),
                    .size = u64(x_transfer->size)
                };

                if (buffer_search.match_found)
                {
                    vkCmdCopyBuffer(
                        command_buffer,
                        staging_buffer,
                        x_transfer->destination_buffer,
                        1,
                        &copy_region
                    );

                    // NOTE: I guess we need a seperate barrier depending on the memory object?
                    VkBufferMemoryBarrier transfer_barrier {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                        .pNext = nullptr,
                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        // TODO: What is the destination mask meant to be?
                        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                        // TODO: Revise if the queue family changes
                        .srcQueueFamilyIndex = u32(g_vulkan->graphics_queue_family),
                        .dstQueueFamilyIndex = u32(g_vulkan->graphics_queue_family),
                        .buffer = x_transfer->destination_buffer,
                        .offset = copy_region.dstOffset,
                        .size = copy_region.size
                    };
                    vkCmdPipelineBarrier(
                        command_buffer,
                        // or COLOR_ATTACHMENT_OUTPUT_BIT if coming from render
                        // VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT is depreceated aparently
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        // TODO: Investigate what dependency bits to use
                        0,
                        0, nullptr,
                        1, &transfer_barrier,
                        0, nullptr
                    );
                }
                break;
            }
            case e_vulkan_memory_object::image:
            {
                auto image_result = g_vulkan->images.linear_search(
                    [image_id = x_transfer->destination_image_]( vulkan_image& arg_ ) {
                    return (arg_.id == image_id) && arg_.id.valid(); } );
                vulkan_image* vk_image = image_result.match;
                if (vk_image == nullptr) { break; }
                
                v2_f32 image_size = vk_image->size;
                VkBufferImageCopy copy_region {
                    .bufferOffset = x_transfer->position,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource = {
                        // TODO: I don't unerstand what this flag is
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                    .imageOffset = { 0, 0, 0 },
                    .imageExtent { u32(image_size.x), u32(image_size.y), u32(1) }
                };

                if (buffer_search.match_found)
                {

                    VkImageMemoryBarrier copy_barrier = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        // no prior access needed for present → transfer
                        .srcAccessMask       = 0,
                        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                        // or UNDEFINED on first acquire
                        .oldLayout           = vk_image->layout,
                        // Make it available to use now
                        .newLayout           = vk_image->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .image               = vk_image->platform_image,
                        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                    };

                    vkCmdPipelineBarrier(
                        command_buffer,
                        // or COLOR_ATTACHMENT_OUTPUT_BIT if coming from render
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &copy_barrier
                    );
                    vkCmdCopyBufferToImage(
                        command_buffer,
                        staging_buffer,
                        vk_image->platform_image,
                        vk_image->layout,
                        1,
                        &copy_region
                    );

                }
                break;
            }
            default: break;
        }
    }
    /* The memory transfer queue isn't strictly bound to a frame... and I don't
       know if it will ever need to be...  but it should be safe to reset each
       time because its passed to the command buffer to save the state... and it
       only needs to be done once...

       WARNING: HOWEVER, you do have to synchronize with a barrier before
       starting a new transfer

       NOTE: I keep on moving this because of issues with resetting it too early
       and the more time I relocate it the more I think my current Vulkan
       organization is wrong in some way

       NOTE: It doesn't actually have to be reset here, it just needs to be
       reset when all the transfers are done. */
    // Reset everyting to do with per-frame transfers
    g_vulkan->transfer.transfer_queue.reset();
    // Need to reset used position too
    g_vulkan->transfer.buffers.map_procedure( [](vulkan_transfer_buffer& arg) {
        arg.head_size = 0; });

    // Set render pass start information
    VkClearValue clear_value {};
    clear_value.color = {{ 0.2f, 0.0f, 0.2f, 1.0f }};
    // VkClearValue clear_values[] = { clear_value, clear_value };
    VkRenderPassBeginInfo render_pass_args{};
    render_pass_args.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_args.renderPass = self->render_pass;
    render_pass_args.framebuffer = self->swapchain_framebuffers[ image_index ];
    render_pass_args.renderArea.offset = {0, 0};
    render_pass_args.renderArea.extent = swapchain.vk_present_size;
    render_pass_args.clearValueCount = 1;
    render_pass_args.pClearValues = &clear_value;
    vkCmdBeginRenderPass( command_buffer, &render_pass_args, VK_SUBPASS_CONTENTS_INLINE );
    // Must bind pipeline before using it
    vkCmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline->platform_pipeline );

    bool resize_viewport = true;
    if (resize_viewport)
    {
        PROFILE_SCOPE( "Vulkan Resize Viewport" );
        // Resize viewport and scissor
        VkViewport viewport_config {
            // Upper left coordinates
            .x = 0,
            .y = 0,
            .width = float(swapchain.vk_present_size.width),
            .height = float(swapchain.vk_present_size.height),
            // Configurable viewport depth, can configurable but usually between 0 and 1
            .minDepth = 0.0,
            .maxDepth = 1.0
        };

        // Only render into a certain portion of the viewport with scissors
        VkRect2D scissor_config {
            VkOffset2D { 0, 0 },
            swapchain.vk_present_size
        };
        vkCmdSetViewport( command_buffer, 0, 1, &viewport_config );
        vkCmdSetScissor( command_buffer, 0, 1, &scissor_config );
    }

    for (i32 i=0; i < current_frame->draw_queue_mesh.size(); ++i)
    {
        // SECTION: Select mesh for drawing
        mesh* draw_mesh = current_frame->draw_queue_mesh[i];

        // Test Draw Meshes
        // draw_mesh = &g_vulkan->test_whale;
        // draw_mesh = &g_vulkan->test_teapot;
        // draw_mesh = &g_vulkan->test_ui_triangle;
        // make test UI meshes resize with window for convenience
        g_vulkan->test_ui_triangle.transform.scale = (v3{0.7} * g_render->ui_camera.sensor_size.y);
        g_vulkan->test_ui_square.transform.scale = (v3{0.7} * g_render->ui_camera.sensor_size.y);
        g_vulkan->test_teapot.transform.scale = (v3{40});
        g_vulkan->test_teapot.transform.rotation.z = 6.28 * 0.25;

        // Find the associated vulkan mesh
        auto mesh_result = g_vulkan->meshes.linear_search( [draw_mesh]( vulkan_mesh& arg ) {
            return arg.id == draw_mesh->id && arg.id.valid(); } );
        vulkan_mesh* vk_draw_mesh = nullptr;
        bool no_vulkan_mesh = (draw_mesh->id.valid() && mesh_result.match_found == false);
        if (no_vulkan_mesh)
        {
            // Recreate and search again
            vulkan_mesh_init( draw_mesh );
            mesh_result = g_vulkan->meshes.linear_search( [=]( vulkan_mesh& arg ) {
                return arg.id == draw_mesh->id && arg.id.valid(); } );
        }
        // NOTE: Pointer copy covers both no_vulkan_mesh search and first search
        vk_draw_mesh = mesh_result.match;
        if (mesh_result.match_found == false)
        {
            VULKAN_ERROR( "We tried to draw a mesh with no associated Vulkan data" );
            continue;
        }


        // Bind vertex/ data to pipeline data slots
        VkBuffer vertex_buffers[] = { vk_draw_mesh->vertex_buffer.buffer };
        /** NOTE: This is the offset for the binding being described by the
         * buffer. NOT the buffer in a memory object. */
        VkDeviceSize offsets[] = { u64(0) };
        u32 n_buffers = 1;
        vkCmdBindVertexBuffers( command_buffer, 0, n_buffers, vertex_buffers, offsets );
        u32 first_set_offset = 0;
        u32 resource_n = 1;
        const uint32_t* dynamic_offsets = nullptr;
        u32 dynamic_offsets_n = 0;
        vkCmdBindDescriptorSets(
                                command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                current_pipeline->platform_layout,
                                first_set_offset,
                                resource_n,
                                &current_frame->vk_resource,
                                dynamic_offsets_n,
                                dynamic_offsets
                                );
        // Pass push constants with basic mesh data
        vulkan_mesh_shader_push mesh_push;
        mesh_push.local_space = matrix_create_transform( draw_mesh->transform );
        mesh_push.debug_mode = g_vulkan->mesh_debug_mode;
        vkCmdPushConstants(
                           command_buffer,
                           current_pipeline->platform_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof( vulkan_mesh_shader_push),
                           &mesh_push
                           );

        // Draw an actual mesh
        auto mesh_args = vulkan_mesh_draw_args {
            .n_vertexes = u32(draw_mesh->vertexes_n),
            .n_instances = 1,
            .first_vertex = 0,
            .first_instance = 0
        };
        vkCmdDraw(
                  command_buffer,
                  mesh_args.n_vertexes,
                  mesh_args.n_instances,
                  mesh_args.first_vertex,
                  mesh_args.first_instance
                  );
    }

    VkPipelineStageFlags wait_stages[] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    // Finalize frame and submit all commands
    VkSubmitInfo submit_args {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        // No semaphores to wait on yet
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = wait_stages,
        // Just the one command buffer for now
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &self->queue_submit_semaphore,
    };

    // VkResult sync_ok = vkWaitForFences(
    //     self->logical_device,
    //     1,
    //     &self->frame_begin_fence,
    //     true,
    //     16'666'666
    // );
    // vkResetFences( self->logical_device, 1, &self->frame_begin_fence );
    vkCmdEndRenderPass( command_buffer );

    // Have to transition the image layout with sychronisation to perform the image blit
    // Transition the framebuffer to destination
    VkImageMemoryBarrier blit_framebuffer_barrier =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        // no prior access needed for present → transfer
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        // or UNDEFINED on first acquire
        // TODO: update old layout to vulkan_image->layout
        .oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image               = g_vulkan->swapchain_images[ inflight_frame_i ],
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    vkCmdPipelineBarrier(
        command_buffer,
         // or COLOR_ATTACHMENT_OUTPUT_BIT if coming from render
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &blit_framebuffer_barrier
    );

    /* NOTE: Vulkan spec states that blitting cannot happen inside of an active render pass */
    for (i32 i=0; i < current_frame->draw_queue_image.size(); ++i)
    {
        render_image* draw_image = current_frame->draw_queue_image[i];
        // Find the associated vulkan image
        auto image_result = g_vulkan->images.linear_search( [=]( vulkan_image& arg ) {
            return (arg.associated_image == draw_image->id) && arg.id.valid(); } );
        vulkan_image* vk_draw_image = image_result.match;
        bool no_vulkan_image = (draw_image->id.valid() && image_result.match_found == false);

        if (vk_draw_image)
        {

            // TODO: Should already be in sreenspace coordinates by this time
            box_2d clip = draw_image->clip_region;
            box_2d region = draw_image->draw_region;
            VkExtent2D present = g_vulkan->swapchain.present_size;
            bool unset_clip = (clip.size.x == 0 && clip.size.y == 0);
            bool unset_region = (region.size.x == 0 && region.size.y == 0);

            if (unset_clip)
            {   // Default to whole image size
                clip.position = { 0.0, 0.0 };
                clip.size = { f32(draw_image->image.size.x), f32(draw_image->image.size.y) };
            }
            if (unset_region)
            {   // Default to swapchain framebuffer size aka 'present_size'
                region.position = { 0.0, 0.0 };
                region.size = { f32(present.width), f32(present.height) };
            }

            v2_f32 clip_down_left = clip.position;
            v2_f32 clip_up_right = clip.position + clip.size;
            v2_f32 draw_up_left = region.position + v2_f32{ 0.0f, region.size.y };
            v2_f32 draw_down_right = region.position + v2_f32{ region.size.x, 0.0f };
            draw_up_left.y = present.height - draw_up_left.y;
            draw_down_right.y =  present.height - draw_down_right.y;

            // Clamp draw region to framebuffer to prevent memory corruption
            draw_up_left.x = clamp_range( 0,  present.width, draw_up_left.x );
            draw_up_left.y = clamp_range( 0,  present.height, draw_up_left.y );
            draw_down_right.x = clamp_range( 0,  present.height, draw_down_right.x );
            draw_down_right.y = clamp_range( 0,  present.width, draw_down_right.y );
            clip_down_left.x = clamp_range( 0,  draw_image->image.size.x, clip_down_left.x );
            clip_down_left.y = clamp_range( 0,  draw_image->image.size.y, clip_down_left.y );
            clip_up_right.x = clamp_range( 0,  draw_image->image.size.x, clip_up_right.x );
            clip_up_right.y = clamp_range( 0,  draw_image->image.size.y, clip_up_right.y );

            VkImageBlit vk_region {
                .srcSubresource = VkImageSubresourceLayers {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .srcOffsets= {
                    // Flip
                    VkOffset3D {i32(clip_down_left.x), i32(clip_down_left.y), 0},
                    VkOffset3D { i32(clip_up_right.x), i32(clip_up_right.y), 1 }
                },
                .dstSubresource = VkImageSubresourceLayers {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .dstOffsets = {
                    VkOffset3D {i32(draw_up_left.x), i32(draw_up_left.y), 0},
                    VkOffset3D { i32(draw_down_right.x), i32(draw_down_right.y), 1 }
                },
            };

            // Transition the blit image to soruce since we're copying from it
            VkImageMemoryBarrier image_barrier =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                // no prior access needed for present → transfer
                .srcAccessMask       = 0,
                .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                // or UNDEFINED on first acquire
                .oldLayout           = vk_draw_image->layout,
                // We're writing so use transfer destination layout
                .newLayout           = vk_draw_image->layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image               = vk_draw_image->platform_image,
                .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };

            vkCmdPipelineBarrier(
                command_buffer,
                // or COLOR_ATTACHMENT_OUTPUT_BIT if coming from render
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &image_barrier
            );

            u32 vk_region_n = 1;
            vkCmdBlitImage(
                command_buffer,
                vk_draw_image->platform_image,
                vk_draw_image->layout,
                g_vulkan->swapchain_images[ inflight_frame_i ],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                vk_region_n,
                &vk_region,
                VK_FILTER_LINEAR
            );

        }
    }
    // Now transition back to present
    VkImageMemoryBarrier present_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        // no prior access needed for present → transfer
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = g_vulkan->swapchain_images[ inflight_frame_i ],
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &present_barrier
    );

    vkEndCommandBuffer( command_buffer );
    TracyCZoneEnd( zone_record_commands_frame );

    // if (sync_ok != VK_SUCCESS)
    // { TYON_ERROR( "Failed to wait on frame start fence for some reason" ); }
    vkQueueSubmit( self->graphics_queue, 1, &submit_args, frame_end_fence );

    VkSwapchainKHR present_swapchains[] = { self->swapchain.platform_swapchain };
    VkPresentInfoKHR present_args {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &self->queue_submit_semaphore,
        .swapchainCount = 1,
        .pSwapchains = present_swapchains,
        .pImageIndices = &image_index,
        // This can be used for storing results from each individual swapchain
        .pResults = nullptr,
    };
    VkResult present_bad = vkQueuePresentKHR( self->present_queue, &present_args );
    if (present_bad)
    {   VULKAN_ERRORF( "Fatal error '{}' with drawing and presentation 'VkQueuePresent'",
                       string_VkResult(present_bad) );
    }

    // TODO: Remove because useless now?
    // auto frame_timeout = vkWaitForFences(
    // self->logical_device, 1, &frame_end_fence, true, 1'000'000'000 );
}

}
