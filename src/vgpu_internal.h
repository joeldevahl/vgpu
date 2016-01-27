#ifndef VGPU_INTERNAL_H
#define VGPU_INTERNAL_H

#include <vgpu.h>

#if defined(WIN64) || defined(_WIN64)  || defined(WIN32) || defined(_WIN32)
#	define VGPU_WINDOWS
#elif defined(MACOSX) || defined(__APPLE__) || defined(__DARWIN__)
#	define VGPU_MACOSX
#	define VGPU_UNIX
#else
#	error not implemented for this platform
#endif

void* vgpu_alloc_wrapper(vgpu_allocator_t* allocator, size_t count, size_t size, size_t align, const char* file, int line);
void* vgpu_realloc_wrapper(vgpu_allocator_t* allocator, void* memory, size_t count, size_t size, size_t align, const char* file, int line);
void vgpu_free_wrapper(vgpu_allocator_t* allocator, void* memory, const char* file, int line);

#define VGPU_ALLOC(allocator, size, align) vgpu_alloc_wrapper(allocator, 1, size, align, __FILE__, __LINE__)
#define VGPU_ALLOC_TYPE(allocator, type) (type*)vgpu_alloc_wrapper(allocator, 1, sizeof(type), alignof(type), __FILE__, __LINE__)
#define VGPU_ALLOC_ARRAY(allocator, count, type) (type*)vgpu_alloc_wrapper(allocator, count, sizeof(type), alignof(type), __FILE__, __LINE__)
#define VGPU_REALLOC(allocator, memory, size, align) vgpu_realloc_wrapper(allocator, memory, 1, size, align, __FILE__, __LINE__)
#define VGPU_REALLOC_ARRAY(allocator, memory, count, type) (type*)vgpu_realloc_wrapper(allocator, memory, count, sizeof(type), alignof(type), __FILE__, __LINE__)
#define VGPU_FREE(allocator, memory) vgpu_free_wrapper(allocator, memory, __FILE__, __LINE__)

#include <new>
#define VGPU_NEW(allocator, type, ...) (new (vgpu_alloc_wrapper(allocator, 1, sizeof(type), alignof(type), __FILE__, __LINE__)) type(__VA_ARGS__))
#define VGPU_DELETE(allocator, type, ptr) do{ if(ptr){ (ptr)->~type(); vgpu_free_wrapper(allocator, ptr, __FILE__, __LINE__); } }while(0)

// TODO
#define VGPU_BREAKPOINT() __debugbreak()

#define VGPU_ASSERT(device, cond, ...) ( (void)( ( !(cond) ) && ( device->error_func( __FILE__, __LINE__, #cond, __VA_ARGS__ ) == 1 ) && ( VGPU_BREAKPOINT(), 1 ) ) )
#define VGPU_HARD_ASSERT(cond, ...) ( (void)( ( !(cond) ) && ( VGPU_BREAKPOINT(), 1 ) ) )

#define VGPU_ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))
#define VGPU_ALIGN_UP(val, align) (((val) + ((align)-1)) & ~((align)-1))

#endif // VGPU_INTERNAL_H