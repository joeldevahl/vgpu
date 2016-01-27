#include "vgpu_gl.h"

void vgpu_platform_load_gl_ptrs(vgpu_glc_t* glc)
{
#define IF_0(t, f) f
#define IF_1(t, f) t
#define LOAD_FUNCTION(type, name) glc->gl##name = (PFNGL##type##PROC)wglGetProcAddress("gl"#name);
#define SET_FUNCTION(type, name) glc->gl##name = gl##name;
#define X(load, type, name) \
	IF_##load(LOAD_FUNCTION(type, name), SET_FUNCTION(type, name)) \
	ASSERT(glc->gl##name, "Could not load gl function gl" #name);
	GPU_GL_FUNCTIONS
#undef X
}

void* vgpu_platform_load_gl_func(const char* name)
{
	return wglGetProcAddress(name);
}

struct vgpu_platform_data_s
{
	HWND hwnd;
	HDC hdc;
	HGLRC glrc, glrc_temp;

	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
};

void vgpu_platform_create_device(vgpu_device_t* device, const vgpu_create_device_params_t* params)
{
	device->platform_data = VGPU_ALLOC_TYPE(device->allocator, vgpu_platform_data_s);
	device->platform_data->hwnd = (HWND)params->window;

	const int attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 4,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	const PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		0,
		PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW,
		PFD_TYPE_RGBA,
		32,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0,
		0,
		0,
	};

	device->platform_data->hdc = GetDC(device->platform_data->hwnd);

	int num_pixel_format = ChoosePixelFormat(device->platform_data->hdc, &pfd);
	SetPixelFormat(device->platform_data->hdc, num_pixel_format, &pfd);
	HGLRC glrc_temp = wglCreateContext(device->platform_data->hdc);
	wglMakeCurrent(device->platform_data->hdc, glrc_temp);
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	device->platform_data->glrc = wglCreateContextAttribsARB(device->platform_data->hdc, NULL, attributes);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(glrc_temp);
	wglMakeCurrent(device->platform_data->hdc, device->platform_data->glrc);

	device->platform_data->wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	device->platform_data->wglSwapIntervalEXT(0);
}

void vgpu_platform_destroy_device(vgpu_device_t* device)
{
	VGPU_FREE(device->allocator, device->platform_data);
	device->platform_data = NULL;
}

void vgpu_platform_swap(vgpu_device_t* device)
{
	SwapBuffers(device->platform_data->hdc);
}
