#include <stdio.h>

#include "vgpu_gl.h"

/******************************************************************************\
 *
 *  Translation utils
 *
\******************************************************************************/
static const GLenum translate_program_type[] = {
	GL_VERTEX_SHADER,
	GL_FRAGMENT_SHADER,
};

static const GLenum translate_data_type[] = {
	GL_FLOAT,
	GL_UNSIGNED_INT,
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_BYTE,
};

static const size_t translate_data_type_size[] = {
	sizeof(float),
	sizeof(uint8_t),
	sizeof(uint16_t),
	sizeof(uint32_t),
};

static const GLenum translate_primitive_type[] = {
	GL_TRIANGLES,
	GL_LINES,
};

static const GLenum translate_fill_mode[] = {
	GL_FILL,
	GL_LINE,
};

static const GLenum translate_cull_mode[] = {
	0,
	GL_FRONT,
	GL_BACK,
};

static const GLenum translate_winding_order[] = {
	GL_CCW,
	GL_CW,
};

static const GLenum translate_blend_elem[] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA_SATURATE,
	GL_CONSTANT_COLOR, // TODO: what about alpha?
	GL_ONE_MINUS_CONSTANT_COLOR, // TODO: what about alpha?
	GL_SRC1_COLOR,
	GL_ONE_MINUS_SRC1_COLOR,
	GL_SRC1_ALPHA,
	GL_ONE_MINUS_SRC1_ALPHA,
};

static const GLenum translate_blend_op[] = {
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX,
};

static const GLenum translate_depth_func[] = {
	GL_ALWAYS,
	GL_LESS,
	GL_LEQUAL,
	GL_EQUAL,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_GREATER,
	GL_NEVER,
};

static const GLenum translate_compare_func[] = {
	GL_ALWAYS,
	GL_LESS,
	GL_LEQUAL,
	GL_EQUAL,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_GREATER,
	GL_NEVER,
};

static const GLenum translate_stencil_op[] = {
	GL_KEEP,
	GL_ZERO,
	GL_REPLACE,
	GL_INCR,
	GL_DECR,
	GL_INVERT,
	GL_INCR_WRAP,
	GL_DECR_WRAP,
};

static int vgpu_gl_check_errors(vgpu_glc_t* glc)
{
	int wasHit = 0;
	const unsigned char *str;

	while(1)
	{
		int e = glc->glGetError();

		if(e == GL_NO_ERROR)
			break;

		str = gluErrorString(e);
#if defined(VGPU_WINDOWS)
		OutputDebugStringA((LPCSTR)str);
		OutputDebugStringA("\n");
#endif
		printf("%s\n", str);

		wasHit = 1;
	}

	return wasHit;
}

#define GLERR_CHECK(glc) vgpu_gl_check_errors(glc)

static void APIENTRY vgpu_gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	vgpu_device_t* device = (vgpu_device_t*)userParam;
	if(device->log_func)
		device->log_func(message);
}

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

	device->width = 1280;
	device->height = 720;
	device->frame_no = 0;
	memset(&device->caps, 0, sizeof(device->caps));
	device->caps.flags = (~params->force_disable_flags) & (VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET | VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET);

	vgpu_platform_create_device(device, params);

	vgpu_glc_t* glc = &device->glc;
	vgpu_platform_load_gl_ptrs(glc);
	device->immediate_command_list = nullptr;

	glc->glDebugMessageCallback(vgpu_gl_debug_callback, device);
	GLERR_CHECK(glc);

	glc->glViewport(0, 0, device->width, device->height);
	GLERR_CHECK(glc);

	glc->glGenVertexArrays(1, &device->vao_gl_id);
	GLERR_CHECK(glc);

	glc->glBindVertexArray(device->vao_gl_id);

	device->backbuffer.gl_id = 0;
	device->backbuffer.type = VGPU_TEXTURETYPE_2D;
	device->backbuffer.width = device->width;
	device->backbuffer.height = device->height;
	device->backbuffer.depth = 1;
	device->backbuffer.num_mips = 1;
	device->backbuffer.clear_value.r = 0.1f;
	device->backbuffer.clear_value.g = 0.1f;
	device->backbuffer.clear_value.b = 0.3f;
	device->backbuffer.clear_value.a = 1.0f;

	return device;
}

