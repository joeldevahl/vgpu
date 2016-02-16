#include <stdio.h>
#include <memory.h>

#include <vgpu.h>
#include "vgpu_internal.h"
#include "vgpu_array.h"

#include <cstring>

#if defined(VGPU_WINDOWS)
#	define VK_USE_PLATFORM_WIN32_KHR
#	define _WIN32_WINNT 0x0600
#	include <windows.h>
#endif

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vk_sdk_platform.h>

#define VGPU_VK_GET_INSTANCE_PROC_ADDR(device, entrypoint)                        \
{                                                                       \
    device->vk##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(device->vk_instance, "vk"#entrypoint); \
    if (device->vk##entrypoint == nullptr) {                                 \
       VGPU_ASSERT(device, false, "vkGetInstanceProcAddr failed to find vk"#entrypoint,  \
                 "vkGetInstanceProcAddr Failure");                      \
    }                                                                   \
}

#define VGPU_VK_GET_DEVICE_PROC_ADDR(device, entrypoint)                           \
{                                                                       \
    device->vk##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(device->vk_device, "vk"#entrypoint);   \
    if (device->vk##entrypoint == nullptr) {                                 \
        VGPU_ASSERT(device, false, "vkGetDeviceProcAddr failed to find vk"#entrypoint,    \
                 "vkGetDeviceProcAddr Failure");                        \
    }                                                                   \
}

#define VGPU_VK_SETUP_TYPE(s, type) do { (s).sType = (type); (s).pNext = nullptr; } while (0);
#define VGPU_VK_ADD_TYPE(type) (type), nullptr

/******************************************************************************\
*
*  Translation utils
*
\******************************************************************************/

static const VkFormat translate_textureformat[] = {
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
	VK_FORMAT_BC2_UNORM_BLOCK,
	VK_FORMAT_BC3_UNORM_BLOCK,

	VK_FORMAT_D32_SFLOAT_S8_UINT,
};

static const VkIndexType translate_indextype[] = {
	VK_INDEX_TYPE_MAX_ENUM,
	VK_INDEX_TYPE_UINT32,
	VK_INDEX_TYPE_UINT16,
	VK_INDEX_TYPE_MAX_ENUM,
};

/******************************************************************************\
 *
 *  Structures
 *
\******************************************************************************/

struct vgpu_buffer_s
{
	VkBuffer buffer;
	VkDeviceMemory mem;
};

struct vgpu_resource_table_s
{
};

struct vgpu_root_layout_s
{
	VkPipelineLayout pipeline_layout;
};

struct vgpu_texture_s
{
	VkImage image;
	VkDeviceMemory mem;
	VkFormat format;
	VkClearValue clear_value;
};

struct vgpu_program_s
{
	VkShaderModule shader_module;
	vgpu_program_type_t program_type;
};

struct vgpu_pipeline_s
{
	VkPipeline vk_pipeline;
};

struct vgpu_render_pass_s
{
	VkRenderPass render_pass;
	VkFramebuffer framebuffer[VGPU_MULTI_BUFFERING];

	size_t num_color_targets;
	vgpu_texture_t* color_targets[16];
	vgpu_texture_t* depth_stencil_target;

	VkImageView color_image_views[16];
	VkImageView depth_stencil_image_view;
	VkImageView backbuffer_image_views[VGPU_MULTI_BUFFERING];
	bool is_framebuffer[16];
	bool has_framebuffer;

	VkClearValue clear_values[16 + 1];
};

struct vgpu_command_list_s
{
	vgpu_device_t* device;
	vgpu_thread_context_t* thread_context;

	VkCommandBuffer command_buffer;
	
	vgpu_render_pass_t* curr_pass;
	bool present_semaphore_needed;
};

struct vgpu_thread_context_s
{
	VkCommandPool command_pool[VGPU_MULTI_BUFFERING];

	vgpu_array_t<VkCommandBuffer> free[VGPU_MULTI_BUFFERING];
	vgpu_array_t<VkCommandBuffer> pending[VGPU_MULTI_BUFFERING];
};

struct vgpu_device_s
{
	vgpu_allocator_t* allocator;
	VkAllocationCallbacks vk_allocator;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;

	VkInstance vk_instance;
	VkPhysicalDevice vk_gpu;
	VkDevice vk_device;
	VkQueue vk_queue;
	VkSurfaceKHR vk_surface;
	VkPhysicalDeviceMemoryProperties memory_props;
	VkPhysicalDeviceProperties device_props;
	VkQueueFamilyProperties queue_props[8];
	uint32_t queue_count;
	uint32_t graphics_queue_node_index;

	uint64_t frame_no;
	vgpu_caps_t caps;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	VkDebugReportCallbackEXT debug_callback;

	VkFormat format;
	VkColorSpaceKHR color_space;

	VkSwapchainKHR swapchain;
	VkImage swapchain_image[VGPU_MULTI_BUFFERING];
	vgpu_texture_t backbuffer;
	VkSemaphore present_semaphore[VGPU_MULTI_BUFFERING];
	uint32_t swapchain_image_index;
	bool present_semaphore_waited_on;
};

/******************************************************************************\
 *
 *  Device operations
 *
\******************************************************************************/

