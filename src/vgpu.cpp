#include "vgpu_internal.h"

#if defined(VGPU_WINDOWS)
#	include <malloc.h>
#endif

void* vgpu_alloc_wrapper(vgpu_allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	return allocator->alloc(allocator, count, size, align, file, line);
}

void* vgpu_realloc_wrapper(vgpu_allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	return allocator->realloc(allocator, memory, count, size, align, file, line);
}

void vgpu_free_wrapper(vgpu_allocator_t* allocator, void* memory, const char* file, int line)
{
	allocator->free(allocator, memory, file, line);
}

void* vgpu_default_alloc(vgpu_allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(VGPU_WINDOWS)
	return _aligned_malloc(count * size, align);
#elif defined(VGPU_UNIX)
	void* ptr = nullptr;
	posix_memalign(&ptr, align < sizeof(void*) ? sizeof(void*) : align, count * size);
	return ptr;
#else
#	error Not implemented for this platform.
#endif
}

void* vgpu_default_realloc(vgpu_allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(VGPU_WINDOWS)
	return _aligned_recalloc(memory, count, size, align);
#elif defined(VGPU_UNIX)
	return realloc(memory, count * size);
#else
#	error Not implemented for this platform.
#endif
}

void vgpu_default_free(vgpu_allocator_t* allocator, void* memory, const char* file, int line)
{
	(void)allocator;
	(void)file;
	(void)line;
#if defined(VGPU_WINDOWS)
	_aligned_free(memory);
#elif defined(VGPU_UNIX)
	free(memory);
#else
#	error Not implemented for this platform.
#endif
}

vgpu_allocator_t vgpu_allocator_default = {
	vgpu_default_alloc,
	vgpu_default_realloc,
	vgpu_default_free
};