void vgpu_destroy_device(vgpu_device_t* device)
{
	vgpu_glc_t* glc = &device->glc;
	GLERR_CHECK(glc);

	glc->glDeleteVertexArrays(1, &device->vao_gl_id);
	GLERR_CHECK(glc);

	vgpu_platform_destroy_device(device);

	VGPU_FREE(device->allocator, device);
}

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device)
{
	return VGPU_DEVICE_GL;
}

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue)
{
}

void vgpu_present(vgpu_device_t* device)
{
	vgpu_platform_swap(device);

	device->frame_no++;
}

vgpu_texture_t* vgpu_get_back_buffer(vgpu_device_t* device)
{
	return &device->backbuffer;
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
	vgpu_glc_t* glc = &device->glc;
	vgpu_buffer_t* buffer = VGPU_ALLOC_TYPE(device->allocator, vgpu_buffer_t);

	glc->glCreateBuffers(1, &buffer->gl_id);
	GLERR_CHECK(glc);

	GLbitfield flags = GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT; // TODO: remove this and have an upload buffer for updating non dynamic buffers
	glc->glNamedBufferStorage(buffer->gl_id, params->num_bytes, nullptr, flags);
	GLERR_CHECK(glc);

	buffer->num_bytes = params->num_bytes;

	return buffer;
}

void vgpu_destroy_buffer(vgpu_device_t* device, vgpu_buffer_t* buffer)
{
	vgpu_glc_t* glc = &device->glc;

	glc->glDeleteBuffers(1, &buffer->gl_id);
	GLERR_CHECK(glc);

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
	memset(resource_table, 0, sizeof(*resource_table));
	resource_table->num_entries = num_entries;
	memcpy(resource_table->entries, entries, num_entries * sizeof(vgpu_resource_table_entry_t));
	// TODO: validate slots are in range of root layout

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
	memset(root_layout, 0, sizeof(*root_layout));
	memcpy(root_layout, slots, num_slots * sizeof(*slots));

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
	vgpu_glc_t* glc = &device->glc;
	vgpu_texture_t* texture = VGPU_ALLOC_TYPE(device->allocator, vgpu_texture_t);

	VGPU_ASSERT(device, params->type == VGPU_TEXTURETYPE_2D, "Type must be 2D for now\n");

	texture->width = params->width;
	texture->height = params->height;
	texture->depth = params->depth;
	texture->num_mips = params->num_mips;

	// TODO: use NamedTexture etc.
	glc->glGenTextures(1, &texture->gl_id);
	glc->glBindTexture(GL_TEXTURE_2D, texture->gl_id);
	glc->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture->width, texture->height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
	glc->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glc->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glc->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glc->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glc->glBindTexture(GL_TEXTURE_2D, 0);
	GLERR_CHECK(glc);

	texture->clear_value = params->clear_value;

	return texture;
}

void vgpu_destroy_texture(vgpu_device_t* device, vgpu_texture_t* texture)
{
	vgpu_glc_t* glc = &device->glc;
	glc->glDeleteTextures(1, &texture->gl_id);

	VGPU_FREE(device->allocator, texture);
}

/******************************************************************************\
*
*  Program handling
*
\******************************************************************************/

static void vgpu_gl_check_for_shader_errors(vgpu_glc_t* glc, GLuint shader)
{
	GLint result;
	glc->glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result != GL_TRUE)
	{
		char buffer[1024 * 4];
		GLsizei length = 0;

		printf("Program: failed to compile\n");

		glc->glGetShaderInfoLog(shader, 1024 * 4 - 1, &length, buffer);
		buffer[length] = 0;
#if defined(VGPU_WINDOWS)
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
#endif
		printf("Program: %s\n", buffer);
	}
}