static void* VKAPI_CALL vgpu_vk_alloc(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope)
{
	vgpu_allocator_t* allocator = (vgpu_allocator_t*)user_data;
	return VGPU_ALLOC(allocator, size, alignment);
}

static void* VKAPI_CALL vgpu_vk_realloc(void* user_data, void* original_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope)
{
	vgpu_allocator_t* allocator = (vgpu_allocator_t*)user_data;
	return VGPU_REALLOC(allocator, original_data, size, alignment);
}

static void VKAPI_CALL vgpu_vk_free(void* user_data, void* ptr)
{
	vgpu_allocator_t* allocator = (vgpu_allocator_t*)user_data;
	VGPU_FREE(allocator, ptr);
}

static VkBool32 VKAPI_PTR vgpu_vk_debug_func(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  object_type,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     message_code,
	const char*                                 layer_prefix,
	const char*                                 message,
	void*                                       user_data)
{
	const char* severity = nullptr;
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		severity = "ERROR: ";
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		severity = "WARNING: ";
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		severity = "PERFORMANCE WARNING: ";
	else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		severity = "INFO: ";
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		severity = "DEBUG: ";
	else
		severity = "";

	// TODO: 

	return false;
}

static uint32_t vgpu_vk_memory_type_from_properties(vgpu_device_t* device, uint32_t type_bits, VkFlags requirements_mask)
{
	for (uint32_t i = 0; i < 32; i++) {
		if ((type_bits & 1) == 1) {
			if ((device->memory_props.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
				return i;
			}
		}
		type_bits >>= 1;
	}
	VGPU_ASSERT(device, false, "Failed to find memory type");
	return 0;
}

vgpu_device_t* vgpu_create_device(const vgpu_create_device_params_t* params)
{
	extern vgpu_allocator_t vgpu_allocator_default;

	vgpu_allocator_t* allocator = params->allocator ? params->allocator : &vgpu_allocator_default;
	vgpu_device_t* device = VGPU_ALLOC_TYPE(allocator, vgpu_device_t);
	device->allocator = allocator;

	device->vk_allocator.pUserData = device->allocator;
	device->vk_allocator.pfnAllocation = vgpu_vk_alloc;
	device->vk_allocator.pfnReallocation = vgpu_vk_realloc;
	device->vk_allocator.pfnFree = vgpu_vk_free;
	device->vk_allocator.pfnInternalAllocation = nullptr;
	device->vk_allocator.pfnInternalFree = nullptr;

	device->log_func = params->log_func;
	device->error_func = params->error_func;

	device->frame_no = 0;
	memset(&device->caps, 0, sizeof(device->caps));
	device->caps.flags = (~params->force_disable_flags) & (VGPU_CAPS_FLAG_BIND_CONSTANT_BUFFER_AT_OFFSET | VGPU_CAPS_FLAG_BIND_BUFFER_AT_OFFSET);

	VkResult res = VK_SUCCESS;

	VkApplicationInfo app =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_APPLICATION_INFO),
		"atec",
		0,
		"atec",
		0,
		VK_API_VERSION,
	};

	const char* instance_layers[] =
	{
		"VK_LAYER_LUNARG_Threading",
		"VK_LAYER_LUNARG_DrawState",
		"VK_LAYER_LUNARG_Image",
		"VK_LAYER_LUNARG_MemTracker",
		"VK_LAYER_LUNARG_ObjectTracker",
		"VK_LAYER_LUNARG_ParamChecker",
	};

	const char* instance_extensions[] =
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VGPU_WINDOWS)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
#		error not implemented for this platform
#endif
	};

	VkInstanceCreateInfo inst_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO),
		0,
		&app,
		VGPU_ARRAY_LENGTH(instance_layers),
		instance_layers,
		VGPU_ARRAY_LENGTH(instance_extensions),
		instance_extensions,
	};

	res = vkCreateInstance(&inst_info, &device->vk_allocator, &device->vk_instance);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create instance");

	uint32_t num_physical_devices;
	VkPhysicalDevice physical_devices[64];
	res = vkEnumeratePhysicalDevices(device->vk_instance, &num_physical_devices, nullptr);
	VGPU_ASSERT(device, res == VK_SUCCESS && num_physical_devices > 0, "Failed to get physical device count");
	VGPU_ASSERT(device, num_physical_devices <= VGPU_ARRAY_LENGTH(physical_devices), "Too many physical devices");
	res = vkEnumeratePhysicalDevices(device->vk_instance, &num_physical_devices, physical_devices);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to enumerate physical devices");
	device->vk_gpu = physical_devices[0]; // TODO: select device

	const char* device_layers[] =
	{
		"VK_LAYER_LUNARG_Threading",
		"VK_LAYER_LUNARG_DrawState",
		"VK_LAYER_LUNARG_Image",
		"VK_LAYER_LUNARG_MemTracker",
		"VK_LAYER_LUNARG_ObjectTracker",
		"VK_LAYER_LUNARG_ParamChecker",
	};

	const char* device_extensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	vkGetPhysicalDeviceQueueFamilyProperties(device->vk_gpu, &device->queue_count, NULL);
	VGPU_ASSERT(device, device->queue_count <= VGPU_ARRAY_LENGTH(device->queue_props), "Too many physical device queue family properties");
	vkGetPhysicalDeviceQueueFamilyProperties(device->vk_gpu, &device->queue_count, device->queue_props);

	bool queue_found = false;
	for (uint32_t i = 0; i < device->queue_count; ++i)
	{
		if (device->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			device->graphics_queue_node_index = i;
			queue_found = true;
			break;
		}
	}
	VGPU_ASSERT(device, queue_found, "Could not find a graphics queue");

	vkGetPhysicalDeviceMemoryProperties(device->vk_gpu, &device->memory_props);
	vkGetPhysicalDeviceProperties(device->vk_gpu, &device->device_props);

	float queue_priorities[1] = { 0.0 };
	VkDeviceQueueCreateInfo queue_create_infos[] =
	{
		{
			VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO),
			0,
			device->graphics_queue_node_index,
			VGPU_ARRAY_LENGTH(queue_priorities),
			queue_priorities
		},
	};

	VkDeviceCreateInfo device_create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO),
		0,
		VGPU_ARRAY_LENGTH(queue_create_infos),
		queue_create_infos,
		VGPU_ARRAY_LENGTH(device_layers),
		device_layers,
		VGPU_ARRAY_LENGTH(device_extensions),
		device_extensions,
		nullptr,
	};
	res = vkCreateDevice(device->vk_gpu, &device_create_info, &device->vk_allocator, &device->vk_device);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create device");

	VGPU_VK_GET_INSTANCE_PROC_ADDR(device, CreateDebugReportCallbackEXT);
	VGPU_VK_GET_INSTANCE_PROC_ADDR(device, DestroyDebugReportCallbackEXT);

	VkDebugReportCallbackCreateInfoEXT debug_callback_create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT),
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
		vgpu_vk_debug_func,
		device,
	};
	res = device->vkCreateDebugReportCallbackEXT(device->vk_instance, &debug_callback_create_info, &device->vk_allocator, &device->debug_callback);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed create message callback");

