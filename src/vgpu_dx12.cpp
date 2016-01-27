#include <vgpu.h>
#include "vgpu_internal.h" 
#include "vgpu_id_pool.h"
#include "vgpu_range_pool.h"

#include <windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcommon.h>

#define SAFE_RELEASE(x) if(x) (x)->Release()

/******************************************************************************\
 *
 *  Translation utils
 *
\******************************************************************************/

static const D3D12_RESOURCE_DIMENSION translate_texturetype[] = {
	D3D12_RESOURCE_DIMENSION_TEXTURE1D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	D3D12_RESOURCE_DIMENSION_TEXTURE3D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D
};

static const DXGI_FORMAT translate_textureformat[] = {
	DXGI_FORMAT_B8G8R8A8_UNORM,
	DXGI_FORMAT_BC1_UNORM,
	DXGI_FORMAT_BC2_UNORM,
	DXGI_FORMAT_BC3_UNORM,

	DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
};

static const D3D12_PRIMITIVE_TOPOLOGY_TYPE translate_primitive_type_top[] = {
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
};

static const D3D12_PRIMITIVE_TOPOLOGY translate_primitive_type[] = {
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D_PRIMITIVE_TOPOLOGY_LINELIST,
};

static const D3D12_FILL_MODE translate_fill_mode[] = {
	D3D12_FILL_MODE_SOLID,
	D3D12_FILL_MODE_WIREFRAME,
};

static const D3D12_CULL_MODE translate_cull_mode[] = {
	D3D12_CULL_MODE_NONE,
	D3D12_CULL_MODE_FRONT,
	D3D12_CULL_MODE_BACK,
};

static const D3D12_BLEND translate_blend_elem[] = {
	D3D12_BLEND_ZERO,
	D3D12_BLEND_ONE,
	D3D12_BLEND_SRC_COLOR,
	D3D12_BLEND_INV_SRC_COLOR,
	D3D12_BLEND_SRC_ALPHA,
	D3D12_BLEND_INV_SRC_ALPHA,
	D3D12_BLEND_DEST_ALPHA,
	D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_DEST_COLOR,
	D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA_SAT,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
	D3D12_BLEND_SRC1_COLOR,
	D3D12_BLEND_INV_SRC1_COLOR,
	D3D12_BLEND_SRC1_ALPHA,
	D3D12_BLEND_INV_SRC1_ALPHA,
};

