#include <vgpu.h>
#include "vgpu_internal.h"
#include <windows.h>
#include <d3d11_1.h>

#define SAFE_RELEASE(x) if(x) (x)->Release()

/******************************************************************************\
 *
 *  Translation utils
 *
\******************************************************************************/

static const D3D11_USAGE translate_usage[] = {
	D3D11_USAGE_DEFAULT,
	D3D11_USAGE_DYNAMIC,
};

static const DXGI_FORMAT translate_textureformat[] = {
	DXGI_FORMAT_B8G8R8A8_UNORM,
	DXGI_FORMAT_BC1_UNORM,
	DXGI_FORMAT_BC2_UNORM,
	DXGI_FORMAT_BC3_UNORM,

	DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
};

static const D3D11_PRIMITIVE_TOPOLOGY translate_primitive_type[] = {
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
};

static const D3D11_FILL_MODE translate_fill_mode[] = {
	D3D11_FILL_SOLID,
	D3D11_FILL_WIREFRAME,
};

static const D3D11_CULL_MODE translate_cull_mode[] = {
	D3D11_CULL_NONE,
	D3D11_CULL_FRONT,
	D3D11_CULL_BACK,
};

static const D3D11_BLEND translate_blend_elem[] = {
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA_SAT,
	D3D11_BLEND_BLEND_FACTOR,
	D3D11_BLEND_INV_BLEND_FACTOR,
	D3D11_BLEND_SRC1_COLOR,
	D3D11_BLEND_INV_SRC1_COLOR,
	D3D11_BLEND_SRC1_ALPHA,
	D3D11_BLEND_INV_SRC1_ALPHA,
};

static const D3D11_BLEND_OP translate_blend_op[] = {
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX,
};

static const D3D11_COMPARISON_FUNC translate_compare_func[] = {
	D3D11_COMPARISON_ALWAYS,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NEVER,
};

static const D3D11_STENCIL_OP translate_stencil_op[] = {
	D3D11_STENCIL_OP_KEEP,
	D3D11_STENCIL_OP_ZERO,
	D3D11_STENCIL_OP_REPLACE,
	D3D11_STENCIL_OP_INCR_SAT,
	D3D11_STENCIL_OP_DECR_SAT,
	D3D11_STENCIL_OP_INVERT,
	D3D11_STENCIL_OP_INCR,
	D3D11_STENCIL_OP_DECR,
};

/******************************************************************************\
 *
 *  Structures
 *
\******************************************************************************/

struct vgpu_buffer_s
{
	ID3D11Buffer* buffer;
	ID3D11ShaderResourceView* view;
	size_t stride;
	D3D11_USAGE usage;
	uint32_t flags;
};

#define RESOURCE_TABLE_MAX_SLOTS 16
struct vgpu_resource_table_s
{
	size_t num_entries;
	vgpu_resource_table_entry_t entries[RESOURCE_TABLE_MAX_SLOTS];
	ID3D11ShaderResourceView* views[RESOURCE_TABLE_MAX_SLOTS];
	ID3D11Buffer* constant_buffers[RESOURCE_TABLE_MAX_SLOTS];
};

struct vgpu_root_layout_s
{
	vgpu_root_layout_slot_t slots[VGPU_MAX_ROOT_SLOTS];
};

struct vgpu_texture_s
{
	ID3D11Texture2D* texture2d;
	
	vgpu_clear_value_t clear_value;
};

struct vgpu_program_s
{
	vgpu_program_type_t program_type;
	union
	{
		ID3D11VertexShader* vertex_shader;
		ID3D11PixelShader* pixel_shader;
	};
};

struct vgpu_pipeline_s
{
	vgpu_program_t* vertex_program;
	vgpu_program_t* fragment_program;

	D3D11_PRIMITIVE_TOPOLOGY prim_type;

	ID3D11BlendState* blend_state;
	ID3D11DepthStencilState* depth_stencil_state;
	ID3D11RasterizerState* rasterizer_state;

	vgpu_root_layout_t* root_layout;
};

struct vgpu_render_pass_s
{
	ID3D11RenderTargetView* rtv[VGPU_MAX_RENDER_TARGETS];
	vgpu_clear_value_t rtv_clear_value[VGPU_MAX_RENDER_TARGETS];
	size_t num_rtv;