#if defined(VGPU_WINDOWS)
	VkWin32SurfaceCreateInfoKHR create_surface_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR),
		0, // flags
		GetModuleHandle(NULL), // TODO: not compatible with DLLs
		(HWND)params->window,
	};
	res = vkCreateWin32SurfaceKHR(device->vk_instance, &create_surface_info, &device->vk_allocator, &device->vk_surface);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create device");
#else
#	error "Not implemented for this platform"
#endif

	VkBool32 supports_present[VGPU_ARRAY_LENGTH(device->queue_props)];
	for(uint32_t i = 0; i < device->queue_count; i++)
		vkGetPhysicalDeviceSurfaceSupportKHR(device->vk_gpu, i, device->vk_surface, &supports_present[i]);

	uint32_t graphics_queue_node_index = UINT32_MAX;
	uint32_t present_queue_node_index = UINT32_MAX;
	for (uint32_t i = 0; i < device->queue_count; i++)
	{
		if ((device->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphics_queue_node_index == UINT32_MAX)
				graphics_queue_node_index = i;

			if (supports_present[i] == VK_TRUE)
			{
				graphics_queue_node_index = i;
				present_queue_node_index = i;
				break;
			}
		}
	}
	if (present_queue_node_index == UINT32_MAX)
	{
		for (uint32_t i = 0; i < device->queue_count; ++i)
		{
			if (supports_present[i] == VK_TRUE)
			{
				present_queue_node_index = i;
				break;
			}
		}
	}
	VGPU_ASSERT(device, graphics_queue_node_index != UINT32_MAX && present_queue_node_index != UINT32_MAX, "Could not find a graphics and a present queue");
	VGPU_ASSERT(device, graphics_queue_node_index == present_queue_node_index, "Could not find a common graphics and a present queue");

	device->graphics_queue_node_index = graphics_queue_node_index;
	vkGetDeviceQueue(device->vk_device, device->graphics_queue_node_index, 0, &device->vk_queue);

	// Get the list of VkFormat's that are supported:
	VkSurfaceFormatKHR surf_formats[64];
	uint32_t format_count;
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(device->vk_gpu, device->vk_surface, &format_count, nullptr);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get surface format count");
	VGPU_ASSERT(device, format_count <= VGPU_ARRAY_LENGTH(surf_formats), "Too many surface formats");
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(device->vk_gpu, device->vk_surface, &format_count, surf_formats);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get surface formats");
	VGPU_ASSERT(device, format_count > 0, "No color formats found");
	device->format = surf_formats[0].format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : surf_formats[0].format;
	device->color_space = surf_formats[0].colorSpace;

	VkSurfaceCapabilitiesKHR surface_capabilities;
	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->vk_gpu, device->vk_surface, &surface_capabilities);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get surface capabilities");

	uint32_t present_mode_count;
	VkPresentModeKHR present_modes[VK_PRESENT_MODE_RANGE_SIZE];
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(device->vk_gpu, device->vk_surface, &present_mode_count, nullptr);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get surface present mode count");
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(device->vk_gpu, device->vk_surface, &present_mode_count, present_modes);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get surface present modes");

	VkExtent2D swapchain_extent;
	if (surface_capabilities.currentExtent.width == -1)
	{
		swapchain_extent.width = 1280;
		swapchain_extent.height = 720;
	}
	else
	{
		swapchain_extent = surface_capabilities.currentExtent;
	}

	VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (size_t i = 0; i < present_mode_count; i++)
	{
		if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if ((swapchain_present_mode != VK_PRESENT_MODE_MAILBOX_KHR) && (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
		{
			swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	uint32_t num_swapchain_images = VGPU_MULTI_BUFFERING;
	if ((surface_capabilities.minImageCount > 0) && (num_swapchain_images < surface_capabilities.minImageCount))
		num_swapchain_images = surface_capabilities.minImageCount;
	if ((surface_capabilities.maxImageCount > 0) && (num_swapchain_images > surface_capabilities.maxImageCount))
		num_swapchain_images = surface_capabilities.maxImageCount;
	VGPU_ASSERT(device, num_swapchain_images == VGPU_MULTI_BUFFERING, "Buffer count must match for now"); // TODO:

	VkSurfaceTransformFlagBitsKHR pre_transform;
	if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else {
		pre_transform = surface_capabilities.currentTransform;
	}

	VkSwapchainCreateInfoKHR swapchain_create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR),
		0,
		device->vk_surface,
		num_swapchain_images,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		swapchain_extent,
		1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		1,
		&device->graphics_queue_node_index,
		pre_transform,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		swapchain_present_mode,
		true,
		VK_NULL_HANDLE,
	};
	res = vkCreateSwapchainKHR(device->vk_device, &swapchain_create_info, &device->vk_allocator, &device->swapchain);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create swapchain");

	uint32_t swapchain_image_count = 0;
	res = vkGetSwapchainImagesKHR(device->vk_device, device->swapchain, &swapchain_image_count, nullptr);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get swapchain image count");
	VGPU_ASSERT(device, swapchain_image_count == VGPU_MULTI_BUFFERING, "Wrong swapchain image count");

	res = vkGetSwapchainImagesKHR(device->vk_device, device->swapchain, &swapchain_image_count, device->swapchain_image);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to get swapchain images");

	const float clear_color[4] = { 0.1f, 0.1f, 0.3f, 1.0f };
	memset(&device->backbuffer, 0, sizeof(device->backbuffer));
	device->backbuffer.image = VK_NULL_HANDLE;
	device->backbuffer.format = swapchain_create_info.imageFormat;
	memcpy(device->backbuffer.clear_value.color.float32, clear_color, sizeof(device->backbuffer.clear_value.color.float32));

	VkSemaphoreCreateInfo semaphore_create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO),
		0,
	};
	for (uint32_t i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		res = vkCreateSemaphore(device->vk_device, &semaphore_create_info, &device->vk_allocator, &device->present_semaphore[i]);
		VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create semaphore");
	}
	res = vkAcquireNextImageKHR(device->vk_device, device->swapchain, UINT64_MAX, device->present_semaphore[0], VK_NULL_HANDLE, &device->swapchain_image_index);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to acquire next swapchain image");

	return device;
}