vgpu_program_t* vgpu_create_program(vgpu_device_t* device, const vgpu_create_program_params_t* params)
{
	vgpu_program_t* program = VGPU_ALLOC_TYPE(device->allocator, vgpu_program_t);

	vgpu_glc_t* glc = &device->glc;

	GLenum type = translate_program_type[params->program_type];

	GLERR_CHECK(glc);
	program->gl_id = glc->glCreateShader(type);
	GLERR_CHECK(glc);
	const GLchar* data = (const GLchar*)params->data;
	GLint size = params->size;
	glc->glShaderSource(program->gl_id, 1, &data, &size);
	GLERR_CHECK(glc);
	glc->glCompileShader(program->gl_id);
	GLERR_CHECK(glc);
	vgpu_gl_check_for_shader_errors(glc, program->gl_id);
	GLERR_CHECK(glc);

	program->program_type = params->program_type;

	return program;
}

void vgpu_destroy_program(vgpu_device_t* device, vgpu_program_t* program)
{
	vgpu_glc_t* glc = &device->glc;
	glc->glDeleteShader(program->gl_id);
	VGPU_FREE(device->allocator, program);
}

/******************************************************************************\
 *
 *  Pipeline handling
 *
\******************************************************************************/

static void vgpu_gl_check_for_program_errors(vgpu_glc_t* glc, GLuint program)
{
	GLint result;
	glc->glGetProgramiv(program, GL_LINK_STATUS, &result);
	if(result != GL_TRUE)
	{
		char buffer[1024*4];
		GLsizei length = 0;

		printf("Pipeline: failed to link\n");

		glc->glGetProgramInfoLog(program, 1024*4-1, &length, buffer);
		buffer[length] = 0;
#if defined(VGPU_WINDOWS)
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
#endif
		printf("Pipeline: %s\n", buffer);
	}
}