	ID3D11DepthStencilView* dsv;
	vgpu_clear_value_t dsv_clear_value;
};

struct vgpu_command_list_s
{
	vgpu_device_t* device;
	ID3D11DeviceContext1* d3dc1;
	ID3D11DeviceContext* d3dc;

	vgpu_pipeline_t* curr_pipeline;
	vgpu_root_layout_t* curr_root_layout;
};

struct vgpu_thread_context_s
{
};

struct vgpu_device_s
{
	vgpu_allocator_t* allocator;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;

	uint32_t width;
	uint32_t height;
	uint64_t frame_no;

	IDXGISwapChain* swapchain;
	ID3D11Device* d3dd;
	ID3D11DeviceContext* d3dc;
	ID3D11DeviceContext1* d3dc1;
	
	vgpu_texture_t backbuffer;

	vgpu_command_list_t* immediate_command_list;

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

	device->width = 1280;
	device->height = 720;
	device->frame_no = 0;
	device->immediate_command_list = nullptr;
	ZeroMemory(&device->caps, sizeof(device->caps));

    DXGI_SWAP_CHAIN_DESC scd;

	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = (HWND)params->window;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

	D3D_FEATURE_LEVEL wanted_feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
			NULL,	// adapter
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,	// software rasterizer
			0, //D3D11_CREATE_DEVICE_DEBUG,
			wanted_feature_levels,
			ARRAYSIZE(wanted_feature_levels),
			D3D11_SDK_VERSION,
			&scd,
			&device->swapchain,
			&device->d3dd,
			NULL, // feature level selected
			&device->d3dc);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create device and swapchain");

	if ((params->force_disable_flags & VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET) == 0)
	{
		hr = device->d3dc->QueryInterface(IID_PPV_ARGS(&device->d3dc1));
		if (SUCCEEDED(hr))
		{
			device->caps.flags |= VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET;
		}
	}

	hr = device->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&device->backbuffer.texture2d);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to get swapchain buffer");
	device->backbuffer.clear_value.r = 0.1f;
	device->backbuffer.clear_value.g = 0.1f;
	device->backbuffer.clear_value.b = 0.3f;
	device->backbuffer.clear_value.a = 1.0f;

    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(device->width);
    vp.Height = static_cast<float>(device->height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    device->d3dc->RSSetViewports(1, &vp);

	return device;
}

void vgpu_destroy_device(vgpu_device_t* device)
{
	SAFE_RELEASE(device->backbuffer.texture2d);
	SAFE_RELEASE(device->swapchain);
	SAFE_RELEASE(device->d3dc);
	SAFE_RELEASE(device->d3dd);

	VGPU_FREE(device->allocator, device);
}

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device)
{
	return VGPU_DEVICE_DX11;
}

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue)
{
}

void vgpu_present(vgpu_device_t* device)
{
	device->swapchain->Present(0, 0);
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
	vgpu_buffer_t* buffer = VGPU_ALLOC_TYPE(device->allocator, vgpu_buffer_t);
	buffer->usage = translate_usage[params->usage];
	buffer->flags = params->flags;

	D3D11_BUFFER_DESC buffer_desc;
	buffer_desc.Usage               = buffer->usage;
	buffer_desc.ByteWidth           = params->num_bytes;
	buffer_desc.BindFlags           = 0;
	buffer_desc.CPUAccessFlags      = buffer->usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;
	buffer_desc.MiscFlags           = 0;
	buffer_desc.StructureByteStride = 0;

	if (params->flags & VGPU_BUFFER_FLAG_INDEX_BUFFER)
	{
		buffer_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
		buffer->stride = 0;
	}
	else if (params->flags & VGPU_BUFFER_FLAG_CONSTANT_BUFFER)
	{
		buffer_desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
		buffer->stride = 0;
	}
	else
	{
		buffer_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		buffer_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = params->structure_stride;
		buffer->stride = params->structure_stride;
	}

	HRESULT hr = device->d3dd->CreateBuffer(
			&buffer_desc,
			NULL,
			&buffer->buffer);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create buffer");

	buffer->view = nullptr;
	if (buffer_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
		view_desc.Format = DXGI_FORMAT_UNKNOWN;
		view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		view_desc.BufferEx.FirstElement = 0;
		view_desc.BufferEx.NumElements = params->num_bytes / buffer->stride;
		view_desc.BufferEx.Flags = 0;
		hr = device->d3dd->CreateShaderResourceView(buffer->buffer, &view_desc, &buffer->view);
		VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create SRV for buffer");
	}

	return buffer;
}