void vgpu_destroy_device(vgpu_device_t* device)
{
	vkDestroySwapchainKHR(device->vk_device, device->swapchain, &device->vk_allocator);
	device->vkDestroyDebugReportCallbackEXT(device->vk_instance, device->debug_callback, &device->vk_allocator);
	vkDestroyDevice(device->vk_device, &device->vk_allocator);
	vkDestroyInstance(device->vk_instance, &device->vk_allocator);
	VGPU_FREE(device->allocator, device);
}

vgpu_device_type_t vgpu_get_device_type(vgpu_device_t* device)
{
	return VGPU_DEVICE_VK;
}

void vgpu_apply_command_lists(vgpu_device_t* device, uint32_t num_command_lists, vgpu_command_list_t** command_lists, uint32_t queue)
{
	size_t id = device->frame_no % VGPU_MULTI_BUFFERING;

	VGPU_ASSERT(device, queue == 0, "Only queue 0 supported for now");
	VGPU_ASSERT(device, num_command_lists > 0, "No command lists to apply");

	VkCommandBuffer command_buffers[128];
	bool present_semaphore_needed = false;
	VGPU_ASSERT(device, num_command_lists <= VGPU_ARRAY_LENGTH(command_buffers), "Too many command lists to apply");
	for (uint32_t i = 0; i < num_command_lists; ++i)
	{
		command_buffers[i] = command_lists[i]->command_buffer;
		present_semaphore_needed |= command_lists[i]->present_semaphore_needed;
		command_lists[i]->thread_context->pending[id].append(command_lists[i]->command_buffer);
		command_lists[i]->command_buffer = VK_NULL_HANDLE;
		command_lists[i]->thread_context = nullptr;
	}
	
	if (present_semaphore_needed)
	{
		if (device->present_semaphore_waited_on)
			present_semaphore_needed = false;
		else
			device->present_semaphore_waited_on = true;
	}

	VkSubmitInfo info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_SUBMIT_INFO),
		present_semaphore_needed ? 1u : 0u,
		present_semaphore_needed ? &device->present_semaphore[id] : nullptr,
		nullptr,
		num_command_lists,
		command_buffers,
		0u,
		nullptr,
	};
	VkResult res = vkQueueSubmit(device->vk_queue, 1, &info, VK_NULL_HANDLE);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to submit command buffer");
}