static const D3D12_BLEND_OP translate_blend_op[] = {
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

static const D3D12_COMPARISON_FUNC translate_compare_func[] = {
	D3D12_COMPARISON_FUNC_ALWAYS,
	D3D12_COMPARISON_FUNC_LESS,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_EQUAL,
	D3D12_COMPARISON_FUNC_NOT_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER,
	D3D12_COMPARISON_FUNC_NEVER,
};

static const D3D12_STENCIL_OP translate_stencil_op[] = {
	D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_ZERO,
	D3D12_STENCIL_OP_REPLACE,
	D3D12_STENCIL_OP_INCR_SAT,
	D3D12_STENCIL_OP_DECR_SAT,
	D3D12_STENCIL_OP_INVERT,
	D3D12_STENCIL_OP_INCR,
	D3D12_STENCIL_OP_DECR,
};

/******************************************************************************\
 *
 *  Structures
 *
\******************************************************************************/

struct vgpu_resource_t
{
	ID3D12Resource* resource;
};

struct vgpu_buffer_s : vgpu_resource_t
{
	size_t num_bytes;
	size_t stride;
	D3D12_HEAP_TYPE heap_type;
};

struct vgpu_texture_s : vgpu_resource_t
{
	D3D12_CLEAR_VALUE clear_value;
};

struct vgpu_resource_table_s
{
	// Keeping track of the allocation
	uint32_t cbv_srv_uav_offset;
	uint32_t num_cbv_srv_uav;

	// The actual data to set
	uint32_t cbv_srv_uav_index;
	D3D12_GPU_DESCRIPTOR_HANDLE cbv_srv_uav;
};

struct vgpu_root_layout_s
{
	ID3D12RootSignature* root_signature;

	struct
	{
		vgpu_root_slot_type_t type;

		bool treat_as_constant_buffer;

		uint32_t cbv_srv_uav_index;

		uint32_t num_cbv_srv_uav;

		struct
		{
			uint8_t start;
			uint8_t count;
			uint8_t offset;
		} srv, cbv;
	} slots[4];
};
struct vgpu_program_s
{
	uint8_t* data;
	size_t size;
	vgpu_program_type_t program_type;
};

struct vgpu_pipeline_s
{
	ID3D12PipelineState* pipeline_state;
	vgpu_root_layout_t* root_layout;
	D3D12_PRIMITIVE_TOPOLOGY primitive_topology;
};

struct vgpu_render_pass_s
{
	size_t num_rtv;
	uint32_t rtv_offset[VGPU_MULTI_BUFFERING];
	uint32_t dsv_offset;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv[VGPU_MULTI_BUFFERING];
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;

	D3D12_CLEAR_VALUE rtv_clear_value[16];
	D3D12_CLEAR_VALUE dsv_clear_value;

	vgpu_texture_t* rtv_texture[16];
	vgpu_texture_t* dsv_texture;

	bool is_framebuffer[16];
	bool has_framebuffer;
};

struct vgpu_command_list_s
{
	vgpu_device_t* device;
	vgpu_thread_context_t* thread_context;
	ID3D12GraphicsCommandList* d3dcl;

	vgpu_pipeline_t* curr_pipeline;
	vgpu_root_layout_t* curr_root_layout;
};

struct vgpu_thread_context_s
{
	struct frame_data_t
	{
		ID3D12CommandAllocator* command_allocator_graphics;
		ID3D12Resource* upload_buffer;
		size_t upload_buffer_size;
		size_t upload_offset;
		vgpu_array_t<IUnknown*> delay_delete_queue;
		vgpu_array_t<ID3D12GraphicsCommandList*> free;
		vgpu_array_t<ID3D12GraphicsCommandList*> pending;
	} frame[VGPU_MULTI_BUFFERING];
};

struct vgpu_device_s
{
	vgpu_allocator_t* allocator;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;

	uint32_t width;
	uint32_t height;
	vgpu_caps_t caps;

	ID3D12Device* d3dd;
	ID3D12CommandQueue* graphics_command_queue;

	IDXGIFactory4* dxgif;
	IDXGISwapChain3* swapchain;

	vgpu_range_pool_t<uint32_t> rtv_pool;
	ID3D12DescriptorHeap* rtv_heap;

	vgpu_id_pool_t<uint32_t> dsv_pool;
	ID3D12DescriptorHeap* dsv_heap;
	
	vgpu_range_pool_t<uint32_t> cbv_srv_uav_pool;
	ID3D12DescriptorHeap* cbv_srv_uav_heap;

	vgpu_range_pool_t<uint32_t> sampler_pool;
	ID3D12DescriptorHeap* sampler_heap;

	uint32_t rtv_size;
	uint32_t dsv_size;
	uint32_t cbv_srv_uav_size;
	uint32_t sampler_size;

	ID3D12Fence* frame_fence;
	HANDLE frame_event;
	uint64_t frame_no;
	struct frame_data_t
	{
		uint64_t fence_value;
		ID3D12Resource* backbuffer_resource;
		vgpu_array_t<IUnknown*> delay_delete_queue;
	} frame[VGPU_MULTI_BUFFERING];

	vgpu_texture_t backbuffer;

	ID3D12CommandSignature* draw_indirect_signature;
	ID3D12CommandSignature* draw_indexed_indirect_signature;

	ID3D12RootSignature* fullscreen_root_signature;
	ID3D12PipelineState* fullscreen_pipeline_state;
};

static vgpu_device_t::frame_data_t& get_frame(vgpu_device_t* device, uint64_t frame)
{
	return device->frame[frame % VGPU_MULTI_BUFFERING];
}

static vgpu_device_t::frame_data_t& curr_frame(vgpu_device_t* device)
{
	return get_frame(device, device->frame_no);
}

static void push_delay_delete(vgpu_device_t* device, IUnknown* res)
{
	auto& frame = curr_frame(device);

	if(frame.delay_delete_queue.full())
		frame.delay_delete_queue.grow();

	frame.delay_delete_queue.append(res);
}

static ID3D12Resource* create_upload_buffer(vgpu_device_t* device, size_t size)
{
	CD3DX12_HEAP_PROPERTIES heap_prop(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
		CD3DX12_RESOURCE_ALLOCATION_INFO(128 * 1024 * 1024, 0));

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->d3dd->CreateCommittedResource(
		&heap_prop,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create commited resource");

	return resource;
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
	ZeroMemory(device, sizeof(vgpu_device_t));
	device->allocator = allocator;

	device->log_func = params->log_func;
	device->error_func = params->error_func;

	device->width = 1280;
	device->height = 720;
	device->caps.flags = (~params->force_disable_flags) & (VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET | VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET);

	HRESULT hr = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device->d3dd));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create device");

	D3D12_COMMAND_QUEUE_DESC gcq_desc = {};
	gcq_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	gcq_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	gcq_desc.NodeMask = 0xFFFFFFFF;
	hr = device->d3dd->CreateCommandQueue(
		&gcq_desc,
		IID_PPV_ARGS(&device->graphics_command_queue));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create command queue");

	hr = CreateDXGIFactory1(IID_PPV_ARGS(&device->dxgif));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create dxgi factory");

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	swap_chain_desc.BufferCount = VGPU_MULTI_BUFFERING;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.OutputWindow = (HWND)params->window;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.Windowed = TRUE;
	hr = device->dxgif->CreateSwapChain(
		device->graphics_command_queue,
		&swap_chain_desc,
		(IDXGISwapChain**)&device->swapchain);
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create swapchain");

	device->rtv_size = device->d3dd->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->dsv_size = device->d3dd->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	device->cbv_srv_uav_size = device->d3dd->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->sampler_size = device->d3dd->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = 1014;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->d3dd->CreateDescriptorHeap(
		&rtv_heap_desc,
		IID_PPV_ARGS(&device->rtv_heap));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create descriptor heap (RTV)");
	device->rtv_pool.create(allocator, rtv_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
	dsv_heap_desc.NumDescriptors = 32;
	dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->d3dd->CreateDescriptorHeap(
		&dsv_heap_desc,
		IID_PPV_ARGS(&device->dsv_heap));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create descriptor heap (DSV)");
	device->dsv_pool.create(allocator, dsv_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc = {};
	cbv_srv_uav_heap_desc.NumDescriptors = 1000000;
	cbv_srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbv_srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = device->d3dd->CreateDescriptorHeap(
		&cbv_srv_uav_heap_desc,
		IID_PPV_ARGS(&device->cbv_srv_uav_heap));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create descriptor heap (CBV/SRV/UAV)");
	device->cbv_srv_uav_pool.create(allocator, cbv_srv_uav_heap_desc.NumDescriptors);

	D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {};
	sampler_heap_desc.NumDescriptors = 2048;
	sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = device->d3dd->CreateDescriptorHeap(
		&sampler_heap_desc,
		IID_PPV_ARGS(&device->sampler_heap));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create descriptor heap (SAMPLER)");
	device->sampler_pool.create(allocator, sampler_heap_desc.NumDescriptors);

	// create per frame resources
	hr = device->d3dd->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->frame_fence));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create frame fence");
	device->frame_event = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
	VGPU_ASSERT(device, device->frame_event != nullptr, "failed to create frame event");
	device->frame_no = 0;
	for(int i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		auto& frame = get_frame(device, i);

		frame.fence_value = 0;
		frame.delay_delete_queue.create(allocator, 128);

		hr = device->swapchain->GetBuffer(
			i,
			IID_PPV_ARGS(&frame.backbuffer_resource));
		VGPU_ASSERT(device, SUCCEEDED(hr), "failed to get back buffer %d", i);

		const char backbuffer_name[] = "back buffer";
		frame.backbuffer_resource->SetPrivateData(
			WKPDID_D3DDebugObjectName,
			sizeof(backbuffer_name) - 1,
			backbuffer_name);
		VGPU_ASSERT(device, SUCCEEDED(hr), "failed to set back buffer name");
	}

	const float clear_color[4] = { 0.1f, 0.1f, 0.3f, 1.0f };
	memset(&device->backbuffer, 0, sizeof(device->backbuffer));
	device->backbuffer.clear_value = CD3DX12_CLEAR_VALUE(swap_chain_desc.BufferDesc.Format, clear_color);

	D3D12_INDIRECT_ARGUMENT_DESC draw_indirect_args[1];
	draw_indirect_args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	D3D12_COMMAND_SIGNATURE_DESC draw_indirect_desc = {
		sizeof(draw_indirect_args[0]),
		VGPU_ARRAY_LENGTH(draw_indirect_args),
		draw_indirect_args,
		0, // node mask
	};
	hr = device->d3dd->CreateCommandSignature(
		&draw_indirect_desc,
		nullptr,
		IID_PPV_ARGS(&device->draw_indirect_signature));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create draw indirect command signature");

	D3D12_INDIRECT_ARGUMENT_DESC draw_indirect_indexed_args[1];
	draw_indirect_indexed_args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	D3D12_COMMAND_SIGNATURE_DESC draw_indirect_indexed_desc = {
		sizeof(draw_indirect_indexed_args[0]),
		VGPU_ARRAY_LENGTH(draw_indirect_indexed_args),
		draw_indirect_indexed_args,
		0, // node mask
	};
	hr = device->d3dd->CreateCommandSignature(
		&draw_indirect_indexed_desc,
		nullptr,
		IID_PPV_ARGS(&device->draw_indexed_indirect_signature));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create draw indexed indirect command signature");

	return device;
}

