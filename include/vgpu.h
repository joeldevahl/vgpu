#ifndef VGPU_H
#define VGPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
*
*  General defines
*
\******************************************************************************/

#define VGPU_MAX_RENDER_TARGETS 8
#define VGPU_MAX_ROOT_SLOTS 4
#define VGPU_MULTI_BUFFERING 2

/******************************************************************************\
*
*  Enumerations
*
\******************************************************************************/

typedef enum
{
	VGPU_DEVICE_NULL,
	VGPU_DEVICE_DX11,
	VGPU_DEVICE_DX12,
	VGPU_DEVICE_GL,
	VGPU_DEVICE_VK,
	MAX_VGPU_DEVICE_TYPES,
} vgpu_device_type_t;

typedef enum
{
	VGPU_COMMAND_LIST_IMMEDIATE_GRAPHICS = 0,
	VGPU_COMMAND_LIST_GRAPHICS,
	VGPU_COMMAND_LIST_COMPUTE,
	VGPU_COMMAND_LIST_COPY,
} vgpu_command_list_type_t;

typedef enum
{
	VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET = 0x1,
	VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET = 0x2,
} vgpu_caps_flag_t;

typedef enum
{
	VGPU_USAGE_DEFAULT = 0,
	VGPU_USAGE_DYNAMIC,
} vgpu_usage_t;

// TODO: streamline or remove
typedef enum
{
	VGPU_RESOURCE_STATE_COMMON = 0,
	VGPU_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
	VGPU_RESOURCE_STATE_INDEX_BUFFER = 0x2,
	VGPU_RESOURCE_STATE_RENDER_TARGET = 0x4,
	VGPU_RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
	VGPU_RESOURCE_STATE_DEPTH_WRITE = 0x10,
	VGPU_RESOURCE_STATE_DEPTH_READ = 0x20,
	VGPU_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
	VGPU_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
	VGPU_RESOURCE_STATE_STREAM_OUT = 0x100,
	VGPU_RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
	VGPU_RESOURCE_STATE_COPY_DEST = 0x400,
	VGPU_RESOURCE_STATE_COPY_SOURCE = 0x800,
	VGPU_RESOURCE_STATE_RESOLVE_DEST = 0x1000,
	VGPU_RESOURCE_STATE_RESOLVE_SOURCE = 0x2000,
	VGPU_RESOURCE_STATE_GENERIC_READ = VGPU_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | VGPU_RESOURCE_STATE_INDEX_BUFFER | VGPU_RESOURCE_STATE_COPY_SOURCE | VGPU_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | VGPU_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | VGPU_RESOURCE_STATE_INDIRECT_ARGUMENT,
	VGPU_RESOURCE_STATE_PRESENT = 0,
	VGPU_RESOURCE_STATE_PREDICATION = VGPU_RESOURCE_STATE_INDIRECT_ARGUMENT
} vgpu_resource_state_t;

typedef enum
{
	VGPU_VERTEX_PROGRAM,
	VGPU_FRAGMENT_PROGRAM,
	MAX_VGPU_PROGRAM_TYPES,
} vgpu_program_type_t;

typedef enum
{
	VGPU_TEXTURETYPE_1D,
	VGPU_TEXTURETYPE_2D,
	VGPU_TEXTURETYPE_3D,
	VGPU_TEXTURETYPE_CUBE,
} vgpu_texture_type_t;

typedef enum
{
	VGPU_TEXTUREFORMAT_RGBA8,
	VGPU_TEXTUREFORMAT_BC1,
	VGPU_TEXTUREFORMAT_BC2,
	VGPU_TEXTUREFORMAT_BC3,

	VGPU_TEXTUREFORMAT_D32F_S8X24,
} vgpu_texture_format_t;

typedef enum
{
	VGPU_DATA_TYPE_FP32,
	VGPU_DATA_TYPE_UINT32,
	VGPU_DATA_TYPE_UINT16,
	VGPU_DATA_TYPE_UINT8,
} vgpu_data_type_t;

typedef enum
{
	VGPU_RESOURCE_NONE,
	VGPU_RESOURCE_TEXTURE,
	VGPU_RESOURCE_BUFFER,
	VGPU_RESOURCE_SAMPLER,
	VGPU_RESOURCE_TABLE,
} vgpu_resource_type_t;

typedef enum
{
	VGPU_PRIMITIVE_TRIANGLES,
	VGPU_PRIMITIVE_LINES,
} vgpu_primitive_type_t;

typedef enum
{
	VGPU_FILL_SOLID,
	VGPU_FILL_WIRE,
} vgpu_fill_mode_t;

typedef enum
{
	VGPU_WIND_CCW,
	VGPU_WIND_CW,
} vgpu_winding_order_t;

