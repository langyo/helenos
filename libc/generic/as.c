/*
 * Copyright (C) 2006 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

#include <as.h>
#include <libc.h>
#include <unistd.h>
#include <kernel/mm/as_arg.h>
#include <task.h>

/** Create address space area.
 *
 * @param address Virtual address where to place new address space area.
 * @param size Size of the area.
 * @param flags Flags describing type of the area.
 *
 * @return address on success, (void *) -1 otherwise.
 */
void *as_area_create(void *address, size_t size, int flags)
{
	return (void *) __SYSCALL3(SYS_AS_AREA_CREATE, (sysarg_t ) address, (sysarg_t) size, (sysarg_t) flags);
}

/** Resize address space area.
 *
 * @param address Virtual address pointing into already existing address space area.
 * @param size New requested size of the area.
 * @param flags Currently unused.
 *
 * @return Zero on success or a code from @ref errno.h on failure.
 */
int as_area_resize(void *address, size_t size, int flags)
{
	return __SYSCALL3(SYS_AS_AREA_RESIZE, (sysarg_t ) address, (sysarg_t) size, (sysarg_t) flags);
}

/** Destroy address space area.
 *
 * @param address Virtual address pointing into the address space area being destroyed.
 *
 * @return Zero on success or a code from @ref errno.h on failure.
 */
int as_area_destroy(void *address)
{
	return __SYSCALL1(SYS_AS_AREA_DESTROY, (sysarg_t ) address);
}

static size_t heapsize = 0;
/* Start of heap linker symbol */
extern char _heap;

/** Sbrk emulation 
 *
 * @param size New area that should be allocated or negative, 
               if it should be shrinked
 * @return Pointer to newly allocated area
 */
void *sbrk(ssize_t incr)
{
	int rc;
	void *res;
	/* Check for invalid values */
	if (incr < 0 && -incr > heapsize)
		return NULL;
	/* Check for too large value */
	if (incr > 0 && incr+heapsize < heapsize)
		return NULL;
	/* Check for too small values */
	if (incr < 0 && incr+heapsize > heapsize)
		return NULL;

	rc = as_area_resize(&_heap, heapsize + incr,0);
	if (rc != 0)
		return NULL;
	
	/* Compute start of new area */
	res = (void *)&_heap + heapsize;

	heapsize += incr;

	return res;
}