void vgpu_destroy_device(vgpu_device_t* device)
{

	uint64_t next_fence = device->frame_no;
	uint64_t last_completed_fence = device->frame_fence->GetCompletedValue();
	device->graphics_command_queue->Signal(device->frame_fence, next_fence);

	// Wait until a new slot of frame resources is ready
	if(last_completed_fence < next_fence)
	{
		device->frame_fence->SetEventOnCompletion(next_fence, device->frame_event);
		WaitForSingleObject(device->frame_event, INFINITE);
	}

	SAFE_RELEASE(device->draw_indexed_indirect_signature);
	SAFE_RELEASE(device->draw_indirect_signature);

	for(int i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		auto& frame = get_frame(device, i);
		SAFE_RELEASE(frame.backbuffer_resource);
	}
	// TODO: destroy frame_event?
	SAFE_RELEASE(device->frame_fence);
	SAFE_RELEASE(device->sampler_heap);
	SAFE_RELEASE(device->cbv_srv_uav_heap);
	SAFE_RELEASE(device->dsv_heap);
	SAFE_RELEASE(device->rtv_heap);
	SAFE_RELEASE(device->swapchain);
	SAFE_RELEASE(device->graphics_command_queue);
	SAFE_RELEASE(device->d3dd);

	VGPU_FREE(device->allocator, device);
}

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device)
{
	return VGPU_DEVICE_DX12;
}

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue)
{
	size_t id = device->frame_no % VGPU_MULTI_BUFFERING;

	VGPU_ASSERT(device, queue == 0, "Only queue 0 supported for now");
	VGPU_ASSERT(device, num_command_lists > 0, "No command lists to apply");

	ID3D12CommandList* d3d_command_lists[128];
	VGPU_ASSERT(device, num_command_lists <= VGPU_ARRAY_LENGTH(d3d_command_lists), "Too many command lists to apply");
	for (uint32_t i = 0; i < num_command_lists; ++i)
	{
		d3d_command_lists[i] = command_lists[i]->d3dcl;
		command_lists[i]->thread_context->frame[id].pending.append(command_lists[i]->d3dcl);
		command_lists[i]->d3dcl = nullptr;
		command_lists[i]->thread_context = nullptr;
	}

	device->graphics_command_queue->ExecuteCommandLists(num_command_lists, d3d_command_lists);
}

void vgpu_present(vgpu_device_t* device)
{
	DXGI_PRESENT_PARAMETERS present_params;
	ZeroMemory(&present_params, sizeof(present_params));
	device->swapchain->Present1(0, DXGI_PRESENT_RESTART, &present_params);

	UINT64 next_fence = device->frame_no;
	UINT64 last_completed_fence = device->frame_fence->GetCompletedValue();
	device->graphics_command_queue->Signal(device->frame_fence, next_fence);

	auto& this_frame = curr_frame(device);
	this_frame.fence_value = next_fence;

	// Step frame counter and reset per frame data
	device->frame_no += 1;
	auto& next_frame = curr_frame(device); // this is now the new frame

										   // Wait until a new slot of frame resources is ready
	if (last_completed_fence < next_frame.fence_value)
	{
		device->frame_fence->SetEventOnCompletion(next_frame.fence_value, device->frame_event);
		WaitForSingleObject(device->frame_event, INFINITE);
	}

	for (size_t i = 0; i < next_frame.delay_delete_queue.length(); ++i)
		next_frame.delay_delete_queue[i]->Release();
	next_frame.delay_delete_queue.set_length(0);
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
	vgpu_thread_context_t* thread_context = VGPU_NEW(device->allocator, vgpu_thread_context_t);
	for (int i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		HRESULT hr = device->d3dd->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&thread_context->frame[i].command_allocator_graphics));
		VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create command allocator");

		thread_context->frame[i].upload_buffer_size = 64 * 1024 * 1024;
		thread_context->frame[i].upload_buffer = create_upload_buffer(device, thread_context->frame[i].upload_buffer_size);
		thread_context->frame[i].upload_offset = 0;
		thread_context->frame[i].delay_delete_queue.create(device->allocator, 4);
		thread_context->frame[i].free.create(device->allocator, 8);
		thread_context->frame[i].pending.create(device->allocator, 8);
	}
	return thread_context;
}

