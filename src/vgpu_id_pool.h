#ifndef VGPU_ID_POOL_H
#define VGPU_ID_POOL_H

#include <stdint.h>
#include "vgpu_internal.h"

#ifdef __cplusplus

template<class TH = uint16_t>
struct vgpu_id_pool_t
{
	vgpu_allocator_t* _alloc;
	size_t            _capacity;
	size_t            _num_free;
	TH*               _handles;

	vgpu_id_pool_t() : _alloc(NULL), _capacity(0), _num_free(0), _handles(NULL) {}

	vgpu_id_pool_t(vgpu_allocator_t* alloc, size_t capacity) : _alloc(NULL), _capacity(0), _num_free(0), _handles(NULL)
	{
		create(alloc, capacity);
	}

	~vgpu_id_pool_t()
	{
		VGPU_ALLOCATOR_FREE(_alloc, _handles);
	}

	void create(vgpu_allocator_t* alloc, size_t capacity)
	{
		_alloc= alloc;
		_capacity = capacity;
		_num_free = capacity;
		_handles = (TH*)VGPU_ALLOC_ARRAY(alloc, capacity, TH);

		for(size_t i = 0; i < capacity; ++i)
			_handles[i] = static_cast<TH>(capacity - i - 1);
	}

	size_t capacity() const
	{
		return _capacity;
	}

	size_t num_free() const
	{
		return _num_free;
	}

	size_t num_used() const
	{
		return _capacity - _num_free;
	}

	TH alloc_handle()
	{
		return _handles[--_num_free];
	}

	void free_handle(TH handle)
	{
		_handles[_num_free++] = handle;
	}
};

#endif

#endif //#ifndef VGPU_ID_POOL_H