void vgpu_present(vgpu_device_t* device)
{
	uint32_t current_buffer = device->frame_no % VGPU_MULTI_BUFFERING;
	VkPresentInfoKHR present_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR),
		0,
		nullptr,
		1,
		&device->swapchain,
		&current_buffer,
		nullptr,
	};
	VkResult res = vkQueuePresentKHR(device->vk_queue, &present_info);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to queue present");

	device->frame_no++;

	res = vkAcquireNextImageKHR(device->vk_device, device->swapchain, UINT64_MAX, device->present_semaphore[device->frame_no % VGPU_MULTI_BUFFERING], VK_NULL_HANDLE, &device->swapchain_image_index);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to acquire next swapchain image");
	device->present_semaphore_waited_on = false;
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

	VkCommandPoolCreateInfo cmd_pool_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO),
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		device->graphics_queue_node_index
	};
	for (uint32_t i = 0; i < VGPU_MULTI_BUFFERING; ++i)
	{
		VkResult res = vkCreateCommandPool(device->vk_device, &cmd_pool_info, &device->vk_allocator, &thread_context->command_pool[i]);
		VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create command pool");

		thread_context->free[i].create(device->allocator, 8);
		thread_context->pending[i].create(device->allocator, 8);
	}

	return thread_context;
}

void vgpu_destroy_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
	// TODO: wait for completion?
	for (uint32_t i = 0; i < VGPU_MULTI_BUFFERING; ++i)
		vkDestroyCommandPool(device->vk_device, thread_context->command_pool[i], &device->vk_allocator);

	VGPU_DELETE(device->allocator, vgpu_thread_context_t, thread_context);
}

void vgpu_prepare_thread_context(vgpu_device_t* device, vgpu_thread_context_t* thread_context)
{
	size_t id = device->frame_no % VGPU_MULTI_BUFFERING;
	VkResult res = vkResetCommandPool(device->vk_device, thread_context->command_pool[id], 0);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to reset command pool");

	// TODO: we would wait for a fence on the whole frame here
	while (thread_context->pending[id].any())
	{
		VkCommandBuffer command_buffer = thread_context->pending[id].back();
		thread_context->pending[id].remove_back();
		thread_context->free[id].append(command_buffer);
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

	VkBufferCreateInfo create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO),
		0,
		params->num_bytes,
		0,
		VK_SHARING_MODE_EXCLUSIVE,
		1,
		&device->graphics_queue_node_index,
	};

	if (params->flags & VGPU_BUFFER_FLAG_INDEX_BUFFER)
		create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (params->flags & VGPU_BUFFER_FLAG_CONSTANT_BUFFER)
		create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VkResult res = vkCreateBuffer(device->vk_device, &create_info, &device->vk_allocator, &buffer->buffer);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create buffer");

	VkMemoryRequirements memory_req;
	vkGetBufferMemoryRequirements(device->vk_device, buffer->buffer, &memory_req);

	VkMemoryAllocateInfo alloc_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
		memory_req.size,
		vgpu_vk_memory_type_from_properties(device, memory_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
	};
	res = vkAllocateMemory(device->vk_device, &alloc_info, &device->vk_allocator, &buffer->mem);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to allocate buffer memory");

	res = vkBindBufferMemory(device->vk_device, buffer->buffer, buffer->mem, 0);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to bind buffer memory");

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
	texture->format = translate_textureformat[params->format];

	bool is_depth_stencil_format = texture->format == VK_FORMAT_D32_SFLOAT_S8_UINT;

	VkImageCreateInfo create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO),
		0,
		VK_IMAGE_TYPE_2D,
		texture->format,
		{ params->width, params->height, 0 },
		1,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		0,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		VK_IMAGE_LAYOUT_GENERAL,
	};

	if (params->is_render_target)
	{
		if (is_depth_stencil_format)
		{
			create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			create_info.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			create_info.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	VkResult res = vkCreateImage(device->vk_device, &create_info, &device->vk_allocator, &texture->image);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create image");

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(device->vk_device, texture->image, &mem_reqs);

	VkMemoryAllocateInfo mem_alloc =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
		mem_reqs.size,
		vgpu_vk_memory_type_from_properties(device, mem_reqs.memoryTypeBits, 0),
	};
	res = vkAllocateMemory(device->vk_device, &mem_alloc, NULL, &texture->mem);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to allocate memory for image");

	res = vkBindImageMemory(device->vk_device, texture->image, texture->mem, 0);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to bind memory for image");

	memcpy(texture->clear_value.color.float32, params->clear_value.color, sizeof(texture->clear_value.color.float32));

	return texture;
}