void vgpu_destroy_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
	for (int i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		SAFE_RELEASE(thread_context->frame[i].command_allocator_graphics);
		SAFE_RELEASE(thread_context->frame[i].upload_buffer);
	}
	VGPU_DELETE(device->allocator, vgpu_thread_context_t, thread_context);
}

void vgpu_prepare_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
	uint32_t id = device->frame_no % VGPU_MULTI_BUFFERING;
	thread_context->frame[id].upload_offset = 0;
	thread_context->frame[id].command_allocator_graphics->Reset();

	// TODO: make sure frame fence has passed
	while (thread_context->frame[id].pending.any())
	{
		ID3D12GraphicsCommandList* cl = thread_context->frame[id].pending.back();
		thread_context->frame[id].pending.remove_back();
		thread_context->frame[id].free.append(cl);
	}
}

/******************************************************************************\
 *
 *  Buffer handling
 *
\******************************************************************************/

vgpu_buffer_t* vgpu_create_buffer(vgpu_device_t* device, const vgpu_create_buffer_params_t* params)
{
	vgpu_buffer_t* buffer = VGPU_ALLOC_TYPE(device->allocator, vgpu_buffer_t);

	bool is_dynamic = params->usage == VGPU_USAGE_DYNAMIC;
	D3D12_HEAP_TYPE heap_type = is_dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_PROPERTIES heap_prop = CD3DX12_HEAP_PROPERTIES(heap_type);

	D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(params->num_bytes);

	HRESULT hr = device->d3dd->CreateCommittedResource(
		&heap_prop,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer->resource));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create committed resource");

	buffer->resource->SetPrivateData(
		WKPDID_D3DDebugObjectName,
		strlen(params->name),
		params->name);

	buffer->num_bytes = params->num_bytes;
	buffer->stride = params->structure_stride;
	buffer->heap_type = heap_type;

	return buffer;
}

void vgpu_destroy_buffer(vgpu_device_t* device, vgpu_buffer_t* buffer)
{
	SAFE_RELEASE(buffer->resource);

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
	ZeroMemory(resource_table, sizeof(*resource_table));

	D3D12_CPU_DESCRIPTOR_HANDLE cbv_srv_uav;
	resource_table->num_cbv_srv_uav = root_layout->slots[root_slot].num_cbv_srv_uav;
	resource_table->cbv_srv_uav_index = root_layout->slots[root_slot].cbv_srv_uav_index;
	if(resource_table->num_cbv_srv_uav)
	{
		resource_table->cbv_srv_uav_offset = device->cbv_srv_uav_pool.alloc(resource_table->num_cbv_srv_uav);
		resource_table->cbv_srv_uav = device->cbv_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
		cbv_srv_uav = device->cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
		resource_table->cbv_srv_uav.ptr += resource_table->cbv_srv_uav_offset * device->cbv_srv_uav_size;
		cbv_srv_uav.ptr += resource_table->cbv_srv_uav_offset * device->cbv_srv_uav_size;
	}

	for(size_t i = 0; i < num_entries; ++i)
	{
		switch(entries[i].type)
		{
			case VGPU_RESOURCE_BUFFER:
			{
				vgpu_buffer_t* buffer = (vgpu_buffer_t*)entries[i].resource;

				if(entries[i].treat_as_constant_buffer)
				{
					VGPU_ASSERT(device, entries[i].location >= root_layout->slots[root_slot].cbv.start, "resource table location %d out of bounds", i);
					VGPU_ASSERT(device, entries[i].location < (root_layout->slots[root_slot].cbv.start + root_layout->slots[root_slot].cbv.count), "resource table location %d out of bounds", i);
					uint32_t offset = entries[i].location + root_layout->slots[root_slot].cbv.offset - root_layout->slots[root_slot].cbv.start;
					D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = cbv_srv_uav;
					descriptor_handle.ptr += offset * device->cbv_srv_uav_size;

					D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
					desc.BufferLocation = buffer->resource->GetGPUVirtualAddress() + entries[i].offset;
					desc.SizeInBytes = entries[i].num_bytes;
					device->d3dd->CreateConstantBufferView(&desc, descriptor_handle);
				}
				else
				{
					VGPU_ASSERT(device, entries[i].location >= root_layout->slots[root_slot].srv.start, "resource table location %d out of bounds", i);
					VGPU_ASSERT(device, entries[i].location < (root_layout->slots[root_slot].srv.start + root_layout->slots[root_slot].srv.count), "resource table location %d out of bounds", i);
					uint32_t offset = entries[i].location + root_layout->slots[root_slot].srv.offset - root_layout->slots[root_slot].srv.start;
					D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = cbv_srv_uav;
					descriptor_handle.ptr += offset * device->cbv_srv_uav_size;

					D3D12_SHADER_RESOURCE_VIEW_DESC desc;
					ZeroMemory(&desc, sizeof(desc));
					desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Buffer.FirstElement = entries[i].offset / buffer->stride;
					desc.Buffer.NumElements = entries[i].num_bytes / buffer->stride;
					desc.Buffer.StructureByteStride = buffer->stride;
					device->d3dd->CreateShaderResourceView(buffer->resource, &desc, descriptor_handle);
				}

				break;
			}
			case VGPU_RESOURCE_TEXTURE:
			case VGPU_RESOURCE_SAMPLER:
				// TODO:
				continue;
			default:
				VGPU_BREAKPOINT();
		}
	}

	return resource_table;
}

