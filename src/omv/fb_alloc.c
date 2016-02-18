/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013-2016 Kwabena W. Agyeman <kwagyeman@openmv.io>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * Interface for using extra frame buffer RAM as a stack.
 *
 */
#include <mp.h>
#include "fb_alloc.h"
#include "framebuffer.h"

extern char _fs_cache;
static char *pointer = &_fs_cache;

NORETURN static void fb_alloc_fail()
{
    nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "FB Alloc Collision!!!"));
}

void fb_alloc_init0()
{
    pointer = &_fs_cache;
}

// returns null pointer without error if size==0
void *fb_alloc(uint32_t size)
{
    if (!size) {
        return NULL;
    }

    size=((size+sizeof(uint32_t)-1)/sizeof(uint32_t))*sizeof(uint32_t);// Round Up
    char *result = pointer - size;
    char *new_pointer = result - sizeof(uint32_t);

    // Check if allocation overwrites the framebuffer pixels
    if (new_pointer < (char *) FB_PIXELS()) {
        fb_alloc_fail();
    }

    *((uint32_t *) new_pointer) = size + sizeof(uint32_t); // Save size.
    pointer = new_pointer;
    return result;
}

// returns null pointer without error if size==0
void *fb_alloc0(uint32_t size)
{
    void *mem = fb_alloc(size);
    memset(mem, 0, size);
    return mem;
}

void fb_free()
{
    if (pointer < &_fs_cache) {
        pointer += *((uint32_t *) pointer); // Get size and pop.
    }
}
