#include <stdio.h>
#include <memory.h>

#include <vgpu.h>
#include "vgpu_internal.h"

/******************************************************************************\
 *
 *  Structures
 *
\******************************************************************************/

struct vgpu_buffer_s
{
};

struct vgpu_resource_table_s
{
};

struct vgpu_root_layout_s
{
};

struct vgpu_texture_s
{
};

struct vgpu_program_s
{
};

struct vgpu_pipeline_s
{
};

struct vgpu_render_pass_s
{
};

struct vgpu_command_list_s
{
	vgpu_device_t* device;
};

struct vgpu_thread_context_s
{
};

struct vgpu_device_s
{
	vgpu_allocator_t* allocator;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;

	uint64_t frame_no;
	vgpu_caps_t caps;
};

/******************************************************************************\
 *
 *  Device operations
 *
\******************************************************************************/

vgpu_device_t* vgpu_create_device(const vgpu_create_device_params_t* params)
{
	extern vgpu_allocator_t vgpu_allocator_default;

	vgpu_allocator_t* allocator = params->allocator ? params->allocator : &vgpu_allocator_default;
	vgpu_device_t* device = VGPU_ALLOC_TYPE(allocator, vgpu_device_t);
	device->allocator = allocator;

	device->log_func = params->log_func;
	device->error_func = params->error_func;

	device->frame_no = 0;
	memset(&device->caps, 0, sizeof(device->caps));
	device->caps.flags = (~params->force_disable_flags) & (VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET | VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET);

	return device;
}

void vgpu_destroy_device(vgpu_device_t* device)
{
	VGPU_FREE(device->allocator, device);
}

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device)
{
	return VGPU_DEVICE_NULL;
}

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue)
{
}

void vgpu_present(vgpu_device_t* device)
{
	device->frame_no++;
}

vgpu_texture_t* vgpu_get_back_buffer(vgpu_device_t* device)
{
	return nullptr;
}

uint64_t vgpu_get_frame_no(vgpu_device_t* device)
{
	return device->frame_no;
}

uint64_t vgpu_get_frame_id(vgpu_device_t* device)
{
	return device->frame_no % VGPU_MULTI_BUFFERING;
}

uint64_t vgpu_max_buffered_frames(vgpu_device_t* device)
{
	return VGPU_MULTI_BUFFERING;
}

void vgpu_get_caps(vgpu_device_t* device, vgpu_caps_t* out_caps)
{
	memcpy(out_caps, &device->caps, sizeof(vgpu_caps_t));
}

/******************************************************************************\
*
*  Thread context handling
*
\******************************************************************************/

vgpu_thread_context_t* vgpu_create_thread_context(vgpu_device_t* device, const vgpu_create_thread_context_params_t* params)
{
	vgpu_thread_context_t* thread_context = VGPU_ALLOC_TYPE(device->allocator, vgpu_thread_context_t);
	return thread_context;
}

void vgpu_destroy_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
	VGPU_FREE(device->allocator, thread_context);
}

void vgpu_prepare_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
}

/******************************************************************************\
 *
 *  Buffer handling
 *
\******************************************************************************/

vgpu_buffer_t* vgpu_create_buffer(vgpu_device_t* device, const vgpu_create_buffer_params_t* params)
{
	vgpu_buffer_t* buffer = VGPU_ALLOC_TYPE(device->allocator, vgpu_buffer_t);
	return buffer;
}

void vgpu_destroy_buffer(vgpu_device_t* device, vgpu_buffer_t* buffer)
{
	VGPU_FREE(device->allocator, buffer);
}

/******************************************************************************\
 *
 *  Resource table handling
 *
\******************************************************************************/

vgpu_resource_table_t* vgpu_create_resource_table(vgpu_device_t* device, const vgpu_root_layout_t* root_layout, uint32_t root_slot, const vgpu_resource_table_entry_t* entries, size_t num_entries)
{
	vgpu_resource_table_t* resource_table = VGPU_ALLOC_TYPE(device->allocator, vgpu_resource_table_t);
	return resource_table;
}

void vgpu_destroy_resource_table(vgpu_device_t* device, vgpu_resource_table_t* resource_table)
{
	VGPU_FREE(device->allocator, resource_table);
}