typedef enum
{
	VGPU_CULL_NONE,
	VGPU_CULL_FRONT,
	VGPU_CULL_BACK,
} vgpu_cull_mode_t;

typedef enum
{
	VGPU_BLEND_ELEM_ZERO,
	VGPU_BLEND_ELEM_ONE,
	VGPU_BLEND_ELEM_SRC_COLOR,
	VGPU_BLEND_ELEM_INV_SRC_COLOR,
	VGPU_BLEND_ELEM_SRC_ALPHA,
	VGPU_BLEND_ELEM_INV_SRC_ALPHA,
	VGPU_BLEND_ELEM_DEST_ALPHA,
	VGPU_BLEND_ELEM_INV_DEST_ALPHA,
	VGPU_BLEND_ELEM_DEST_COLOR,
	VGPU_BLEND_ELEM_INV_DEST_COLOR,
	VGPU_BLEND_ELEM_SRC_ALPHA_SAT,
	VGPU_BLEND_ELEM_BLEND_FACTOR,
	VGPU_BLEND_ELEM_INV_BLEND_FACTOR,
	VGPU_BLEND_ELEM_SRC1_COLOR,
	VGPU_BLEND_ELEM_INV_SRC1_COLOR,
	VGPU_BLEND_ELEM_SRC1_ALPHA,
	VGPU_BLEND_ELEM_INV_SRC1_ALPHA,
} vgpu_blend_elem_t;

typedef enum
{
	VGPU_BLEND_OP_ADD,
	VGPU_BLEND_OP_SUBTRACT,
	VGPU_BLEND_OP_REV_SUBTRACT,
	VGPU_BLEND_OP_MIN,
	VGPU_BLEND_OP_MAX,
} vgpu_blend_op_t;

typedef enum
{
	VGPU_COMPARE_ALWAYS,
	VGPU_COMPARE_LESS,
	VGPU_COMPARE_LESS_EQUAL,
	VGPU_COMPARE_EQUAL,
	VGPU_COMPARE_NOT_EQUAL,
	VGPU_COMPARE_GREATER_EQUAL,
	VGPU_COMPARE_GREATER,
	VGPU_COMPARE_NEVER,
} vgpu_compare_func_t;

typedef enum
{
	VGPU_STENCIL_OP_KEEP,
	VGPU_STENCIL_OP_ZERO,
	VGPU_STENCIL_OP_REPLACE,
	VGPU_STENCIL_OP_INCR_SAT,
	VGPU_STENCIL_OP_DECR_SAT,
	VGPU_STENCIL_OP_INVERT,
	VGPU_STENCIL_OP_INCR,
	VGPU_STENCIL_OP_DECR,
} vgpu_stencil_op_t;

typedef enum
{
	VGPU_ROOT_SLOT_TYPE_TABLE = 0,
	VGPU_ROOT_SLOT_TYPE_RESOURCE,
} vgpu_root_slot_type_t;

/******************************************************************************\
*
*  Internal types
*
\******************************************************************************/

typedef struct vgpu_device_s vgpu_device_t;
typedef struct vgpu_thread_context_s vgpu_thread_context_t;
typedef struct vgpu_command_list_s vgpu_command_list_t;
typedef struct vgpu_texture_s vgpu_texture_t;
typedef struct vgpu_buffer_s vgpu_buffer_t;
typedef struct vgpu_resource_table_s vgpu_resource_table_t;
typedef struct vgpu_root_layout_s vgpu_root_layout_t;
typedef struct vgpu_program_s vgpu_program_t;
typedef struct vgpu_pipeline_s vgpu_pipeline_t;
typedef struct vgpu_render_pass_s vgpu_render_pass_t;

/******************************************************************************\
*
*  Allocation interface
*
\******************************************************************************/

typedef struct vgpu_allocator_s
{
	 void* (*alloc)(struct vgpu_allocator_s* allocator, size_t count, size_t size, size_t align, const char* file, int line);
	 void* (*realloc)(struct vgpu_allocator_s* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line);
	 void  (*free)(struct vgpu_allocator_s* allocator, void* memory, const char* file, int line);
} vgpu_allocator_t;

/******************************************************************************\
*
*  Structures
*
\******************************************************************************/

typedef struct
{
	uint32_t flags;
} vgpu_caps_t;

typedef struct
{
	uint32_t num_vertices;
	uint32_t num_instances;
	uint32_t first_vertex;
	uint32_t first_instance;
} vgpu_draw_indirect_args_t;

typedef struct
{
	uint32_t num_indices;
	uint32_t num_instances;
	uint32_t first_index;
	uint32_t first_vertex;
	uint32_t first_instance;
} vgpu_draw_indexed_indirect_args_t;

