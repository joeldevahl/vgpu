#ifndef VGPU_GL_H
#define VGPU_GL_H

#include <vgpu.h>
#include "vgpu_internal.h"

// TODO: enable asserts. error callback?
#define ASSERT(X, ...)

#if defined(VGPU_WINDOWS)
#	include <windows.h>
#	include <GL/gl.h>
#	include <GL/glu.h>
#	include "wglext.h"
#elif defined(VGPU_MACOSX)
#	include <OpenGL/gl3.h>
#else
#	error not implemented for this platform
#endif

#include "glcorearb.h"
#include "glext.h"

#if defined(VGPU_WINDOWS)
typedef GLenum(APIENTRYP PFNGLGETERRORPROC)();
typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (APIENTRYP PFNGLCLEARDEPTHPROC) (GLclampd depth);
typedef void (APIENTRYP PFNGLCLEARSTENCILPROC) (GLint s);
typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLENABLEPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLPOLYGONMODEPROC) (GLenum face, GLenum mode);
typedef void (APIENTRYP PFNGLCULLFACEPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLFRONTFACEPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC) (GLenum func);
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGPROC) (GLenum en);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC) (GLenum en, GLint* ptr);

typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);

typedef void (APIENTRYP PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
#endif

#define GPU_GL_FUNCTIONS \
	/* General GL functions */ \
	X(0, GETERROR,			GetError) \
	X(0, GETSTRING,			GetString) \
	X(0, GETINTEGERV,		GetIntegerv) \
	X(1, GETSTRINGI,		GetStringi) \
	X(0, VIEWPORT,			Viewport) \
	X(0, CLEARCOLOR,			ClearColor) \
	X(0, CLEARDEPTH,			ClearDepth) \
	X(0, CLEARSTENCIL,		ClearStencil) \
	X(0, CLEAR,				Clear) \
	X(0, DISABLE,			Disable) \
	X(1, DISABLEI,			Disablei) \
	X(0, ENABLE,				Enable) \
	X(1, ENABLEI,			Enablei) \
	X(0, CULLFACE,			CullFace) \
	X(0, POLYGONMODE,		PolygonMode) \
	X(0, FRONTFACE,			FrontFace) \
	X(0, POLYGONOFFSET,		PolygonOffset) \
	X(0, DEPTHFUNC,			DepthFunc) \
	X(1, BLENDEQUATIONSEPARATEI,BlendEquationSeparatei) \
	X(1, BLENDFUNCSEPARATEI,	BlendFuncSeparatei) \
	X(1, STENCILFUNCSEPARATE,StencilFuncSeparate) \
	X(1, STENCILOPSEPARATE,	StencilOpSeparate) \
	/* Texture management */ \
	X(0, GENTEXTURES,		GenTextures) \
	X(0, DELETETEXTURES,		DeleteTextures) \
	X(0, BINDTEXTURE,		BindTexture) \
	X(0, TEXIMAGE2D,			TexImage2D) \
	X(0, TEXPARAMETERI,		TexParameteri) \
	/* Draw commands */ \
	X(0, DRAWARRAYS,			DrawArrays) \
	X(0, DRAWELEMENTS,		DrawElements) \
	X(1, DRAWARRAYSINSTANCED,DrawArraysInstanced) \
	X(1, DRAWELEMENTSINSTANCED,DrawElementsInstanced) \
	/* Vertex array object management */ \
	X(1, GENVERTEXARRAYS,	GenVertexArrays) \
	X(1, DELETEVERTEXARRAYS,	DeleteVertexArrays) \
	X(1, BINDVERTEXARRAY,	BindVertexArray) \
	/* Buffer management */ \
	X(1, DELETEBUFFERS,		DeleteBuffers) \
	X(1, CREATEBUFFERS,		CreateBuffers) \
	X(1, BINDBUFFER, 		BindBuffer) \
	X(1, BINDBUFFERRANGE,	BindBufferRange) \
	X(1, NAMEDBUFFERSTORAGE, NamedBufferStorage) \
	X(1, NAMEDBUFFERSUBDATA,NamedBufferSubData) \
	X(1, MAPNAMEDBUFFERRANGE,			MapNamedBufferRange) \
	X(1, UNMAPNAMEDBUFFER,				UnmapNamedBuffer) \
	/* Program management */ \
	X(1, CREATESHADER,		CreateShader) \
	X(1, DELETESHADER,		DeleteShader) \
	X(1, COMPILESHADER,		CompileShader) \
	X(1, ATTACHSHADER,		AttachShader) \
	X(1, SHADERSOURCE,		ShaderSource) \
	X(1, GETSHADERINFOLOG,	GetShaderInfoLog) \
	X(1, GETSHADERIV,		GetShaderiv) \
	X(1, CREATEPROGRAM, 		CreateProgram) \
	X(1, DELETEPROGRAM, 		DeleteProgram) \
	X(1, LINKPROGRAM, 		LinkProgram) \
	X(1, USEPROGRAM, 		UseProgram) \
	X(1, GETPROGRAMINFOLOG,	GetProgramInfoLog) \
	X(1, GETPROGRAMIV,		GetProgramiv) \
	X(1, GETUNIFORMLOCATION,	GetUniformLocation) \
	/* Debug */ \
	X(1, DEBUGMESSAGECALLBACK,DebugMessageCallback) \


struct vgpu_buffer_s
{
	GLuint gl_id;
	size_t num_bytes;
};

#define RESOURCE_TABLE_MAX_ENTRIES 64
struct vgpu_resource_table_s
{
	size_t num_entries;
	vgpu_resource_table_entry_t entries[RESOURCE_TABLE_MAX_ENTRIES];
};

struct vgpu_root_layout_s
{
	vgpu_root_layout_slot_t slots[VGPU_MAX_ROOT_SLOTS];
};

struct vgpu_texture_s
{
	GLuint gl_id;
	vgpu_texture_type_t type;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t num_mips;

	vgpu_clear_value_t clear_value;
};

struct vgpu_program_s
{
	GLuint gl_id;
	vgpu_program_type_t program_type;
};

struct vgpu_pipeline_s
{
	GLuint gl_id;
	GLenum prim_type;

	struct
	{
		GLenum mode;
		GLenum front_face;

		bool cull_face_enable;
		GLenum cull_face;

		bool offset_enable;
		float offset_factor;
		float offset_units;
	} polygon;

	struct
	{
		bool enabled;

		GLenum src_color_func;
		GLenum dst_color_func;
		GLenum color_equation;

		GLenum src_alpha_func;
		GLenum dst_alpha_func;
		GLenum alpha_equation;
	} blend[VGPU_MAX_RENDER_TARGETS];

	struct
	{
		bool enabled;
		GLenum func;
	} depth_test;

	struct
	{
		bool enabled;

		struct
		{
			GLenum func;
			GLenum fail_op;
			GLenum depth_fail_op;
			GLenum pass_op;
			GLint ref;
			GLuint mask;
		} front, back;
	} stencil_test;

	vgpu_root_layout_t* root_layout;
};

struct vgpu_render_pass_s
{
	GLuint gl_id;
	size_t num_color_targets;

	vgpu_clear_value_t color_clear_value[VGPU_MAX_RENDER_TARGETS];
	vgpu_clear_value_t depth_stencil_clear_value;
};

typedef struct vgpu_glc_s
{
#define X(load, type, name) PFNGL##type##PROC gl##name;
	GPU_GL_FUNCTIONS
#undef X
} vgpu_glc_t;

struct vgpu_command_list_s
{
	vgpu_device_t* device;
	vgpu_glc_t* glc;

	// Shadow state that needs to be kept
	vgpu_pipeline_t* curr_pipeline;
	vgpu_root_layout_t* curr_root_layout;
	vgpu_render_pass_t* curr_render_pass;
	GLenum curr_index_type;
	uint32_t curr_index_size;
};

struct vgpu_thread_context_s
{
};

struct vgpu_device_s
{
	vgpu_allocator_t* allocator;

	vgpu_log_func_t log_func;
	vgpu_error_func_t error_func;

	struct vgpu_platform_data_s* platform_data;
	vgpu_glc_t glc;
	vgpu_command_list_t* immediate_command_list;

	GLuint vao_gl_id;

	uint16_t width;
	uint16_t height;
	uint64_t frame_no;

	vgpu_texture_t backbuffer;

	vgpu_caps_t caps;
};

#ifdef __cplusplus
extern "C" {
#endif

void* vgpu_platform_load_gl_func(const char* name);
void vgpu_platform_load_gl_ptrs(vgpu_glc_t* glc);

void vgpu_platform_create_device(vgpu_device_t* device, const vgpu_create_device_params_t* params);
void vgpu_platform_destroy_device(vgpu_device_t* device);
void vgpu_platform_swap(vgpu_device_t* device);

#ifdef __cplusplus
}
#endif

#endif // VGPU_GL_H
