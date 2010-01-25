/*
 * Copyright (c) 2010 Martin Decky
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

/** @addtogroup abs32lemm
 * @{
 */
/** @file
 */

#ifndef KERN_abs32le_AS_H_
#define KERN_abs32le_AS_H_

#define KERNEL_ADDRESS_SPACE_SHADOWED_ARCH  0

#define KERNEL_ADDRESS_SPACE_START_ARCH  ((unsigned long) 0x80000000)
#define KERNEL_ADDRESS_SPACE_END_ARCH    ((unsigned long) 0xffffffff)
#define USER_ADDRESS_SPACE_START_ARCH    ((unsigned long) 0x00000000)
#define USER_ADDRESS_SPACE_END_ARCH      ((unsigned long) 0x7fffffff)

#define USTACK_ADDRESS_ARCH  (USER_ADDRESS_SPACE_END_ARCH - (PAGE_SIZE - 1))

typedef struct {
} as_arch_t;

#include <genarch/mm/as_pt.h>

#define as_constructor_arch(as, flags)  (as != as)
#define as_destructor_arch(as)          (as != as)
#define as_create_arch(as, flags)       (as != as)
#define as_install_arch(as)
#define as_deinstall_arch(as)
#define as_invalidate_translation_cache(as, page, cnt)

extern void as_arch_init(void);

#endif

/** @}
 */
