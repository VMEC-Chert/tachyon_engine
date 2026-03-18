
/** NOTE: This is just a file to hold stuff being worked on ,this should not be included as a part of any build*/

/* NOTE: Vulkan spec states that blitting cannot happen inside of an active render pass */
    for (i32 i=0; i < frame->draw_queue_image.size(); ++i)
    {
        render_image* draw_image = frame->draw_queue_image[i];
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
            {   // Default to corner of the screen at native size
                region.position = { 0.0, 0.0 };
                region.size = v2_f32{ f32( draw_image->image.size.x), f32(draw_image->image.size.y) };
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

            // Transition the blit image a SHADER_READ_ONLY so the shader can read it
            VkImageMemoryBarrier image_barrier =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                // no prior access needed for present → transfer
                .srcAccessMask       = 0,
                .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                // or UNDEFINED on first acquire
                .oldLayout           = vk_draw_image->platform_layout,
                // We're writing so use transfer destination layout
                .newLayout           = (vk_draw_image->platform_layout =
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                .image               = vk_draw_image->platform_image,
                .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };

            vkCmdPipelineBarrier(
                frame->command,
                // or COLOR_ATTACHMENT_OUTPUT_BIT if coming from render
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                // NOTE: Required flag when doing a barrier/transition inside a render pass
                VK_DEPENDENCY_VIEW_LOCAL_BIT,
                0, nullptr,
                0, nullptr,
                1, &image_barrier
            );

            // Update the image/texture
            // NOTE: Validation says it prefers descriptors to be updated before binding them
            // pipeline = &g_vulkan->ui_blit_pipeline;
            // VkDescriptorImageInfo image_info {
            //     .sampler = pipeline->base_sampler,
            //     .imageView = vk_draw_image->platform_view,
            //     .imageLayout = vk_draw_image->platform_layout
            // };
            // VkWriteDescriptorSet resource_write_args {
            //     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            //     .pNext = nullptr,
            //     .dstSet = frame->blit_resource,
            //     .dstBinding = 1,
            //     .dstArrayElement = 0,
            //     .descriptorCount = 1,
            //     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            //     .pImageInfo = &image_info,
            //     .pBufferInfo = nullptr,
            //     .pTexelBufferView = nullptr
            // };
            // vkUpdateDescriptorSets (g_vulkan->logical_device, 1, &resource_write_args, 0, nullptr );

            vkCmdBindPipeline(
                frame->command,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->platform_pipeline
            );
            u32 first_set_offset = 0;
            u32 resource_n = 1;
            const uint32_t* dynamic_offsets = nullptr;
            u32 dynamic_offsets_n = 0;
            // vkCmdBindDescriptorSets(
            //     frame->command,
            //     VK_PIPELINE_BIND_POINT_GRAPHICS,
            //     pipeline->platform_layout,
            //     first_set_offset,
            //     resource_n,
            //     &frame->blit_resource,
            //     dynamic_offsets_n,
            //     dynamic_offsets
            // );

            /* Draw a triangle covering the entire screen and stretching outside the boundaries
               But generate the triangle directly in the shader
            NOTE: Bind a bogus buffer first since we don't actually need any vertex data */
            VkDeviceSize offsets[] = { u64(0) };
            u32 n_buffers = 1;
            ERROR_GUARD( g_vulkan->meshes.head_size >= 1,
                         "We're supposed to have 1 valid mesh by now" );
            VkBuffer& stub_buffer = g_vulkan->meshes[0].vertex_buffer.buffer;
            vkCmdBindVertexBuffers( frame->command, 0, n_buffers, &stub_buffer, offsets );
            vkCmdDraw( frame->command, 3, 1, 0, 0 );
        }
    }
