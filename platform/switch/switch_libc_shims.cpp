/**************************************************************************/
/*  switch_libc_shims.cpp                                                 */
/**************************************************************************/

// Functions Godot or its bundled libraries call that devkitA64's newlib does
// not provide. Keep each shim minimal and documented.

#include <cerrno>
#include <cstddef>
#include <malloc.h>

extern "C" {

// newlib has memalign() but not posix_memalign(); free() accepts either result.
int posix_memalign(void **p_memptr, size_t p_alignment, size_t p_size) {
	void *mem = memalign(p_alignment, p_size);
	if (mem == nullptr) {
		return ENOMEM;
	}
	*p_memptr = mem;
	return 0;
}

} // extern "C"