typedef void (*vgpu_log_func_t)(const char* message);
typedef int (*vgpu_error_func_t)(const char* file, unsigned int line, const char* cond, const char* fmt, ...);

typedef struct vgpu_create_device_params_s
{
	vgpu_allocator_t* allocator;

	void* window;

	uint32_t force_disable_flags;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;
} vgpu_create_device_params_t;

typedef struct vgpu_create_thread_context_params_s
{
} vgpu_create_thread_context_params_t;

typedef struct vgpu_create_command_list_params_s
{
	vgpu_command_list_type_t type;
} vgpu_create_command_list_params_t;

typedef struct vgpu_root_layout_range_s
{
	uint16_t start;
	uint16_t count;
} vgpu_root_layout_range_t;

typedef struct vgpu_root_layout_slot_s
{
	vgpu_root_slot_type_t type;
	union
	{
		struct  
		{
			vgpu_root_layout_range_t range_buffers;
			vgpu_root_layout_range_t range_constant_buffers;
		} table;
		struct
		{
			uint16_t location;
			vgpu_resource_type_t type;
			bool treat_as_constant_buffer;
		} resource;
	};
} vgpu_root_layout_slot_t;

typedef struct vgpu_blend_s
{
	bool enabled;

	vgpu_blend_elem_t color_src;
	vgpu_blend_elem_t color_dst;
	vgpu_blend_op_t color_op;

	vgpu_blend_elem_t alpha_src;
	vgpu_blend_elem_t alpha_dst;
	vgpu_blend_op_t alpha_op;
} vgpu_blend_t;

typedef struct vgpu_depth_s
{
	bool enabled;
	vgpu_compare_func_t func;
} vgpu_depth_t;

typedef struct vgpu_stencil_s
{
	bool enabled;

	struct
	{
		vgpu_compare_func_t func;
		vgpu_stencil_op_t fail_op;
		vgpu_stencil_op_t depth_fail_op;
		vgpu_stencil_op_t pass_op;
	} front, back;

	uint8_t read_mask;
	uint8_t write_mask;
} vgpu_stencil_t;

typedef struct vgpu_state_s
{
	vgpu_fill_mode_t fill;
	vgpu_winding_order_t wind;
	vgpu_cull_mode_t cull;
	uint32_t depth_bias;

	bool blend_independent;
	vgpu_blend_t blend[VGPU_MAX_RENDER_TARGETS];
	vgpu_depth_t depth;
	vgpu_stencil_t stencil;
} vgpu_state_t;

typedef struct vgpu_create_program_params_s
{
	const uint8_t* data;
	size_t size;
	vgpu_program_type_t program_type;
} vgpu_create_program_params_t;

typedef struct vgpu_create_pipeline_params_s
{
	vgpu_root_layout_t* root_layout;
	vgpu_render_pass_t* render_pass;

	vgpu_program_t* vertex_program;
	vgpu_program_t* fragment_program;

	vgpu_state_t state;

	vgpu_primitive_type_t primitive_type;
} vgpu_create_pipeline_params_t;

typedef struct vgpu_resource_table_entry_s
{
	uint8_t location;
	vgpu_resource_type_t type;
	void* resource;
	size_t offset;
	size_t num_bytes;
	bool treat_as_constant_buffer;
} vgpu_resource_table_entry_t;

typedef enum
{
	VGPU_BUFFER_FLAG_INDEX_BUFFER = 0x1,
	VGPU_BUFFER_FLAG_CONSTANT_BUFFER = 0x2,
} vgpu_buffer_flag_t;

typedef struct vgpu_create_buffer_params_s
{
	size_t num_bytes;
	vgpu_usage_t usage;
	uint32_t flags;
	uint32_t structure_stride;

	const char* name;
} vgpu_create_buffer_params_t;

typedef struct vgpu_lock_buffer_params_s
{
	vgpu_buffer_t* buffer;
	size_t offset;
	size_t num_bytes;

	uintptr_t unlock_data[4];
} vgpu_lock_buffer_params_t;

typedef union vgpu_clear_value_s
{
	struct
	{
		float r, g, b, a;
	};

	float color[4];

	struct
	{
		float depth;
		uint8_t stencil;
	} depth_stencil;
} vgpu_clear_value_t;

typedef struct vgpu_create_texture_params_s
{
	vgpu_texture_type_t type;
	vgpu_texture_format_t format;
	vgpu_usage_t usage; 

	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t num_mips;

	uint32_t is_render_target;

	const char* name;

	vgpu_clear_value_t clear_value;
} vgpu_create_texture_params_t;