void vgpu_destroy_texture(vgpu_device_t* device, vgpu_texture_t* texture)
{
	vkDestroyImage(device->vk_device, texture->image, &device->vk_allocator);
	vkFreeMemory(device->vk_device, texture->mem, &device->vk_allocator);
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

	VkShaderModuleCreateInfo create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO),
		0,
		params->size,
		(const uint32_t*)params->data,
	};
	VkResult res = vkCreateShaderModule(device->vk_device, &create_info, &device->vk_allocator, &program->shader_module);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create shader module");

	program->program_type = params->program_type;

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

	uint32_t num_stages = 0;
	VkPipelineShaderStageCreateInfo stage_info[2];
	if (params->vertex_program)
	{
		VGPU_VK_SETUP_TYPE(stage_info[num_stages], VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
		stage_info[num_stages].flags = 0;
		stage_info[num_stages].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_info[num_stages].module = params->vertex_program->shader_module;
		stage_info[num_stages].pName = "" ;
		stage_info[num_stages].pSpecializationInfo = nullptr;
		num_stages += 1;
	}
	if (params->fragment_program)
	{
		VGPU_VK_SETUP_TYPE(stage_info[num_stages], VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
		stage_info[num_stages].flags = 0;
		stage_info[num_stages].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_info[num_stages].module = params->fragment_program->shader_module;
		stage_info[num_stages].pName = "";
		stage_info[num_stages].pSpecializationInfo = nullptr;
		num_stages += 1;
	}

	VkPipelineVertexInputStateCreateInfo vertex_input_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO),
		0,
		0,
		nullptr,
		0,
		nullptr,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO),
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE,
	};

	VkViewport viewport =
	{
		0.0f,
		0.0f,
		1280.0f,
		720.0f,
		0.0f,
		1.0f,
	};
	VkPipelineViewportStateCreateInfo viewport_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO),
		0,
		1,
		&viewport,
		0,
		nullptr,
	};

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO),
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
	};

	VkSampleMask sample_mask[] =
	{
		0,
	};
	VkPipelineMultisampleStateCreateInfo multisample_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO),
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		sample_mask,
		VK_FALSE,
		VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO),
		0,
		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		VK_FALSE,
		VK_FALSE,
		{
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP,
			VK_COMPARE_OP_ALWAYS,
			0,
			0,
			0,
		},
		{
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP,
			VK_COMPARE_OP_ALWAYS,
			0,
			0,
			0,
		},
		0.0f,
		1.0f,
	};

	VkPipelineColorBlendAttachmentState attachments[] =
	{
		{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		}
	};
	VkPipelineColorBlendStateCreateInfo color_blend_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO),
		0,
		VK_FALSE,
		VK_LOGIC_OP_CLEAR,
		VGPU_ARRAY_LENGTH(attachments),
		attachments,
		{ 0.0f, 0.0f, 0.0f, 0.0f },
	};

	//VkDynamicState dynamic_states[] = {};
	VkPipelineDynamicStateCreateInfo dynamic_state =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO),
		0,
		0, //VGPU_ARRAY_LENGTH(dynamic_states),
		nullptr, //dynamic_states,
	};

	VkGraphicsPipelineCreateInfo create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO),
		0,
		num_stages,
		stage_info,
		&vertex_input_state,
		&input_assembly_state,
		nullptr, // tessellation_state
		&viewport_state,
		&rasterization_state,
		&multisample_state,
		&depth_stencil_state,
		&color_blend_state,
		&dynamic_state,
		params->root_layout->pipeline_layout,
		params->render_pass->render_pass,
		0, // subpass
		VK_NULL_HANDLE, // base_pipeline_handle
		0, // base_pipeline_index
	};
	VkResult res = vkCreateGraphicsPipelines(device->vk_device, VK_NULL_HANDLE, 1, &create_info, &device->vk_allocator, &pipeline->vk_pipeline);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create graphics pipeline");

	return pipeline;
}