void vgpu_destroy_resource_table(vgpu_device_t* device, vgpu_resource_table_t* resource_table)
{
	if(resource_table->num_cbv_srv_uav)
		device->cbv_srv_uav_pool.free(resource_table->cbv_srv_uav_offset, resource_table->num_cbv_srv_uav);
	
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
	ZeroMemory(root_layout, sizeof(*root_layout));

	size_t curr_range = 0;
	size_t prev_curr_range = 0;
	CD3DX12_DESCRIPTOR_RANGE ranges[32];

	size_t curr_param = 0;
	CD3DX12_ROOT_PARAMETER parameters[8];

	for(size_t i = 0; i < num_slots; ++i)
	{
		root_layout->slots[i].type = slots[i].type;

		if (slots[i].type == VGPU_ROOT_SLOT_TYPE_TABLE)
		{
			root_layout->slots[i].num_cbv_srv_uav =
				slots[i].table.range_buffers.count +
				slots[i].table.range_constant_buffers.count;
			if (root_layout->slots[i].num_cbv_srv_uav)
			{
				root_layout->slots[i].cbv_srv_uav_index = curr_param;

				root_layout->slots[i].srv.start = slots[i].table.range_buffers.start;
				root_layout->slots[i].srv.count = slots[i].table.range_buffers.count;
				root_layout->slots[i].srv.offset = 0;

				root_layout->slots[i].cbv.start = slots[i].table.range_constant_buffers.start;
				root_layout->slots[i].cbv.count = slots[i].table.range_constant_buffers.count;
				root_layout->slots[i].cbv.offset = root_layout->slots[i].srv.offset +
					root_layout->slots[i].srv.count;

				if (root_layout->slots[i].srv.count)
				{
					ranges[curr_range++].Init(
						D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
						slots[i].table.range_buffers.count,
						slots[i].table.range_buffers.start);
				}

				if (root_layout->slots[i].cbv.count)
				{
					ranges[curr_range++].Init(
						D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
						slots[i].table.range_constant_buffers.count,
						slots[i].table.range_constant_buffers.start);
				}

				parameters[curr_param++].InitAsDescriptorTable(
					curr_range - prev_curr_range,
					&ranges[prev_curr_range],
					D3D12_SHADER_VISIBILITY_ALL);

				prev_curr_range = curr_range;
			}
			else
			{
				root_layout->slots[i].cbv_srv_uav_index = -1;
			}
		}
		else if (slots[i].type == VGPU_ROOT_SLOT_TYPE_RESOURCE)
		{
			root_layout->slots[i].treat_as_constant_buffer = slots[i].resource.treat_as_constant_buffer;
			root_layout->slots[i].cbv_srv_uav_index = curr_param;
			root_layout->slots[i].num_cbv_srv_uav = 1;

			if (slots[i].resource.treat_as_constant_buffer)
			{
				parameters[curr_param++].InitAsConstantBufferView(
					slots[i].resource.location);
			}
			else
			{
				parameters[curr_param++].InitAsShaderResourceView(
					slots[i].resource.location);
			}
		}
	}

	CD3DX12_ROOT_SIGNATURE_DESC root_desc;
	root_desc.Init(
		curr_param,
		parameters,
		0, // num static samplers
		nullptr, // static samplers
		D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ID3DBlob* out_blob;
	ID3DBlob* error_blob;
	HRESULT hr = D3D12SerializeRootSignature(
		&root_desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&out_blob,
		&error_blob);
	const char* error_msg;
	if(error_blob)
		error_msg = (const char*)error_blob->GetBufferPointer();
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to serialize root signature");

	hr = device->d3dd->CreateRootSignature(
		0,
		out_blob->GetBufferPointer(),
		out_blob->GetBufferSize(),
		IID_PPV_ARGS(&root_layout->root_signature));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create root signature");

	SAFE_RELEASE(error_blob);
	SAFE_RELEASE(out_blob);

	return root_layout;
}

void vgpu_destroy_root_layout(vgpu_device_t* device, vgpu_root_layout_t* root_layout)
{
	SAFE_RELEASE(root_layout->root_signature);
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

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		translate_textureformat[params->format],
		params->width,
		params->height,
		params->depth,
		params->num_mips,
		1, // SampleCount,
		0); // SampleQuality

	if (desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	else if (params->is_render_target)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heap_prop;
	ZeroMemory(&heap_prop, sizeof(heap_prop));
	heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;

	texture->clear_value.Format = desc.Format;
	memcpy(texture->clear_value.Color, params->clear_value.color, sizeof(texture->clear_value.Color));

	HRESULT hr = device->d3dd->CreateCommittedResource(
		&heap_prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		params->is_render_target ? &texture->clear_value : nullptr,
		IID_PPV_ARGS(&texture->resource));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create committed resource");

	texture->resource->SetPrivateData(
		WKPDID_D3DDebugObjectName,
		strlen(params->name),
		params->name);

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
	program->data = (uint8_t*)VGPU_ALLOC(device->allocator, params->size, 16);
	program->size = params->size;
	program->program_type = params->program_type;
	memcpy(program->data, params->data, params->size);
	return program;
}

void vgpu_destroy_program(vgpu_device_t* device, vgpu_program_t* program)
{
	VGPU_FREE(device->allocator, program->data);
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};
	pipeline_desc.pRootSignature = params->root_layout->root_signature;
	pipeline_desc.VS.pShaderBytecode = params->vertex_program->data;
	pipeline_desc.VS.BytecodeLength = params->vertex_program->size;
	pipeline_desc.PS.pShaderBytecode = params->fragment_program->data;
	pipeline_desc.PS.BytecodeLength = params->fragment_program->size;

	pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
	pipeline_desc.BlendState.IndependentBlendEnable = params->state.blend_independent;
	size_t max_blend = params->state.blend_independent ? VGPU_MAX_RENDER_TARGETS : 1;
	for (size_t i = 0; i < max_blend; ++i)
	{
		D3D12_RENDER_TARGET_BLEND_DESC* target = pipeline_desc.BlendState.RenderTarget + i;
		target->BlendEnable = params->state.blend[i].enabled;
		target->SrcBlend = translate_blend_elem[params->state.blend[i].color_src];
		target->DestBlend = translate_blend_elem[params->state.blend[i].color_dst];
		target->BlendOp = translate_blend_op[params->state.blend[i].color_op];
		target->SrcBlendAlpha = translate_blend_elem[params->state.blend[i].alpha_src];
		target->DestBlendAlpha = translate_blend_elem[params->state.blend[i].alpha_dst];
		target->BlendOpAlpha = translate_blend_op[params->state.blend[i].alpha_op];
		target->RenderTargetWriteMask = 0x0f;
	}

	pipeline_desc.SampleMask = 0xFFFFFFFF;

	pipeline_desc.RasterizerState.FillMode = translate_fill_mode[params->state.fill];
	pipeline_desc.RasterizerState.CullMode = translate_cull_mode[params->state.cull];
	pipeline_desc.RasterizerState.FrontCounterClockwise = params->state.wind == VGPU_WIND_CCW;
	pipeline_desc.RasterizerState.DepthBias = params->state.depth_bias;
	pipeline_desc.RasterizerState.DepthBiasClamp = 0.0f;
	pipeline_desc.RasterizerState.SlopeScaledDepthBias = 0.0f;
	pipeline_desc.RasterizerState.DepthClipEnable = FALSE;
	pipeline_desc.RasterizerState.MultisampleEnable = FALSE;
	pipeline_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	pipeline_desc.RasterizerState.ForcedSampleCount = 0; // TODO
	pipeline_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	pipeline_desc.DepthStencilState.DepthEnable = params->state.depth.enabled;
	pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipeline_desc.DepthStencilState.DepthFunc = translate_compare_func[params->state.depth.func];
	pipeline_desc.DepthStencilState.StencilEnable = params->state.stencil.enabled;
	pipeline_desc.DepthStencilState.StencilReadMask = params->state.stencil.read_mask;
	pipeline_desc.DepthStencilState.StencilWriteMask = params->state.stencil.write_mask;
	pipeline_desc.DepthStencilState.FrontFace.StencilFailOp = translate_stencil_op[params->state.stencil.front.fail_op];
	pipeline_desc.DepthStencilState.FrontFace.StencilDepthFailOp = translate_stencil_op[params->state.stencil.front.depth_fail_op];
	pipeline_desc.DepthStencilState.FrontFace.StencilPassOp = translate_stencil_op[params->state.stencil.front.pass_op];
	pipeline_desc.DepthStencilState.FrontFace.StencilFunc = translate_compare_func[params->state.stencil.front.func];
	pipeline_desc.DepthStencilState.BackFace.StencilFailOp = translate_stencil_op[params->state.stencil.back.fail_op];
	pipeline_desc.DepthStencilState.BackFace.StencilDepthFailOp = translate_stencil_op[params->state.stencil.back.depth_fail_op];
	pipeline_desc.DepthStencilState.BackFace.StencilPassOp = translate_stencil_op[params->state.stencil.back.pass_op];
	pipeline_desc.DepthStencilState.BackFace.StencilFunc = translate_compare_func[params->state.stencil.back.func];

	pipeline_desc.PrimitiveTopologyType = translate_primitive_type_top[params->primitive_type];
	pipeline_desc.NumRenderTargets = 1;
	pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO
	pipeline_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; // TODO
	pipeline_desc.SampleDesc.Count = 1;
	pipeline_desc.NodeMask = 0xFFFFFFFF;

	HRESULT hr = device->d3dd->CreateGraphicsPipelineState(
		&pipeline_desc,
		IID_PPV_ARGS(&pipeline->pipeline_state));
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to create graphics pipeline state");

	pipeline->root_layout = params->root_layout;
	pipeline->primitive_topology = translate_primitive_type[params->primitive_type];

	return pipeline;
}

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline)
{
	SAFE_RELEASE(pipeline->pipeline_state);

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

	for (uint32_t i = 0; i < params->num_color_targets; ++i)
	{
		render_pass->is_framebuffer[i] = (params->color_targets->texture == &device->backbuffer);
		render_pass->has_framebuffer |= render_pass->is_framebuffer[i];
	}

	render_pass->num_rtv = params->num_color_targets;

	if(params->num_color_targets)
	{
		uint32_t end_count = render_pass->has_framebuffer ? VGPU_MULTI_BUFFERING : 1;
		for (uint32_t f = 0; f < end_count; ++f)
		{
			render_pass->rtv_offset[f] = device->rtv_pool.alloc(params->num_color_targets);
			render_pass->rtv[f] = device->rtv_heap->GetCPUDescriptorHandleForHeapStart();
			render_pass->rtv[f].ptr += render_pass->rtv_offset[f] * device->rtv_size;
			for (size_t i = 0; i < params->num_color_targets; ++i)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE desc = render_pass->rtv[f];
				desc.ptr += i * device->rtv_size;
				device->d3dd->CreateRenderTargetView(
					render_pass->is_framebuffer[i] ? device->frame[f].backbuffer_resource : params->color_targets[i].texture->resource,
					nullptr,
					desc);
			}
		}

		for (size_t i = 0; i < params->num_color_targets; ++i)
		{
			memcpy(&render_pass->rtv_clear_value[i], &params->color_targets[i].texture->clear_value, sizeof(D3D12_CLEAR_VALUE));
			render_pass->rtv_texture[i] = params->color_targets[i].texture;
		}
	}

	if(params->depth_stencil_target.texture)
	{
		render_pass->dsv_offset = device->dsv_pool.alloc_handle();
		render_pass->dsv = device->dsv_heap->GetCPUDescriptorHandleForHeapStart();
		render_pass->dsv.ptr += render_pass->dsv_offset * device->dsv_size;
		device->d3dd->CreateDepthStencilView(
			params->depth_stencil_target.texture->resource,
			nullptr,
			render_pass->dsv);
		memcpy(&render_pass->dsv_clear_value, &params->depth_stencil_target.texture->clear_value, sizeof(D3D12_CLEAR_VALUE));
		render_pass->dsv_texture = params->depth_stencil_target.texture;
	}
	else
	{
		render_pass->dsv_offset = -1;
	}

	return render_pass;
}