vgpu_pipeline_t* vgpu_create_pipeline(vgpu_device_t* device, const vgpu_create_pipeline_params_t* params)
{
	vgpu_glc_t* glc = &device->glc;
	vgpu_pipeline_t* pipeline = VGPU_ALLOC_TYPE(device->allocator, vgpu_pipeline_t);

	pipeline->gl_id = glc->glCreateProgram();

	glc->glAttachShader(pipeline->gl_id, params->vertex_program->gl_id);
	if(params->fragment_program)
		glc->glAttachShader(pipeline->gl_id, params->fragment_program->gl_id);
	glc->glLinkProgram(pipeline->gl_id);
	vgpu_gl_check_for_program_errors(glc, pipeline->gl_id);

	pipeline->prim_type = translate_primitive_type[params->primitive_type];

	pipeline->polygon.mode = translate_fill_mode[params->state.fill];
	pipeline->polygon.front_face = translate_winding_order[params->state.wind];
	pipeline->polygon.cull_face_enable = (params->state.cull != VGPU_CULL_NONE);
	pipeline->polygon.cull_face = translate_cull_mode[params->state.cull];
	pipeline->polygon.offset_enable = (params->state.depth_bias != 0);
	pipeline->polygon.offset_factor = (float)params->state.depth_bias;
	pipeline->polygon.offset_units = 0.0f; // TODO: ???

	int max_blend = params->state.blend_independent ? VGPU_MAX_RENDER_TARGETS-1 : 0;
	for(int i = 0; i < VGPU_MAX_RENDER_TARGETS; ++i)
	{
		int b = i > max_blend ? max_blend : i;

		pipeline->blend[i].enabled = params->state.blend[i].enabled;

		pipeline->blend[i].src_color_func = translate_blend_elem[params->state.blend[i].color_src];
		pipeline->blend[i].dst_color_func = translate_blend_elem[params->state.blend[i].color_dst];
		pipeline->blend[i].color_equation = translate_blend_op[params->state.blend[i].color_op];

		pipeline->blend[i].src_alpha_func = translate_blend_elem[params->state.blend[i].alpha_src];
		pipeline->blend[i].dst_alpha_func = translate_blend_elem[params->state.blend[i].alpha_dst];
		pipeline->blend[i].alpha_equation = translate_blend_op[params->state.blend[i].alpha_op];
	}

	pipeline->depth_test.enabled = params->state.depth.enabled;
	pipeline->depth_test.func = translate_compare_func[params->state.depth.func];

	pipeline->stencil_test.enabled = params->state.stencil.enabled;

	pipeline->stencil_test.front.func = translate_compare_func[params->state.stencil.front.func];
	pipeline->stencil_test.front.fail_op = translate_stencil_op[params->state.stencil.front.fail_op];
	pipeline->stencil_test.front.depth_fail_op = translate_stencil_op[params->state.stencil.front.depth_fail_op];
	pipeline->stencil_test.front.pass_op = translate_stencil_op[params->state.stencil.front.pass_op];
	pipeline->stencil_test.front.ref = 0; // TODO: ???
	pipeline->stencil_test.front.mask = params->state.stencil.read_mask; // TODO: ???

	pipeline->stencil_test.back.func = translate_compare_func[params->state.stencil.back.func];
	pipeline->stencil_test.back.fail_op = translate_stencil_op[params->state.stencil.back.fail_op];
	pipeline->stencil_test.back.depth_fail_op = translate_stencil_op[params->state.stencil.back.depth_fail_op];
	pipeline->stencil_test.back.pass_op = translate_stencil_op[params->state.stencil.back.pass_op];
	pipeline->stencil_test.back.ref = 0; // TODO: ???
	pipeline->stencil_test.back.mask = params->state.stencil.read_mask; // TODO: ???

	pipeline->root_layout = params->root_layout;

	return pipeline;
}

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline)
{
	vgpu_glc_t* glc = &device->glc;
	glc->glDeleteProgram(pipeline->gl_id);
	VGPU_FREE(device->allocator, pipeline);
}

/******************************************************************************\
*
*  Render setup handling
*
\******************************************************************************/

vgpu_render_pass_t* vgpu_create_render_pass(vgpu_device_t* device, const vgpu_create_render_pass_params_t* params)
{
	vgpu_render_pass_t* render_pass = VGPU_ALLOC_TYPE(device->allocator, vgpu_render_pass_t);
	memset(render_pass, 0, sizeof(*render_pass));
	render_pass->num_color_targets = params->num_color_targets;

	for (size_t i = 0; i < params->num_color_targets; ++i)
	{
		render_pass->color_clear_value[i] = params->color_targets[i].texture->clear_value;
	}

	if (params->depth_stencil_target.texture)
	{
		render_pass->depth_stencil_clear_value = params->depth_stencil_target.texture->clear_value;
	}

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
	VGPU_ASSERT(device, params->type == VGPU_COMMAND_LIST_IMMEDIATE_GRAPHICS, "Only immediate graphics command lists supported on OpenGL");
	VGPU_ASSERT(device, device->immediate_command_list == nullptr, "An immediate graphics command list has already been created");

	vgpu_command_list_t* command_list = VGPU_ALLOC_TYPE(device->allocator, vgpu_command_list_t);
	command_list->device = device;
	command_list->glc = &device->glc;
	command_list->curr_pipeline = nullptr;
	command_list->curr_root_layout = nullptr;
	command_list->curr_render_pass = nullptr;
	command_list->curr_index_type = 0;
	command_list->curr_index_size = 0;

	device->immediate_command_list = command_list;

	return command_list;
}

void vgpu_destroy_command_list(vgpu_device_t* device, vgpu_command_list_t* command_list)
{
	device->immediate_command_list = nullptr;
	VGPU_FREE(device->allocator, command_list);
}