void vgpu_destroy_pipeline(vgpu_device_t* device, vgpu_pipeline_t* pipeline)
{
	vkDestroyPipeline(device->vk_device, pipeline->vk_pipeline, &device->vk_allocator);
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
	memset(render_pass, 0, sizeof(*render_pass));
	VkResult res = VK_SUCCESS;

	VkAttachmentDescription attachments[VGPU_ARRAY_LENGTH(params->color_targets) + 1];
	VkAttachmentReference attachment_references[VGPU_ARRAY_LENGTH(params->color_targets) + 1];

	uint32_t attachment_count = params->num_color_targets;
	for (uint32_t i = 0; i < attachment_count; ++i)
	{
		render_pass->is_framebuffer[i] = (params->color_targets->texture == &device->backbuffer);
		render_pass->has_framebuffer |= render_pass->is_framebuffer[i];
	}

	for (uint32_t i = 0; i < attachment_count; ++i)
	{
		attachments[i].flags = 0;
		attachments[i].format = params->color_targets[i].texture->format;
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp = params->color_targets[i].clear_on_bind ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachment_references[i].attachment = i;
		attachment_references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkImageViewCreateInfo image_view_create_info =
		{
			VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO),
			0,
			params->color_targets[i].texture->image,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R8G8B8A8_UNORM,
			{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			{
				VK_IMAGE_ASPECT_COLOR_BIT,
				0,
				1,
				0,
				1,
			},
		};

		if (render_pass->is_framebuffer[i])
		{
			for (uint32_t f = 0; f < VGPU_MULTI_BUFFERING; ++f)
			{
				image_view_create_info.image = device->swapchain_image[f];
				res = vkCreateImageView(device->vk_device, &image_view_create_info, &device->vk_allocator, &render_pass->backbuffer_image_views[f]);
				VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create image view");
			}
		}
		else
		{
			res = vkCreateImageView(device->vk_device, &image_view_create_info, &device->vk_allocator, &render_pass->color_image_views[i]);
			VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create image view");
		}

		render_pass->color_targets[i] = params->color_targets[i].texture;

		render_pass->clear_values[i] = params->color_targets[i].texture->clear_value;
	}
	render_pass->num_color_targets = params->num_color_targets;

	if (params->depth_stencil_target.texture)
	{
		attachments[attachment_count].flags = 0;
		attachments[attachment_count].format = params->depth_stencil_target.texture->format;
		attachments[attachment_count].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[attachment_count].loadOp = params->depth_stencil_target.clear_on_bind ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachment_count].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[attachment_count].stencilLoadOp = params->depth_stencil_target.clear_on_bind ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachment_count].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[attachment_count].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[attachment_count].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachment_references[attachment_count].attachment = attachment_count;
		attachment_references[attachment_count].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkImageViewCreateInfo image_view_create_info =
		{
			VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO),
			0,
			params->depth_stencil_target.texture->image,
			VK_IMAGE_VIEW_TYPE_2D,
			params->depth_stencil_target.texture->format,
			{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			{
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				0,
			1,
			0,
			1,
			},
		};
		res = vkCreateImageView(device->vk_device, &image_view_create_info, &device->vk_allocator, &render_pass->depth_stencil_image_view);
		VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create image view");

		render_pass->clear_values[attachment_count] = params->depth_stencil_target.texture->clear_value;

		attachment_count += 1;
		render_pass->depth_stencil_target = params->depth_stencil_target.texture;
	}

	VkSubpassDescription subpass =
	{
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		(uint32_t)params->num_color_targets,
		attachment_references,
		nullptr,
		params->depth_stencil_target.texture ? attachment_references + params->num_color_targets : nullptr,
		0,
		nullptr,
	};

	VkRenderPassCreateInfo pass_create_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO),
		0,
		attachment_count,
		attachments,
		1,
		&subpass,
		0,
		nullptr,
	};

	res = vkCreateRenderPass(device->vk_device, &pass_create_info, &device->vk_allocator, &render_pass->render_pass);
	VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create render pass");

	uint32_t end_count = render_pass->has_framebuffer ? VGPU_MULTI_BUFFERING : 1;
	for (uint32_t f = 0; f < end_count; ++f)
	{
		VkImageView image_views[15 + VGPU_MULTI_BUFFERING + 1] = {};
		uint32_t i = 0;
		for (i = 0; i < params->num_color_targets; ++i)
			image_views[i] = render_pass->is_framebuffer[i] ? render_pass->backbuffer_image_views[f] : render_pass->color_image_views[i];
		if (params->depth_stencil_target.texture)
			image_views[i++] = render_pass->depth_stencil_image_view;
		VGPU_ASSERT(device, i == attachment_count, "Attachment count missmatch");

		VkFramebufferCreateInfo framebuffer_create_info =
		{
			VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO),
			0,
			render_pass->render_pass,
			attachment_count,
			image_views,
			1280,
			720,
			1
		};
		res = vkCreateFramebuffer(device->vk_device, &framebuffer_create_info, &device->vk_allocator, &render_pass->framebuffer[f]);
		VGPU_ASSERT(device, res == VK_SUCCESS, "Failed to create framebuffer");
	}

	return render_pass;
}

void vgpu_destroy_render_pass(vgpu_device_t* device, vgpu_render_pass_t* render_pass)
{
	vkDestroyRenderPass(device->vk_device, render_pass->render_pass, &device->vk_allocator);
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
	command_list->command_buffer = VK_NULL_HANDLE;
	command_list->curr_pass = nullptr;

	return command_list;
}

void vgpu_destroy_command_list(vgpu_device_t* device, vgpu_command_list_t* command_list)
{
	VGPU_ASSERT(device, command_list->command_buffer == VK_NULL_HANDLE, "Destroying command list while building");
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
	size_t id = command_list->device->frame_no % VGPU_MULTI_BUFFERING;
	
	VGPU_ASSERT(command_list->device, command_list->command_buffer == VK_NULL_HANDLE, "Command list already begun");
	if (thread_context->free[id].empty())
	{
		VkCommandBufferAllocateInfo command_buffer_info =
		{
			VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO),
			thread_context->command_pool[id],
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1
		};
		VkResult res = vkAllocateCommandBuffers(command_list->device->vk_device, &command_buffer_info, &command_list->command_buffer);
	}
	else
	{
		command_list->command_buffer = thread_context->free[id].back();
		thread_context->free[id].remove_back();
	}

	command_list->thread_context = thread_context;

	VkCommandBufferInheritanceInfo inheritance_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO),
		render_pass ? render_pass->render_pass : VK_NULL_HANDLE,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		render_pass ? render_pass->framebuffer[render_pass->has_framebuffer ? command_list->device->swapchain_image_index : 0] : VK_NULL_HANDLE,
		VK_FALSE,
		0,
		0,
	};

	VkCommandBufferBeginInfo begin_info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO),
		0, // flags
		&inheritance_info,
	};
	VkResult res = vkBeginCommandBuffer(command_list->command_buffer, &begin_info);
	VGPU_ASSERT(command_list->device, res == VK_SUCCESS, "Failed to begin command buffer");

	command_list->curr_pass = render_pass;
	command_list->present_semaphore_needed = false;
}

