/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013-2024 OpenMV, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Common data structures.
 */
#include "imlib.h"
#define CHAR_BITS     (sizeof(char) * 8)
#define CHAR_MASK     (CHAR_BITS - 1)
#define CHAR_SHIFT    IM_LOG2(CHAR_MASK)

// Bitmap
void bitmap_alloc(bitmap_t *ptr, size_t size) {
    ptr->size = size;
    ptr->data = (char *) fb_alloc0(((size + CHAR_MASK) >> CHAR_SHIFT) * sizeof(char));
}

void bitmap_free(bitmap_t *ptr) {
    if (ptr->data) {
        fb_free();
    }
}

void bitmap_clear(bitmap_t *ptr) {
    memset(ptr->data, 0, ((ptr->size + CHAR_MASK) >> CHAR_SHIFT) * sizeof(char));
}

void bitmap_bit_set(bitmap_t *ptr, size_t index) {
    ptr->data[index >> CHAR_SHIFT] |= 1 << (index & CHAR_MASK);
}

bool bitmap_bit_get(bitmap_t *ptr, size_t index) {
    return (ptr->data[index >> CHAR_SHIFT] >> (index & CHAR_MASK)) & 1;
}

// LIFO
void lifo_alloc(lifo_t *ptr, size_t size, size_t data_len) {
    ptr->len = 0;
    ptr->size = size;
    ptr->data_len = data_len;
    ptr->data = (char *) fb_alloc(size * data_len);
}

void lifo_alloc_all(lifo_t *ptr, size_t *size, size_t data_len) {
    uint32_t tmp_size;
    ptr->data = (char *) fb_alloc_all(&tmp_size, false);
    ptr->data_len = data_len;
    ptr->size = tmp_size / data_len;
    ptr->len = 0;
    *size = ptr->size;
}

void lifo_free(lifo_t *ptr) {
    if (ptr->data) {
        fb_free();
    }
}

void lifo_clear(lifo_t *ptr) {
    ptr->len = 0;
}

size_t lifo_size(lifo_t *ptr) {
    return ptr->len;
}

bool lifo_is_not_empty(lifo_t *ptr) {
    return ptr->len;
}

bool lifo_is_not_full(lifo_t *ptr) {
    return ptr->len != ptr->size;
}

void lifo_enqueue(lifo_t *ptr, void *data) {
    memcpy(ptr->data + (ptr->len * ptr->data_len), data, ptr->data_len);

    ptr->len += 1;
}

void lifo_dequeue(lifo_t *ptr, void *data) {
    if (data) {
        memcpy(data, ptr->data + ((ptr->len - 1) * ptr->data_len), ptr->data_len);
    }

    ptr->len -= 1;
}

void lifo_poke(lifo_t *ptr, void *data) {
    memcpy(ptr->data + (ptr->len * ptr->data_len), data, ptr->data_len);
}

void lifo_peek(lifo_t *ptr, void *data) {
    memcpy(data, ptr->data + ((ptr->len - 1) * ptr->data_len), ptr->data_len);
}

// FIFO
void fifo_alloc(fifo_t *ptr, size_t size, size_t data_len) {
    ptr->head = 0;
    ptr->tail = 0;
    ptr->len = 0;
    ptr->size = size;
    ptr->data_len = data_len;
    ptr->data = (char *) fb_alloc(size * data_len);
}

void fifo_alloc_all(fifo_t *ptr, size_t *size, size_t data_len) {
    uint32_t tmp_size;
    ptr->data = (char *) fb_alloc_all(&tmp_size, false);
    ptr->data_len = data_len;
    ptr->size = tmp_size / data_len;
    ptr->len = 0;
    ptr->tail = 0;
    ptr->head = 0;
    *size = ptr->size;
}

void fifo_free(fifo_t *ptr) {
    if (ptr->data) {
        fb_free();
    }
}

void fifo_clear(fifo_t *ptr) {
    ptr->head = 0;
    ptr->tail = 0;
    ptr->len = 0;
}

size_t fifo_size(fifo_t *ptr) {
    return ptr->len;
}

bool fifo_is_not_empty(fifo_t *ptr) {
    return ptr->len;
}

bool fifo_is_not_full(fifo_t *ptr) {
    return ptr->len != ptr->size;
}

