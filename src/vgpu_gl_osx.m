#import <OpenGL/OpenGL.h>
#import <AppKit/NSOpenGL.h>
#import <Cocoa/Cocoa.h>

#include "vgpu_gl.h"

void vgpu_platform_load_gl_ptrs(glc_t* glc)
{
#define IF_0(t, f) f
#define IF_1(t, f) t
#define LOAD_FUNCTION(type, name) glc->gl##name = (PFNGL##type##PROC)gpu_platform_load_gl_func("gl"#name);
#define SET_FUNCTION(type, name) glc->gl##name = gl##name;
#define X(load, type, name) \
	IF_##load(LOAD_FUNCTION(type, name), SET_FUNCTION(type, name)) \
	ASSERT(glc->gl##name, "Could not load gl function gl" #name);
GPU_GL_FUNCTIONS
#undef X
}

void* vgpu_platform_load_gl_func(const char* name)
{
	return NULL;
}

struct vgpu_platform_data_s
{
	id pixel_format;
	id context;
};

void vgpu_platform_create_device(vgpu_device_t* device, const vgpu_create_device_params_t* params)
{
	device->platform_data = (vgpu_platform_data_s*)params->alloc_func(params->alloc_context, 1, sizeof(vgpu_platform_data_s), alignof(vgpu_platform_data_s), __FILE__, __LINE__);

    NSOpenGLPixelFormatAttribute attributes[] = {
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize, 32,
		NSOpenGLPFADepthSize, 24,
		NSOpenGLPFAStencilSize, 8,
		0,
	};

    device->platform_data->pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	ASSERT(device->platform_data->pixel_format != nil, "Could not allocate a pixel format");

    device->platform_data->context = [[NSOpenGLContext alloc] initWithFormat:device->platform_data->pixel_format shareContext:nil];
	ASSERT(device->platform_data->context != nil, "Could not create OpenGL context");

	id window = (id)params->window;
    [device->platform_data->context setView:[window contentView]];
    [device->platform_data->context makeCurrentContext];
}

void vgpu_platform_destroy_device(vgpu_device_t* device)
{
    [device->platform_data->pixel_format release];

    [NSOpenGLContext clearCurrentContext];
    [device->platform_data->context release];

	device->free_func(device->alloc_context, device->platform_data, __FILE__, __LINE__);
	device->platform_data = NULL;
}

void vgpu_platform_swap(vgpu_device_t* device)
{
    [device->platform_data->context flushBuffer];
}