void vgpu_end_command_list(vgpu_command_list_t* command_list)
{
	VkResult res = vkEndCommandBuffer(command_list->command_buffer);
	VGPU_ASSERT(command_list->device, res == VK_SUCCESS, "Failed to end command buffer");

	command_list->curr_pass = nullptr;
}

void* vgpu_lock_buffer(vgpu_command_list_t* command_list, vgpu_lock_buffer_params_t* params)
{
	// TODO: handle memory that is not host accessible
	void* data = nullptr;
	VkResult res = vkMapMemory(command_list->device->vk_device, params->buffer->mem, params->offset, params->num_bytes, 0, &data);
	VGPU_ASSERT(command_list->device, res == VK_SUCCESS, "Failed to map memory for buffer");
	return data;
}

void vgpu_unlock_buffer(vgpu_command_list_t* command_list, const vgpu_lock_buffer_params_t* params)
{
	vkUnmapMemory(command_list->device->vk_device, params->buffer->mem);
}

void vgpu_set_buffer_data(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, size_t offset, const void* data, size_t num_bytes)
{
	vgpu_lock_buffer_params_t params = {};
	params.buffer = buffer;
	params.offset = offset;
	params.num_bytes = num_bytes;
	void* dst = vgpu_lock_buffer(command_list, &params);
	memcpy(dst, data, num_bytes);
	vgpu_unlock_buffer(command_list, &params);
}

void vgpu_set_resource_table(vgpu_command_list_t* command_list, uint32_t slot, vgpu_resource_table_t* resource_table)
{
}

void vgpu_set_buffer(vgpu_command_list_t* command_list, uint32_t slot, vgpu_buffer_t* buffer, size_t offset, size_t num_bytes)
{
}

void vgpu_set_pipeline(vgpu_command_list_t* command_list, vgpu_pipeline_t* pipeline)
{
	vkCmdBindPipeline(command_list->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk_pipeline);
}

void vgpu_set_index_buffer(vgpu_command_list_t* command_list, vgpu_data_type_t index_type, vgpu_buffer_t* index_buffer)
{
	vkCmdBindIndexBuffer(command_list->command_buffer, index_buffer->buffer, 0, translate_indextype[index_type]);
}

void vgpu_draw(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_vertex, uint32_t num_vertices)
{
	//vkCmdDraw(command_list->command_buffer, num_vertices, num_instances, first_vertex, first_instance);
}

void vgpu_draw_indexed(vgpu_command_list_t* command_list, uint32_t first_instance, uint32_t num_instances, uint32_t first_index, uint32_t num_indices, uint32_t first_vertex)
{
	//vkCmdDrawIndexed(command_list->command_buffer, num_indices, num_instances, first_index, first_vertex, first_instance);
}

void vgpu_draw_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	//vkCmdDrawIndirect(command_list->command_buffer, buffer->buffer, offset, count, 128);
}

void vgpu_draw_indexed_indirect(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, uint64_t offset, uint32_t count)
{
	//vkCmdDrawIndexedIndirect(command_list->command_buffer, buffer->buffer, offset, count, 128); // TODO: stride
}

void vgpu_clear_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	for (uint32_t i = 0; i < render_pass->num_color_targets; ++i)
	{
		VkImage image;
		if (render_pass->is_framebuffer[i])
			image = command_list->device->swapchain_image[command_list->device->swapchain_image_index];
		else
			image = render_pass->color_targets[i]->image;

		VkImageSubresourceRange range =
		{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			1,
		};
		vkCmdClearColorImage(command_list->command_buffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &render_pass->clear_values[i].color, 1, &range);
	}

	if (render_pass->depth_stencil_target)
	{
		VkImageSubresourceRange range =
		{
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			0,
			1,
			0,
			1,
		};
		vkCmdClearDepthStencilImage(command_list->command_buffer, render_pass->depth_stencil_target->image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &render_pass->clear_values[render_pass->num_color_targets + 1].depthStencil, 1, &range);
	}
}

void vgpu_set_render_pass(vgpu_command_list_t* command_list, vgpu_render_pass_t* render_pass)
{
	if(command_list->curr_pass)
		vkCmdEndRenderPass(command_list->command_buffer);

	VkRenderPassBeginInfo info =
	{
		VGPU_VK_ADD_TYPE(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO),
		render_pass->render_pass,
		render_pass->framebuffer[render_pass->has_framebuffer ? command_list->device->swapchain_image_index : 0],
		{ { 0, 0 }, { 1280, 720 } },
		render_pass->num_color_targets + (render_pass->depth_stencil_target ? 1 : 0),
		render_pass->clear_values,
	};
	vkCmdBeginRenderPass(command_list->command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	command_list->curr_pass = render_pass;
}

void vgpu_transition_buffer(vgpu_command_list_t* command_list, vgpu_buffer_t* buffer, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}

void vgpu_transition_texture(vgpu_command_list_t* command_list, vgpu_texture_t* texture, vgpu_resource_state_t state_before, vgpu_resource_state_t state_after)
{
}
