// SPDX-License-Identifier: MIT
//
// Copyright (C) 2013-2024 OpenMV, LLC.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// NOTE this linker script is pre-processed using the CPP first before passing it
// to LD. It has access to the board config file, can define and use CPP macros,
// and it uses a lot of magic 🪄.

/* Adds a section to a table */
#define OMV_ADD_SECTION(s) \
  LONG (ADDR(s)) \
  LONG (SIZEOF(s))

/* Aligns a section to the next power of 2. */
#define OMV_ALIGN_DMA_SECTION(section) \
    ALIGN(., 1 << LOG2CEIL(SIZEOF(section)))

/* Define a new GC block. */
#define OMV_GC_BLOCK_NEW(n) \
  .gc.block##n (NOLOAD) : { \
    . = ALIGN(4); \
    . += OMV_GC_BLOCK##n##_SIZE; \
    . = ALIGN(4); \
  } >OMV_GC_BLOCK##n##_MEMORY \

/* Define a new DMA buffer region. */
#define OMV_DMA_MEMORY_NEW(X) \
    .dma.memory##X (NOLOAD) : ALIGN(32) \
    { \
        *(.d##X##_dma_buffer) \
        . = OMV_ALIGN_DMA_SECTION(.dma.memory##X); \
    } >OMV_DMA_MEMORY_D##X

/* Main GC heap for MicroPython */
#if defined(OMV_GC_BLOCK0_MEMORY)
.gc.block0 (NOLOAD) : ALIGN(4)
{
  _heap_start = .;
  . = . + OMV_GC_BLOCK0_SIZE;
  . = ALIGN(4);
  _heap_end  = .;
} >OMV_GC_BLOCK0_MEMORY
#endif

/* Additional GC heap blocks */
#if defined(OMV_GC_BLOCK1_MEMORY)
OMV_GC_BLOCK_NEW(1)
#endif
#if defined(OMV_GC_BLOCK2_MEMORY)
OMV_GC_BLOCK_NEW(2)
#endif
#if defined(OMV_GC_BLOCK3_MEMORY)
OMV_GC_BLOCK_NEW(3)
#endif
#if defined(OMV_GC_BLOCK4_MEMORY)
OMV_GC_BLOCK_NEW(4)
#endif

/* Additional GC heap blocks table */
#if defined(OMV_GC_BLOCK1_MEMORY)
.gc.blocks.table (READONLY) :
{
  . = ALIGN(4);
  _gc_blocks_table_start = .;
  OMV_ADD_SECTION(.gc.block1)
  #if defined(OMV_GC_BLOCK2_MEMORY)
  OMV_ADD_SECTION(.gc.block2)
  #endif
  #if defined(OMV_GC_BLOCK3_MEMORY)
  OMV_ADD_SECTION(.gc.block3)
  #endif
  #if defined(OMV_GC_BLOCK4_MEMORY)
  OMV_ADD_SECTION(.gc.block4)
  #endif
  _gc_blocks_table_end = .;
  . = ALIGN(4);
} > FLASH_TEXT
#endif

/* Heap memfor libc malloc/sbrk */
#if defined(OMV_HEAP_MEMORY)
.heap (NOLOAD) : ALIGN(4)
{
  __end__ = .;
  PROVIDE(end = .);
  . = . + OMV_HEAP_SIZE;
  . = ALIGN(4);
  _heap_limit = .;
} > OMV_HEAP_MEMORY
#endif

/* Stack memory */
#if defined(OMV_STACK_MEMORY)
.stack (NOLOAD) : ALIGN(8)
{
  _sstack  = .;
  __StackLimit = .;
  . = . + OMV_STACK_SIZE;
  . = ALIGN(8);
  _estack  = .;
  __StackTop = .;
} > OMV_STACK_MEMORY
PROVIDE(__stack = __StackTop);
#endif

/* JPEG framebuffer memory */
#if defined(OMV_JPEG_MEMORY)
.jpeg_memory (NOLOAD) :
{
  . = ALIGN(4);
  _jpeg_memory_start = .;
  . = . + OMV_JPEG_SIZE;
  . = ALIGN(4);
  _jpeg_memory_end = .;
} >OMV_JPEG_MEMORY
#endif

/* VOSPI framebuffer memory */
#if defined(OMV_VOSPI_MEMORY)
.vospi_memory (NOLOAD) :
{
  . = ALIGN(4);
  _vospi_buf = .;
  . = . + OMV_VOSPI_SIZE;
  . = ALIGN(4);
} >OMV_VOSPI_MEMORY
#endif

/* GPU memory */
#if defined(OMV_GPU_MEMORY)
.gpu_memory (NOLOAD) :
{
  . = ALIGN(16);
  _gpu_memory_start = .;
  . = . + OMV_GPU_SIZE;
  . = ALIGN(16);
  _gpu_memory_end = .;
} >OMV_GPU_MEMORY
#endif

/* Open-AMP Shared Memory Region */
#if defined(OMV_OPENAMP_MEMORY)
.openamp_memory (NOLOAD) : ALIGN(32)
{
  _openamp_shm_region_start = .;
  . = . + OMV_OPENAMP_SIZE;
  _openamp_shm_region_end = .;
  . = OMV_ALIGN_DMA_SECTION(.openamp_memory);
} >OMV_OPENAMP_MEMORY
#endif

/* Memory for the second core */
#if defined(OMV_CORE1_MEMORY)
.core1_memory (NOLOAD) : ALIGN(32)
{
  _core1_memory_start = .;
  . = . + OMV_CORE1_SIZE;
  _core1_memory_end = .;
  . = OMV_ALIGN_DMA_SECTION(.core1_memory);
} >OMV_CORE1_MEMORY
#endif

/* Main framebuffer memory */
#if defined(OMV_FB_MEMORY)
.fb_memory (NOLOAD) :
{
  . = ALIGN(4);
  _fb_memory_start = .;
  . += OMV_FB_SIZE;
  . = ALIGN(4);
  _fb_memory_end = .;
  . += OMV_FB_ALLOC_SIZE;
  . = ALIGN(4);
  _fb_alloc_end = .;
  . = ALIGN(4);
} >OMV_FB_MEMORY
#endif

/* Main framebuffer fast memory */
#if defined(OMV_FB_OVERLAY_MEMORY)
.fb_overlay_memory (NOLOAD) :
{
  . = ALIGN(4);
  _fballoc_overlay_start = .;
  . = . + OMV_FB_OVERLAY_SIZE;
  _fballoc_overlay_end = .;
} >OMV_FB_OVERLAY_MEMORY
#endif

/* Misc DMA buffers section */
.dma.memory0 (NOLOAD) : ALIGN(32)
{
  /* Image line buffer. */
  #if defined(OMV_LINE_BUF_SIZE)
  . = ALIGN(16);
  _line_buf = .;
  . = . + OMV_LINE_BUF_SIZE;
  #endif

  /* USB MSC bot data (2K) */
  #if defined(OMV_MSC_BUF_SIZE)
  . = ALIGN(16);
  _msc_buf  = .;
  . = . + OMV_MSC_BUF_SIZE;
  #endif

  #if defined(OMV_JPEG_SIZE) && !defined(OMV_JPEG_MEMORY)
  . = ALIGN(16);
  _jpeg_memory_start = .;
  . = . + OMV_JPEG_SIZE;
  _jpeg_memory_end = .;
  #endif

  /* FFS (internal) filesystem buffer */
  #if defined(OMV_FFS_BUF_SIZE)
  . = ALIGN(16);
  _micropy_hw_internal_flash_storage_ram_cache_start = .;
  . = . + OMV_FFS_BUF_SIZE;
  _micropy_hw_internal_flash_storage_ram_cache_end = .;
  #endif

 . = ALIGN(16);
  *(.dma_buffer)

 . = ALIGN(16);
 *(NonCacheable)

  . = OMV_ALIGN_DMA_SECTION(.dma.memory0);
#if defined(OMV_DMA_MEMORY)
} >OMV_DMA_MEMORY
#else
} >OMV_MAIN_MEMORY
#endif

/* Additional DMA memory buffers */
#if defined(OMV_DMA_MEMORY_D1)
OMV_DMA_MEMORY_NEW(1)
#endif
#if defined(OMV_DMA_MEMORY_D2)
OMV_DMA_MEMORY_NEW(2)
#endif
#if defined(OMV_DMA_MEMORY_D3)
OMV_DMA_MEMORY_NEW(3)
#endif

/* Additional DMA memory tables */
.dma.memory.table (READONLY) :
{
  . = ALIGN(4);
  _dma_memory_table_start = .;
  OMV_ADD_SECTION(.dma.memory0)
  #if defined(OMV_DMA_MEMORY_D1)
  OMV_ADD_SECTION(.dma.memory1)
  #endif
  #if defined(OMV_DMA_MEMORY_D2)
  OMV_ADD_SECTION(.dma.memory2)
  #endif
  #if defined(OMV_DMA_MEMORY_D3)
  OMV_ADD_SECTION(.dma.memory3)
  #endif
  _dma_memory_table_end = .;
  . = ALIGN(4);
} > FLASH_TEXT


// Only one partition is currently supported.
#if defined(OMV_ROMFS_PART0_ORIGIN)
_micropy_hw_romfs_start = OMV_ROMFS_PART0_ORIGIN;
_micropy_hw_romfs_end =   OMV_ROMFS_PART0_ORIGIN + OMV_ROMFS_PART0_LENGTH;
#endif
.ARM.attributes 0 : { *(.ARM.attributes) }