bool vgpu_is_command_list_type_supported(vgpu_device_t* device, vgpu_command_list_type_t command_list_type)
{
	return command_list_type == VGPU_COMMAND_LIST_IMMEDIATE_GRAPHICS;
}

/******************************************************************************\
*
*  Command list building
*
\******************************************************************************/

void vgpu_begin_command_list(vgpu_thread_context_t* thread_context, vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	command_list->curr_pipeline = nullptr;
	command_list->curr_root_layout = nullptr;
	command_list->curr_render_pass = nullptr;
}

void vgpu_end_command_list(vgpu_command_list_t* command_list)
{
}

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params)
{
	vgpu_glc_t* glc = command_list->glc;

	void* ptr = glc->glMapNamedBufferRange(params->buffer->gl_id, params->offset, params->num_bytes, GL_MAP_WRITE_BIT);
	GLERR_CHECK(glc);

	return ptr;
}

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params)
{
	vgpu_glc_t* glc = command_list->glc;

	glc->glUnmapNamedBuffer(params->buffer->gl_id);
	GLERR_CHECK(glc);
}

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes)
{
	vgpu_glc_t* glc = command_list->glc;

	glc->glNamedBufferSubData(buffer->gl_id, offset, num_bytes, data);
	GLERR_CHECK(glc);
}

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table)
{
	vgpu_glc_t* glc = command_list->glc;

	vgpu_pipeline_t* pipeline = command_list->curr_pipeline;
	for(size_t i = 0; i < resource_table->num_entries; ++i)
	{
		vgpu_resource_table_entry_t* entry = &resource_table->entries[i];
		switch (entry->type)
		{
			case VGPU_RESOURCE_TEXTURE:
			case VGPU_RESOURCE_SAMPLER:
			case VGPU_RESOURCE_NONE:
				break;
			case VGPU_RESOURCE_BUFFER:
				{
					vgpu_buffer_t* buffer = (vgpu_buffer_t*)entry->resource;
					VGPU_ASSERT(command_list->device, buffer->num_bytes >= (entry->offset + entry->num_bytes), "Bind range exceeds buffer range");
					glc->glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
						(GLint)entry->location,
						buffer->gl_id,
						entry->offset,
						entry->num_bytes);
					GLERR_CHECK(glc);
					break;
				}
			default:
				break;
		}
	}
}

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes)
{
	vgpu_glc_t* glc = command_list->glc;
	glc->glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
		(GLint)command_list->curr_root_layout->slots[slot].resource.location,
		buffer->gl_id,
		offset,
		num_bytes);
	GLERR_CHECK(glc);
}

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline)
{
	vgpu_glc_t* glc = command_list->glc;

	glc->glUseProgram(pipeline->gl_id);

	glc->glPolygonMode(GL_FRONT_AND_BACK, pipeline->polygon.mode);
	GLERR_CHECK(glc);
	glc->glFrontFace(pipeline->polygon.front_face);
	GLERR_CHECK(glc);
	if(pipeline->polygon.cull_face_enable)
	{
		glc->glEnable(GL_CULL_FACE);
		glc->glCullFace(pipeline->polygon.cull_face);
		GLERR_CHECK(glc);
	}
	else
	{
		glc->glDisable(GL_CULL_FACE);
		GLERR_CHECK(glc);
	}

	if(pipeline->polygon.offset_enable)
	{
		glc->glEnable(GL_POLYGON_OFFSET_FILL); // TODO: other fill modes
		glc->glPolygonOffset(
				pipeline->polygon.offset_factor,
				pipeline->polygon.offset_units);
		GLERR_CHECK(glc);
	}
	else
	{
		glc->glDisable(GL_POLYGON_OFFSET_FILL); // TODO: other fill modes
		GLERR_CHECK(glc);
	}

	// blending
	for(int i = 0; i < VGPU_MAX_RENDER_TARGETS; ++i)
	{
		if(pipeline->blend[0].enabled)
		{
			glc->glEnablei(GL_BLEND, i);
			glc->glBlendEquationSeparatei(i,
					pipeline->blend[i].color_equation,
					pipeline->blend[i].alpha_equation);
			glc->glBlendFuncSeparatei(i,
					pipeline->blend[i].src_color_func,
					pipeline->blend[i].dst_color_func,
					pipeline->blend[i].src_alpha_func,
					pipeline->blend[i].dst_alpha_func);
			GLERR_CHECK(glc);
		}
		else
		{
			glc->glDisablei(GL_BLEND, i);
			GLERR_CHECK(glc);
		}
	}

	// depth
	if(pipeline->depth_test.enabled)
	{
		glc->glEnable(GL_DEPTH_TEST);
		glc->glDepthFunc(pipeline->depth_test.func);
		GLERR_CHECK(glc);
	}
	else
	{
		glc->glDisable(GL_DEPTH_TEST);
		GLERR_CHECK(glc);
	}

	// stencil
	if(pipeline->stencil_test.enabled)
	{
		glc->glEnable(GL_STENCIL_TEST);
		glc->glStencilFuncSeparate(GL_FRONT,
				pipeline->stencil_test.front.func,
				pipeline->stencil_test.front.ref,
				pipeline->stencil_test.front.mask);
		glc->glStencilOpSeparate(GL_FRONT,
				pipeline->stencil_test.front.fail_op,
				pipeline->stencil_test.front.depth_fail_op,
				pipeline->stencil_test.front.pass_op);
		glc->glStencilFuncSeparate(GL_BACK,
				pipeline->stencil_test.back.func,
				pipeline->stencil_test.back.ref,
				pipeline->stencil_test.back.mask);
		glc->glStencilOpSeparate(GL_BACK,
				pipeline->stencil_test.back.fail_op,
				pipeline->stencil_test.back.depth_fail_op,
				pipeline->stencil_test.back.pass_op);
		GLERR_CHECK(glc);
	}
	else
	{
		glc->glDisable(GL_STENCIL_TEST);
		GLERR_CHECK(glc);
	}

	command_list->curr_pipeline = pipeline;
	command_list->curr_root_layout = pipeline->root_layout;
}

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer)
{
	vgpu_glc_t* glc = command_list->glc;
	glc->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->gl_id);
	GLERR_CHECK(glc);

	command_list->curr_index_type = translate_data_type[index_type];
	command_list->curr_index_size = translate_data_type_size[index_type];
}

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices)
{
	vgpu_glc_t* glc = command_list->glc;
	vgpu_pipeline_s* pipeline = command_list->curr_pipeline;

	glc->glDrawArraysInstanced(pipeline->prim_type,
			first_vertex,
			num_vertices,
			num_instances);
	GLERR_CHECK(glc);
}

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex)
{
	vgpu_glc_t* glc = command_list->glc;
	vgpu_pipeline_s* pipeline = command_list->curr_pipeline;

	glc->glDrawElementsInstanced(pipeline->prim_type,
			num_indices,
			command_list->curr_index_type,
			(char*)0 + first_index*command_list->curr_index_size,
			num_instances);
	GLERR_CHECK(glc);
}

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	vgpu_glc_t* glc = command_list->glc;

	// TODO: set up the FBO
	glc->glClearColor(
		render_pass->color_clear_value[0].r,
		render_pass->color_clear_value[0].g,
		render_pass->color_clear_value[0].b,
		render_pass->color_clear_value[0].a);
	glc->glClearDepth(render_pass->depth_stencil_clear_value.depth_stencil.depth);
	glc->glClearStencil(render_pass->depth_stencil_clear_value.depth_stencil.stencil);
	glc->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	GLERR_CHECK(glc);
}

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	vgpu_glc_t* glc = command_list->glc;

	command_list->curr_render_pass = render_pass;
	// TODO: set up the FBO

	vgpu_clear_render_pass(command_list, render_pass);
}

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}