void vgpu_destroy_buffer(vgpu_device_t* device, vgpu_buffer_t* buffer)
{
	SAFE_RELEASE(buffer->view);
	SAFE_RELEASE(buffer->buffer);
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

	for(size_t i = 0; i < num_entries; ++i)
	{
		switch(entries[i].type)
		{
			case VGPU_RESOURCE_BUFFER:
			{
				vgpu_buffer_t* buffer = (vgpu_buffer_t*)entries[i].resource;

				if (entries[i].treat_as_constant_buffer)
				{
					resource_table->constant_buffers[i] = buffer->buffer;
				}
				else
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
					view_desc.Format = DXGI_FORMAT_UNKNOWN;
					view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
					view_desc.BufferEx.FirstElement = entries[i].offset;
					view_desc.BufferEx.NumElements = entries[i].num_bytes / buffer->stride;
					view_desc.BufferEx.Flags = 0;
					HRESULT hr = device->d3dd->CreateShaderResourceView(
						buffer->buffer,
						&view_desc,
						&resource_table->views[i]);
					VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create table SRV for buffer");
				}
			}
			break;

		default:
			break;
		}
	}

	return resource_table;
}

void vgpu_destroy_resource_table(vgpu_device_t* device, vgpu_resource_table_t* resource_table)
{
	for(size_t i = 0; i < resource_table->num_entries; ++i)
	{
		switch(resource_table->entries[i].type)
		{
		case VGPU_RESOURCE_BUFFER:
			SAFE_RELEASE(resource_table->views[i]);
			SAFE_RELEASE(resource_table->constant_buffers[i]);
			break;
		default:
			break;
		}
	}
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
	vgpu_texture_t* texture = VGPU_ALLOC_TYPE(device->allocator, vgpu_texture_t);

	// TODO: more than 2D
	VGPU_ASSERT(device, params->type == VGPU_TEXTURETYPE_2D, "Invalid texture format");

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = params->width;
	desc.Height = params->height;
	desc.MipLevels = params->num_mips;
	desc.ArraySize = 1;
	desc.Format = translate_textureformat[params->format];
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = translate_usage[params->usage];
	desc.BindFlags = desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ? D3D11_BIND_DEPTH_STENCIL : 0;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	HRESULT hr = device->d3dd->CreateTexture2D(
		&desc,
		nullptr,
		&texture->texture2d);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create texture");

	texture->clear_value = params->clear_value;

	return texture;
}

void vgpu_destroy_texture(vgpu_device_t* device, vgpu_texture_t* texture)
{
	SAFE_RELEASE(texture->texture2d);
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
	program->program_type = params->program_type;

	HRESULT hr = E_FAIL;
	switch (params->program_type)
	{
	case VGPU_VERTEX_PROGRAM:
		hr = device->d3dd->CreateVertexShader(params->data, params->size, NULL, &program->vertex_shader);
		break;
	case VGPU_FRAGMENT_PROGRAM:
		hr = device->d3dd->CreatePixelShader(params->data, params->size, NULL, &program->pixel_shader);
		break;
	}
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create shader");
	return program;
}