/******************************************************************************\
*
*  Root layout handling
*
\******************************************************************************/

vgpu_root_layout_t* vgpu_create_root_layout(vgpu_device_t* device, const vgpu_root_layout_slot_t* slots, size_t num_slots)
{
	vgpu_root_layout_t* root_layout = VGPU_ALLOC_TYPE(device->allocator, vgpu_root_layout_t);
	return root_layout;
}

void vgpu_destroy_root_layout(vgpu_device_t* device, vgpu_root_layout_t* root_layout)
{
	VGPU_FREE(device->allocator, root_layout);
}

/******************************************************************************\
 *
 *  Texture handling
 *
\******************************************************************************/

vgpu_texture_t* vgpu_create_texture(vgpu_device_t* device, const vgpu_create_texture_params_t* params)
{
	vgpu_texture_t* texture = VGPU_ALLOC_TYPE(device->allocator, vgpu_texture_t);
	return texture;
}

void vgpu_destroy_texture(vgpu_device_t* device, vgpu_texture_t* texture)
{
	VGPU_FREE(device->allocator, texture);
}

/******************************************************************************\
*
*  Program handling
*
\******************************************************************************/

vgpu_program_t* vgpu_create_program(vgpu_device_t* device, const vgpu_create_program_params_t* params)
{
	vgpu_program_t* program = VGPU_ALLOC_TYPE(device->allocator, vgpu_program_t);
	return program;
}

void vgpu_destroy_program(vgpu_device_t* device, vgpu_program_t* program)
{
	VGPU_FREE(device->allocator, program);
}

/******************************************************************************\
 *
 *  Pipeline handling
 *
\******************************************************************************/

vgpu_pipeline_t* vgpu_create_pipeline(vgpu_device_t* device, const vgpu_create_pipeline_params_t* params)
{
	vgpu_pipeline_t* pipeline = VGPU_ALLOC_TYPE(device->allocator, vgpu_pipeline_t);
	return pipeline;
}

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline)
{
	VGPU_FREE(device->allocator, pipeline);
}

/******************************************************************************\
*
*  Render pass handling
*
\******************************************************************************/

vgpu_render_pass_t* vgpu_create_render_pass(vgpu_device_t* device, const vgpu_create_render_pass_params_t* params)
{
	vgpu_render_pass_t* render_pass = VGPU_ALLOC_TYPE(device->allocator, vgpu_render_pass_t);
	return render_pass;
}

void vgpu_destroy_render_pass(vgpu_device_t* device, vgpu_render_pass_t* render_pass)
{
	VGPU_FREE(device->allocator, render_pass);
}

/******************************************************************************\
 *
 *  Command list handling
 *
\******************************************************************************/

vgpu_command_list_t* vgpu_create_command_list(vgpu_device_t* device, const vgpu_create_command_list_params_t* params)
{
	vgpu_command_list_t* command_list = VGPU_ALLOC_TYPE(device->allocator, vgpu_command_list_t);
	command_list->device = device;
	return command_list;
}

void vgpu_destroy_command_list(vgpu_device_t* device, vgpu_command_list_t* command_list)
{
	VGPU_FREE(device->allocator, command_list);
}

bool vgpu_is_command_list_type_supported(vgpu_device_t* device, vgpu_command_list_type_t command_list_type)
{
	return true;
}

/******************************************************************************\
*
*  Command list building
*
\******************************************************************************/

void vgpu_begin_command_list(vgpu_thread_context_t* thread_context, vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
}

void vgpu_end_command_list(vgpu_command_list_t* command_list)
{
}

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params)
{
	params->unlock_data[0] = (uintptr_t)VGPU_ALLOC(command_list->device->allocator, params->num_bytes, 16);
	return (void*)params->unlock_data[0];
}

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params)
{
	VGPU_FREE(command_list->device->allocator, (void*)params->unlock_data[0]);
}

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes)
{
}

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table)
{
}

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes)
{
}

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline)
{
}

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer)
{
}

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices)
{
}

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex)
{
}

void vgpu_draw_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
}

void vgpu_draw_indexed_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
}

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
}

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
}

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}