void vgpu_destroy_render_pass(vgpu_device_t* device, vgpu_render_pass_t* render_pass)
{
	uint32_t end_count = render_pass->has_framebuffer ? VGPU_MULTI_BUFFERING : 1;
	for (uint32_t f = 0; f < end_count; ++f)
	{
		device->rtv_pool.free(render_pass->rtv_offset[f], render_pass->num_rtv);
	}
	if(render_pass->dsv_offset != -1)
		device->dsv_pool.free_handle(render_pass->dsv_offset);
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
	command_list->thread_context = nullptr;
	command_list->d3dcl = nullptr;
	command_list->curr_pipeline = nullptr;
	command_list->curr_root_layout = nullptr;

	return command_list;
}

void vgpu_destroy_command_list(vgpu_device_t* device, vgpu_command_list_t* command_list)
{
	VGPU_ASSERT(device, command_list->d3dcl == nullptr, "Destroying command list while building");
	VGPU_FREE(device->allocator, command_list);
}

bool vgpu_is_command_list_type_supported(vgpu_device_t* device, vgpu_command_list_type_t command_list_type)
{
	return command_list_type != VGPU_COMMAND_LIST_IMMEDIATE_GRAPHICS;
}

void vgpu_begin_command_list(vgpu_thread_context_t* thread_context, vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	size_t id = command_list->device->frame_no % VGPU_MULTI_BUFFERING;

	VGPU_ASSERT(command_list->device, command_list->d3dcl == nullptr, "Command list already begun");
	if (thread_context->frame[id].free.empty())
	{
		// TODO: needs to be rewritten to be one command list on the command_list object to be reset on the specified thread context
		HRESULT hr = command_list->device->d3dd->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			thread_context->frame[id].command_allocator_graphics,
			nullptr,
			IID_PPV_ARGS(&command_list->d3dcl));
		VGPU_ASSERT(command_list->device, SUCCEEDED(hr), "failed to create commandlist");
	}
	else
	{
		command_list->d3dcl = thread_context->frame[id].free.back();
		thread_context->frame[id].free.remove_back();
		command_list->d3dcl->Reset(thread_context->frame[id].command_allocator_graphics, nullptr);
	}

	ID3D12DescriptorHeap* heaps[] = {
		command_list->device->cbv_srv_uav_heap,
		command_list->device->sampler_heap,
	};
	command_list->d3dcl->SetDescriptorHeaps(VGPU_ARRAY_LENGTH(heaps), heaps);

	command_list->thread_context = thread_context;
	command_list->curr_pipeline = nullptr;
	command_list->curr_root_layout = nullptr;
}