void vgpu_destroy_program(vgpu_device_t* device, vgpu_program_t* program)
{
	switch (program->program_type)
	{
	case VGPU_VERTEX_PROGRAM:
		SAFE_RELEASE(program->vertex_shader);
		break;
	case VGPU_FRAGMENT_PROGRAM:
		SAFE_RELEASE(program->pixel_shader);
		break;
	}
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
	ZeroMemory(pipeline, sizeof(*pipeline));

	pipeline->vertex_program = params->vertex_program;
	pipeline->fragment_program = params->fragment_program;
	pipeline->prim_type = translate_primitive_type[params->primitive_type];

	D3D11_BLEND_DESC blend_desc = {
		FALSE, // Alpha to coverage blend
		params->state.blend_independent, // Separate blend
		{
			0
		}
	};
	size_t max_blend = params->state.blend_independent ? VGPU_MAX_RENDER_TARGETS : 1;
	for(size_t i = 0; i < max_blend; ++i)
	{
		D3D11_RENDER_TARGET_BLEND_DESC* target = blend_desc.RenderTarget + i;
		target->BlendEnable = params->state.blend[i].enabled;
		target->SrcBlend = translate_blend_elem[params->state.blend[i].color_src];
		target->DestBlend = translate_blend_elem[params->state.blend[i].color_dst];
		target->BlendOp = translate_blend_op[params->state.blend[i].color_op];
		target->SrcBlendAlpha = translate_blend_elem[params->state.blend[i].alpha_src];
		target->DestBlendAlpha = translate_blend_elem[params->state.blend[i].alpha_dst];
		target->BlendOpAlpha = translate_blend_op[params->state.blend[i].alpha_op];
		target->RenderTargetWriteMask = 0x0f;
	}
	HRESULT hr = device->d3dd->CreateBlendState(&blend_desc, &pipeline->blend_state);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create blend state");

	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {
		params->state.depth.enabled,
		D3D11_DEPTH_WRITE_MASK_ALL,
		translate_compare_func[params->state.depth.func],
		params->state.stencil.enabled,
		params->state.stencil.read_mask,
		params->state.stencil.write_mask,
		{ // FrontFace
			translate_stencil_op[params->state.stencil.front.fail_op],
			translate_stencil_op[params->state.stencil.front.depth_fail_op],
			translate_stencil_op[params->state.stencil.front.pass_op],
			translate_compare_func[params->state.stencil.front.func],
		},
		{ // BackFace
			translate_stencil_op[params->state.stencil.back.fail_op],
			translate_stencil_op[params->state.stencil.back.depth_fail_op],
			translate_stencil_op[params->state.stencil.back.pass_op],
			translate_compare_func[params->state.stencil.back.func],
		},
	};
	hr = device->d3dd->CreateDepthStencilState(&depth_stencil_desc, &pipeline->depth_stencil_state);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create depth stencil state");

	D3D11_RASTERIZER_DESC rasterizer_desc = {
		translate_fill_mode[params->state.fill],
		translate_cull_mode[params->state.cull],
		params->state.wind == VGPU_WIND_CCW,
		(INT)params->state.depth_bias,
		0.0f, // DepthBiasClamp
		0.0f, // SlopeScaledDepthBias
		FALSE, // DepthClipEnable
		FALSE, // ScissorEnable
		FALSE, // MultisampleEnable
		FALSE, // AntialiasedLineEnable
	};
	hr = device->d3dd->CreateRasterizerState(&rasterizer_desc, &pipeline->rasterizer_state);
	VGPU_ASSERT(device, SUCCEEDED(hr), "Failed to create rasterizer state");

	pipeline->root_layout = params->root_layout;

	return pipeline;
}

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline)
{
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
	ZeroMemory(render_pass, sizeof(*render_pass));
	render_pass->num_rtv = params->num_color_targets;
	
	for (size_t i = 0; i < params->num_color_targets; ++i)
	{
		/*D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = ...;
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;*/
		device->d3dd->CreateRenderTargetView(
			params->color_targets[i].texture->texture2d,
			nullptr,
			&render_pass->rtv[i]);
		render_pass->rtv_clear_value[i] = params->color_targets[i].texture->clear_value;
		// TODO: clear on bind
	}

	if (params->depth_stencil_target.texture)
	{
		/*D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = ...;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Flags = 0;
		desc.Texture2D.MipSlice = 0;*/
		device->d3dd->CreateDepthStencilView(
			params->depth_stencil_target.texture->texture2d,
			nullptr,
			&render_pass->dsv);
		render_pass->dsv_clear_value = params->depth_stencil_target.texture->clear_value;
		// TODO: clear on bind
	}

	return render_pass;
}

void vgpu_destroy_render_pass(vgpu_device_t* device, vgpu_render_pass_t* render_pass)
{
	SAFE_RELEASE(render_pass->dsv);
	for (size_t i = 0; i < render_pass->num_rtv; ++i)
		SAFE_RELEASE(render_pass->rtv[i]);
	VGPU_FREE(device->allocator, render_pass);
}

/******************************************************************************\
 *
 *  command_list handling
 *
\******************************************************************************/