void fifo_enqueue(fifo_t *ptr, void *data) {
    memcpy(ptr->data + (ptr->head * ptr->data_len), data, ptr->data_len);

    size_t temp = ptr->head + 1;

    if (temp == ptr->size) {
        temp = 0;
    }

    ptr->head = temp;
    ptr->len += 1;
}

void fifo_dequeue(fifo_t *ptr, void *data) {
    if (data) {
        memcpy(data, ptr->data + (ptr->tail * ptr->data_len), ptr->data_len);
    }

    size_t temp = ptr->tail + 1;

    if (temp == ptr->size) {
        temp = 0;
    }

    ptr->tail = temp;
    ptr->len -= 1;
}

void fifo_poke(fifo_t *ptr, void *data) {
    memcpy(ptr->data + (ptr->head * ptr->data_len), data, ptr->data_len);
}

void fifo_peek(fifo_t *ptr, void *data) {
    memcpy(data, ptr->data + (ptr->tail * ptr->data_len), ptr->data_len);
}

// Linked List
void list_init(list_t *ptr, size_t data_len) {
    ptr->head = NULL;
    ptr->tail = NULL;
    ptr->size = 0;
    ptr->data_len = data_len;
}

void list_copy(list_t *dst, list_t *src) {
    memcpy(dst, src, sizeof(list_t));
}

void list_free(list_t *ptr) {
    for (list_lnk_t *i = ptr->head; i; ) {
        list_lnk_t *j = i->next;
        xfree(i);
        i = j;
    }
}

void list_clear(list_t *ptr) {
    list_free(ptr);

    ptr->head = NULL;
    ptr->tail = NULL;
    ptr->size = 0;
}

size_t list_size(list_t *ptr) {
    return ptr->size;
}

static void list_link(list_t *dst, list_lnk_t *insert_before, list_lnk_t *lnk) {
    if (!dst->size) {
        lnk->next = NULL;
        lnk->prev = NULL;
        dst->head = lnk;
        dst->tail = lnk;
    } else if (dst->head == insert_before) {
        lnk->next = insert_before;
        lnk->prev = NULL;
        insert_before->prev = lnk;
        dst->head = lnk;
    } else if (!insert_before) {
        lnk->next = NULL;
        lnk->prev = dst->tail;
        dst->tail->next = lnk;
        dst->tail = lnk;
    } else {
        lnk->next = insert_before;
        lnk->prev = insert_before->prev;
        insert_before->prev->next = lnk;
        insert_before->prev = lnk;
    }

    dst->size += 1;
}

static void list_unlink(list_t *src, list_lnk_t *lnk) {
    if (src->head == lnk) {
        if (lnk->next) {
            lnk->next->prev = NULL;
        }
        src->head = lnk->next;
    } else if (src->tail == lnk) {
        if (lnk->prev) {
            lnk->prev->next = NULL;
        }
        src->tail = lnk->prev;
    } else {
        lnk->prev->next = lnk->next;
        lnk->next->prev = lnk->prev;
    }

    src->size -= 1;
}

void list_insert(list_t *ptr, list_lnk_t *lnk, void *data) {
    list_lnk_t *tmp = (list_lnk_t *) xalloc(sizeof(list_lnk_t) + ptr->data_len);
    memcpy(tmp->data, data, ptr->data_len);
    list_link(ptr, lnk, tmp);
}

void list_push_front(list_t *ptr, void *data) {
    list_insert(ptr, ptr->head, data);
}

void list_push_back(list_t *ptr, void *data) {
    list_insert(ptr, NULL, data);
}

void list_remove(list_t *ptr, list_lnk_t *lnk, void *data) {
    if (data) {
        memcpy(data, lnk->data, ptr->data_len);
    }

    list_unlink(ptr, lnk);
    xfree(lnk);
}

void list_pop_front(list_t *ptr, void *data) {
    list_remove(ptr, ptr->head, data);
}

void list_pop_back(list_t *ptr, void *data) {
    list_remove(ptr, ptr->tail, data);
}

void list_move(list_t *dst, list_t *src, list_lnk_t *before, list_lnk_t *lnk) {
    list_unlink(src, lnk);
    list_link(dst, before, lnk);
}

void list_move_front(list_t *dst, list_t *src, list_lnk_t *lnk) {
    list_move(dst, src, dst->head, lnk);
}

void list_move_back(list_t *dst, list_t *src, list_lnk_t *lnk) {
    list_move(dst, src, NULL, lnk);
}