void vgpu_end_command_list(vgpu_command_list_t* command_list)
{
	HRESULT hr = command_list->d3dcl->Close();
	VGPU_ASSERT(command_list->device, SUCCEEDED(hr), "failed to close command list");
}

struct gpu_lock_data_internal_t
{
	size_t offset;
	ID3D12Resource* upload_buffer;
};

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params)
{
	vgpu_device_t* device = command_list->device;

	D3D12_RANGE range;
	ID3D12Resource* buffer = nullptr;
	if(params->buffer->heap_type == D3D12_HEAP_TYPE_UPLOAD)
	{
		range.Begin = params->offset;
		range.End = VGPU_ALIGN_UP(params->offset + params->num_bytes, 16);
		buffer = params->buffer->resource;
	}
	else
	{
		//TODO: needs a lock in the device
		uint32_t id = device->frame_no % VGPU_MULTI_BUFFERING;
		if (command_list->thread_context->frame[id].upload_buffer_size <= command_list->thread_context->frame[id].upload_offset + params->num_bytes)
		{
			// Not enough space, we need to reallocate the buffer
			push_delay_delete(device, command_list->thread_context->frame[id].upload_buffer);
			command_list->thread_context->frame[id].upload_buffer_size = VGPU_ALIGN_UP(max(command_list->thread_context->frame[id].upload_buffer_size, params->num_bytes), 0x10000);
			command_list->thread_context->frame[id].upload_buffer = create_upload_buffer(device, command_list->thread_context->frame[id].upload_buffer_size);
		}
		buffer = command_list->thread_context->frame[id].upload_buffer;

		// TODO: take frame lock or move buffer to command_list?
		range.Begin = command_list->thread_context->frame[id].upload_offset;
		range.End = VGPU_ALIGN_UP(command_list->thread_context->frame[id].upload_offset + params->num_bytes, 16);
		command_list->thread_context->frame[id].upload_offset = range.End;
	}

	void* data = nullptr;
	HRESULT hr = buffer->Map(
		0,
		&range,
		&data);
	VGPU_ASSERT(device, SUCCEEDED(hr), "failed to map buffer");

	gpu_lock_data_internal_t internal_data;
	internal_data.offset = range.Begin;
	internal_data.upload_buffer = buffer;

	memcpy(params->unlock_data, &internal_data, sizeof(internal_data));

	return ((uint8_t*)data) + range.Begin;
}

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params)
{
	vgpu_device_t* device = command_list->device;

	gpu_lock_data_internal_t internal_data;
	memcpy(&internal_data, params->unlock_data, sizeof(internal_data));

	D3D12_RANGE range;
	range.Begin = internal_data.offset;
	range.End = internal_data.offset + params->num_bytes;
	internal_data.upload_buffer->Unmap(0, &range);

	if (params->buffer->heap_type == D3D12_HEAP_TYPE_DEFAULT)
	{
		command_list->d3dcl->CopyBufferRegion(
			params->buffer->resource,
			params->offset,
			internal_data.upload_buffer,
			internal_data.offset,
			params->num_bytes);
	}
}

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes)
{
	vgpu_lock_buffer_params_t lock_params;
	lock_params.buffer = buffer;
	lock_params.offset = offset;
	lock_params.num_bytes = num_bytes;

	void* ptr = vgpu_lock_buffer(command_list, &lock_params);
	memcpy(ptr, data, num_bytes);
	vgpu_unlock_buffer(command_list, &lock_params);
}

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table)
{
	if(resource_table->num_cbv_srv_uav)
	{
		command_list->d3dcl->SetGraphicsRootDescriptorTable(
			resource_table->cbv_srv_uav_index,
			resource_table->cbv_srv_uav);
	}
}

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes)
{
	if(command_list->curr_root_layout->slots[slot].treat_as_constant_buffer)
	{
		command_list->d3dcl->SetGraphicsRootConstantBufferView(
			command_list->curr_root_layout->slots[slot].cbv_srv_uav_index,
			buffer->resource->GetGPUVirtualAddress() + offset);
	}
	else
	{
		command_list->d3dcl->SetGraphicsRootShaderResourceView(
			command_list->curr_root_layout->slots[slot].cbv_srv_uav_index,
			buffer->resource->GetGPUVirtualAddress() + offset);
	}
}

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline)
{
	command_list->curr_pipeline = pipeline;
	command_list->curr_root_layout = pipeline->root_layout;
	command_list->d3dcl->SetPipelineState(pipeline->pipeline_state);
	command_list->d3dcl->IASetPrimitiveTopology(pipeline->primitive_topology);
	command_list->d3dcl->SetGraphicsRootSignature(pipeline->root_layout->root_signature);
}

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer)
{
	D3D12_INDEX_BUFFER_VIEW view;
	view.BufferLocation = index_buffer->resource->GetGPUVirtualAddress();
	view.SizeInBytes = index_buffer->num_bytes;
	view.Format = index_type == VGPU_DATA_TYPE_UINT32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	command_list->d3dcl->IASetIndexBuffer(&view);
}

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices)
{
	VGPU_ASSERT(command_list->device, command_list->curr_pipeline != nullptr, "A valid pipeline was not set when drawing");
	VGPU_ASSERT(command_list->device, command_list->curr_root_layout != nullptr, "A valid root layout was not set when drawing");

	command_list->d3dcl->DrawInstanced(num_vertices, num_instances, first_vertex, first_instance);
}

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex)
{
	VGPU_ASSERT(command_list->device, command_list->curr_pipeline != nullptr, "A valid pipeline was not set when drawing");
	VGPU_ASSERT(command_list->device, command_list->curr_root_layout != nullptr, "A valid root layout was not set when drawing");

	command_list->d3dcl->DrawIndexedInstanced(num_indices, num_instances, first_index, first_vertex, first_instance);
}