vgpu_command_list_t* vgpu_create_command_list(vgpu_device_t* device, const vgpu_create_command_list_params_t* params)
{
	VGPU_ASSERT(device, params->type == VGPU_COMMAND_LIST_IMMEDIATE_GRAPHICS, "Only immediate graphics command lists supported on DX11");
	VGPU_ASSERT(device, device->immediate_command_list == nullptr, "An immediate graphics command list has already been created");

	vgpu_command_list_t* command_list = VGPU_ALLOC_TYPE(device->allocator, vgpu_command_list_t);
	command_list->device = device;
	command_list->d3dc = device->d3dc;
	command_list->d3dc1 = device->d3dc1;
	command_list->curr_pipeline = nullptr;
	command_list->curr_root_layout = nullptr;

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
}

void vgpu_end_command_list(vgpu_command_list_t* command_list)
{
}

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;

	// TODO: map only part of the buffer
	D3D11_MAPPED_SUBRESOURCE res;
	HRESULT hr = d3dc->Map(params->buffer->buffer,
			0, // SubResource
			D3D11_MAP_WRITE_NO_OVERWRITE,
			0, // MapFlags
			&res);
	VGPU_ASSERT(command_list->device, SUCCEEDED(hr), "Failed to map buffer");

	uint8_t* ptr = ((uint8_t*)res.pData) + params->offset;

	return (void*)ptr;
}

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;

	d3dc->Unmap(params->buffer->buffer,
			0); // SubResource
}

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes)
{
	if (buffer->usage == D3D11_USAGE_DEFAULT)
	{
		ID3D11DeviceContext* d3dc = command_list->d3dc;
		if (buffer->flags & VGPU_BUFFER_FLAG_CONSTANT_BUFFER)
		{
			d3dc->UpdateSubresource(
				buffer->buffer,
				0, // DstSubresource
				nullptr, // pDstBox
				data,
				0, //SrcRowPitch
				0); //SrcDepthPitch
		}
		else
		{
			D3D11_BOX box;
			box.left = offset;
			box.top = 0;
			box.front = 0;
			box.right = offset + num_bytes;
			box.bottom = 1;
			box.back = 1;

			d3dc->UpdateSubresource(
				buffer->buffer,
				0, // DstSubresource
				&box,
				data,
				0, //SrcRowPitch
				0); //SrcDepthPitch
		}
	}
	else
	{
		vgpu_lock_buffer_params_t lock_params;
		lock_params.buffer = buffer;
		lock_params.offset = offset;
		lock_params.num_bytes = num_bytes;

		void* ptr = vgpu_lock_buffer(command_list, &lock_params);
		memcpy(ptr, data, num_bytes);
		vgpu_unlock_buffer(command_list, &lock_params);
	}
}

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;
	vgpu_pipeline_t* pipeline = command_list->curr_pipeline;

	// TODO: prebake views into arrays and set as many as possible at once
	for(size_t i = 0; i < resource_table->num_entries; ++i)
	{
		vgpu_resource_table_entry_t* entry = &resource_table->entries[i];
		size_t loc = entry->location;
		switch(entry->type)
		{
			case VGPU_RESOURCE_TEXTURE:
			case VGPU_RESOURCE_SAMPLER:
			case VGPU_RESOURCE_NONE:
				break;
			case VGPU_RESOURCE_BUFFER:
				{
					// TODO: get slot mask from root table layout
					if (resource_table->views[i])
					{
						//if(pipeline->vertex_program_slot_mask & (1<<loc))
						d3dc->VSSetShaderResources(loc, 1, &resource_table->views[i]);
						//if(pipeline->fragment_program_slot_mask & (1<<loc))
						d3dc->PSSetShaderResources(loc, 1, &resource_table->views[i]);
					}

					if (resource_table->constant_buffers[i])
					{
						d3dc->VSSetConstantBuffers(loc, 1, &resource_table->constant_buffers[i]);
						d3dc->PSSetConstantBuffers(loc, 1, &resource_table->constant_buffers[i]);
					}
					break;
				}
		}
	}
}

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes)
{
	if (command_list->curr_root_layout->slots[slot].resource.treat_as_constant_buffer)
	{
		if (command_list->device->caps.flags & VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET)
		{
			VGPU_ASSERT(command_list->device, offset % 256 == 0, "Buffer offset must be 256 byte aligned");
			VGPU_ASSERT(command_list->device, num_bytes % 256 == 0, "Buffer num bytes must be a multiple of 256");
			UINT first = offset / 16;
			UINT num = num_bytes / 16;
			command_list->d3dc1->VSSetConstantBuffers1(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->buffer, &first, &num);
			command_list->d3dc1->PSSetConstantBuffers1(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->buffer, &first, &num);
		}
		else
		{
			VGPU_ASSERT(command_list->device, offset == 0, "Buffer must be bound with an offset of 0 on this device");
			command_list->d3dc->VSSetConstantBuffers(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->buffer);
			command_list->d3dc->PSSetConstantBuffers(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->buffer);
		}
	}
	else
	{
		if (command_list->device->caps.flags & VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET)
		{
			VGPU_BREAKPOINT();
			// Ugly hack :/
			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
			view_desc.Format = DXGI_FORMAT_UNKNOWN;
			view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			view_desc.BufferEx.FirstElement = offset / buffer->stride;
			view_desc.BufferEx.NumElements = num_bytes / buffer->stride;
			view_desc.BufferEx.Flags = 0;

			ID3D11ShaderResourceView* view;
			HRESULT hr = command_list->device->d3dd->CreateShaderResourceView(buffer->buffer, &view_desc, &view);
			VGPU_ASSERT(command_list->device, SUCCEEDED(hr), "Failed to create buffer SRV");

			command_list->d3dc->VSSetShaderResources(command_list->curr_root_layout->slots[slot].resource.location, 1, &view);
			command_list->d3dc->PSSetShaderResources(command_list->curr_root_layout->slots[slot].resource.location, 1, &view);

			view->Release();
		}
		else
		{
			VGPU_ASSERT(command_list->device, offset == 0, "Buffer must be bound at with an offset of 0");
			command_list->d3dc->VSSetShaderResources(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->view);
			command_list->d3dc->PSSetShaderResources(command_list->curr_root_layout->slots[slot].resource.location, 1, &buffer->view);
		}
	}
}

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;

	d3dc->VSSetShader(pipeline->vertex_program->vertex_shader, NULL, 0);
	d3dc->PSSetShader(pipeline->fragment_program->pixel_shader, NULL, 0);
	d3dc->IASetPrimitiveTopology(pipeline->prim_type);

	ID3D11ShaderResourceView* rv[64] = { NULL };
	d3dc->VSSetShaderResources(0, 64, rv);
	d3dc->PSSetShaderResources(0, 64, rv);
	d3dc->OMSetBlendState(pipeline->blend_state, NULL, 0xffffffff);
	d3dc->OMSetDepthStencilState(pipeline->depth_stencil_state, 0);
	d3dc->RSSetState(pipeline->rasterizer_state);

	command_list->curr_pipeline = pipeline;
	command_list->curr_root_layout = pipeline->root_layout;
}

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;
	DXGI_FORMAT format = (index_type == VGPU_DATA_TYPE_UINT32) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	d3dc->IASetIndexBuffer(index_buffer->buffer, format, 0);
}

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;
	d3dc->DrawInstanced(
			num_vertices,
			num_instances,
			first_vertex,
			first_instance);
}

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex)
{
	ID3D11DeviceContext* d3dc = command_list->d3dc;
	d3dc->DrawIndexedInstanced(
			num_indices,
			num_instances,
			first_index,
			first_vertex,
			first_instance);
}

void vgpu_draw_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	VGPU_BREAKPOINT();
}

void vgpu_draw_indexed_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	VGPU_BREAKPOINT();
}

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	for (uint32_t i = 0; i < render_pass->num_rtv; ++i)
	{
		command_list->d3dc->ClearRenderTargetView(
			render_pass->rtv[i],
			render_pass->rtv_clear_value[i].color);
	}
	if (render_pass->dsv)
	{
		command_list->d3dc->ClearDepthStencilView(
			render_pass->dsv,
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			render_pass->dsv_clear_value.depth_stencil.depth,
			render_pass->dsv_clear_value.depth_stencil.stencil);
	}
}

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	vgpu_clear_render_pass(command_list, render_pass);

	command_list->d3dc->OMSetRenderTargets(
		render_pass->num_rtv,
		render_pass->rtv,
		render_pass->dsv);
}

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}