typedef struct vgpu_render_pass_target_param_s
{
	vgpu_texture_t* texture;
	uint32_t clear_on_bind; // TODO: more fine grained begin/end operations
} vgpu_render_pass_target_param_t;

typedef struct vgpu_create_render_pass_params_s
{
	size_t num_color_targets;
	vgpu_render_pass_target_param_t color_targets[16];
	vgpu_render_pass_target_param_t depth_stencil_target;
} vgpu_create_render_pass_params_t;

/******************************************************************************\
*
*  Device operations
*
\******************************************************************************/

vgpu_device_t* vgpu_create_device(const vgpu_create_device_params_t* params);

void vgpu_destroy_device(vgpu_device_t* device);

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device);

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue = 0);

void vgpu_present(vgpu_device_t* device);

vgpu_texture_t* vgpu_get_back_buffer(vgpu_device_t* device);

uint64_t vgpu_get_frame_no(vgpu_device_t* device);

uint64_t vgpu_get_frame_id(vgpu_device_t* device);

uint64_t vgpu_max_buffered_frames(vgpu_device_t* device);

void vgpu_get_caps(vgpu_device_t* device, vgpu_caps_t* out_caps);

/******************************************************************************\
*
*  Thread context handling
*
\******************************************************************************/

vgpu_thread_context_t* vgpu_create_thread_context(vgpu_device_t* device, const vgpu_create_thread_context_params_t* params);

void vgpu_destroy_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context);

void vgpu_prepare_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context);

/******************************************************************************\
*
*  Buffer handling
*
\******************************************************************************/

vgpu_buffer_t* vgpu_create_buffer(vgpu_device_t* device, const vgpu_create_buffer_params_t* params);

void vgpu_destroy_buffer(vgpu_device_t* device, vgpu_buffer_t* buffer);

/******************************************************************************\
*
*  Resource table handling
*
\******************************************************************************/

vgpu_resource_table_t* vgpu_create_resource_table(vgpu_device_t* device, const vgpu_root_layout_t* root_layout, uint32_t root_slot, const vgpu_resource_table_entry_t* entries, size_t num_entries);

void vgpu_destroy_resource_table(vgpu_device_t* device, vgpu_resource_table_t* resource_table);

/******************************************************************************\
*
*  Root layout handling
*
\******************************************************************************/

vgpu_root_layout_t* vgpu_create_root_layout(vgpu_device_t* device, const vgpu_root_layout_slot_t* slots, size_t num_slots);

void vgpu_destroy_root_layout(vgpu_device_t* device, vgpu_root_layout_t* root_layout);

/******************************************************************************\
*
*  Texture handling
*
\******************************************************************************/

vgpu_texture_t* vgpu_create_texture(vgpu_device_t* device, const vgpu_create_texture_params_t* params);

void vgpu_destroy_texture(vgpu_device_t* device, vgpu_texture_t* texture);

/******************************************************************************\
*
*  Program handling
*
\******************************************************************************/

vgpu_program_t* vgpu_create_program(vgpu_device_t* device, const vgpu_create_program_params_t* params);

void vgpu_destroy_program(vgpu_device_t* device, vgpu_program_t* program);

/******************************************************************************\
*
*  Pipeline handling
*
\******************************************************************************/

vgpu_pipeline_t* vgpu_create_pipeline(vgpu_device_t* device, const vgpu_create_pipeline_params_t* params);

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline);

/******************************************************************************\
*
*  Render pass handling
*
\******************************************************************************/

vgpu_render_pass_t* vgpu_create_render_pass(vgpu_device_t* device, const vgpu_create_render_pass_params_t* params);

void vgpu_destroy_render_pass(vgpu_device_t* device, vgpu_render_pass_t* render_pass);

/******************************************************************************\
*
*  Command list handling
*
\******************************************************************************/

vgpu_command_list_t* vgpu_create_command_list(vgpu_device_t* device, const vgpu_create_command_list_params_t* params);

void vgpu_destroy_command_list(vgpu_device_t* device, vgpu_command_list_t* command_list);

bool vgpu_is_command_list_type_supported(vgpu_device_t* device, vgpu_command_list_type_t command_list_type);

/******************************************************************************\
*
*  Command list building
*
\******************************************************************************/

void vgpu_begin_command_list(vgpu_thread_context_t* thread_context, vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass);

void vgpu_end_command_list(vgpu_command_list_t* command_list);

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params);

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params);

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes);

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table);

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes);

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline);

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer);

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices);

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex);

void vgpu_draw_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint32_t count);

void vgpu_draw_indexed_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint32_t count);

void vgpu_blit(vgpu_command_list_t* command_list, vgpu_texture_t* texture);

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass);

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass);

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after);

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after);

#ifdef __cplusplus
}
#endif

#endif // VGPU_H