void vgpu_draw_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	VGPU_ASSERT(command_list->device, command_list->curr_pipeline != nullptr, "A valid pipeline was not set when drawing");
	VGPU_ASSERT(command_list->device, command_list->curr_root_layout != nullptr, "A valid root layout was not set when drawing");

	command_list->d3dcl->ExecuteIndirect(
		command_list->device->draw_indirect_signature,
		count,
		buffer->resource,
		offset,
		nullptr,
		0);
}

void vgpu_draw_indexed_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	VGPU_ASSERT(command_list->device, command_list->curr_pipeline != nullptr, "A valid pipeline was not set when drawing");
	VGPU_ASSERT(command_list->device, command_list->curr_root_layout != nullptr, "A valid root layout was not set when drawing");

	command_list->curr_root_layout = nullptr;
	command_list->d3dcl->ExecuteIndirect(
		command_list->device->draw_indexed_indirect_signature,
		count,
		buffer->resource,
		offset,
		nullptr,
		0);
}

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	uint64_t id = vgpu_get_frame_id(command_list->device);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = render_pass->rtv[render_pass->has_framebuffer ? id : 0];
	for (int i = 0; i < render_pass->num_rtv; ++i)
	{
		rtv.ptr += i * command_list->device->rtv_size;
		command_list->d3dcl->ClearRenderTargetView(
			rtv,
			render_pass->rtv_clear_value[i].Color,
			0,
			nullptr);
	}

	if (render_pass->dsv_offset != -1)
	{
		command_list->d3dcl->ClearDepthStencilView(
			render_pass->dsv,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			render_pass->dsv_clear_value.DepthStencil.Depth,
			render_pass->dsv_clear_value.DepthStencil.Stencil,
			0,
			nullptr);
	}
}

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	vgpu_clear_render_pass(command_list, render_pass);

	uint64_t id = vgpu_get_frame_id(command_list->device);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = render_pass->rtv[render_pass->has_framebuffer ? id : 0];
	command_list->d3dcl->OMSetRenderTargets(
		render_pass->num_rtv,
		&rtv,
		true,
		render_pass->dsv_offset != -1 ? &render_pass->dsv : nullptr);

	D3D12_RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = command_list->device->width;
	rect.bottom = command_list->device->height;
	command_list->d3dcl->RSSetScissorRects(1, &rect);

	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = command_list->device->width;
	viewport.Height = command_list->device->height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	command_list->d3dcl->RSSetViewports(1, &viewport);
}

void vgpu_transition_resource(vgpu_command_list_t* command_list, vgpu_resource_t* resource, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
	D3D12_RESOURCE_BARRIER barrier_desc;
	ZeroMemory(&barrier_desc, sizeof(barrier_desc));
	barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_desc.Transition.pResource = resource->resource;
	barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_desc.Transition.StateBefore = (D3D12_RESOURCE_STATES)state_before;
	barrier_desc.Transition.StateAfter = (D3D12_RESOURCE_STATES)state_after;

	command_list->d3dcl->ResourceBarrier(1, &barrier_desc);
}

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
	vgpu_transition_resource(command_list, buffer, state_before, state_after);
}

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
	if (texture == &command_list->device->backbuffer)
	{
		vgpu_texture_t tmp_texture = *texture;
		tmp_texture.resource = curr_frame(command_list->device).backbuffer_resource;
	}
	else
	{
		vgpu_transition_resource(command_list, texture, state_before, state_after);
	}
}
