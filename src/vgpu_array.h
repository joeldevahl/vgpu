#ifndef VGPU_ARRAY_H
#define VGPU_ARRAY_H

#ifdef __cplusplus

#include <stdint.h>
#include "vgpu_internal.h"

template<class T>
struct vgpu_array_t
{
	T* _ptr;
	size_t _capacity;
	size_t _length;
	vgpu_allocator_t* _alloc;

	vgpu_array_t() : _ptr(NULL), _capacity(0), _length(0), _alloc(NULL) { }

	vgpu_array_t(vgpu_allocator_t* alloc, size_t capacity = 0) : _ptr(NULL), _capacity(0), _length(0), _alloc(NULL)
	{
		create(alloc, capacity);
	}

	~vgpu_array_t()
	{
		if(_alloc)
			VGPU_FREE(_alloc, _ptr);
	}

	void create(vgpu_allocator_t* alloc, size_t capacity)
	{
		VGPU_HARD_ASSERT(_ptr == NULL, "array was already created");
		_alloc = alloc;
		_capacity = capacity;
		_length = 0;
		if(capacity)
			_ptr = (T*)VGPU_ALLOC_ARRAY(alloc, capacity, T);
	}

	void set_capacity(size_t new_capacity)
	{
		VGPU_HARD_ASSERT(_alloc != NULL, "array was not created before calling set_capacity");
		VGPU_HARD_ASSERT(new_capacity >= _capacity, "array cannot shrink in size for now");

		_ptr = VGPU_REALLOC_ARRAY(_alloc, _ptr, new_capacity, T);
		_capacity = new_capacity;
	}

	void grow(size_t amount = 0)
	{
		set_capacity(_capacity + (amount ? amount : _capacity));
	}

	void ensure_capacity(size_t new_capacity)
	{
		if (_capacity < new_capacity)
			set_capacity(new_capacity);
	}

	T& operator[](size_t i)
	{
		return _ptr[i];
	}

	const T& operator[](size_t i) const
	{
		return _ptr[i];
	}

	void set_length(size_t length)
	{
		VGPU_HARD_ASSERT(length <= _capacity, "length greater than capacity");
		_length = length;
	}

	void clear()
	{
		set_length(0);
	}

	size_t capacity() const
	{
		return _capacity;
	}

	size_t length() const
	{
		return _length;
	}

	bool empty() const
	{
		return _length == 0;
	}

	bool any() const
	{
		return _length != 0;
	}

	bool full() const
	{
		return _length == _capacity;
	}

	T* begin()
	{
		return _ptr;
	}

	const T* begin() const
	{
		return _ptr;
	}

	T* end()
	{
		return _ptr + _length;
	}

	const T* end() const
	{
		return _ptr + _length;
	}

	T& front()
	{
		return _ptr[0];
	}

	const T& front() const
	{
		return _ptr[0];
	}

	T& back()
	{
		return _ptr[_length-1];
	}

	const T& back() const
	{
		return _ptr[_length-1];
	}

	void remove_front()
	{
		_length -= 1;
		memmove(_ptr, _ptr + 1, _length * sizeof(T));
	}

	void remove_front_fast()
	{
		_length -= 1;
		memmove(_ptr, _ptr + _length, sizeof(T));
	}

	void remove_back()
	{
		_length -= 1;
	}

	void remove_at(size_t i)
	{
		_length -= 1;
		memmove(_ptr + i, _ptr + i + 1, (_length - i) * sizeof(T));
	}

	void remove_at_fast(size_t i)
	{
		_length -= 1;
		memmove(_ptr + i, _ptr + _length, sizeof(T));
	}

	void append(const T& val)
	{
		VGPU_HARD_ASSERT(_length < _capacity, "cannot add beyond capacity");
		size_t i = _length;
		memcpy(_ptr + i, &val, sizeof(T));
		_length += 1;
	}

	void insert_at(size_t i, const T& val)
	{
		VGPU_HARD_ASSERT(_length < _capacity, "cannot add beyond capacity");
		VGPU_HARD_ASSERT(i <= _length, "cannot insert outside current range");
		memmove(_ptr + i + 1, _ptr + i, (_length - i) * sizeof(T));
		memcpy(_ptr + i, &val, sizeof(T));
		_length += 1;
	}
};

#endif

#endif // VGPU_ARRAY_H
