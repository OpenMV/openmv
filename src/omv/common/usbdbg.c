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
 * USB debugger.
 */
#include <string.h>
#include <stdio.h>
#include "py/nlr.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"

#include "imlib.h"
#if MICROPY_PY_CSI
#include "omv_i2c.h"
#include "omv_csi.h"
#endif
#include "framebuffer.h"
#include "usbdbg.h"
#include "omv_boardconfig.h"
#include "py_image.h"

static int xfer_offs;
static int xfer_size;
static enum usbdbg_cmd cmd;

static volatile bool script_ready;
static volatile bool script_running;
static volatile bool irq_enabled;
static vstr_t script_buf;

// These functions must be implemented by the stack.
extern uint32_t usb_cdc_buf_len();
extern uint32_t usb_cdc_get_buf(uint8_t *buf, uint32_t len);
extern void usb_cdc_reset_buffers(void);


void usbdbg_init() {
    cmd = USBDBG_NONE;
    script_ready = false;
    script_running = false;
    irq_enabled = false;

    vstr_init(&script_buf, 32);
}

bool usbdbg_script_ready() {
    return script_ready;
}

vstr_t *usbdbg_get_script() {
    return &script_buf;
}

bool usbdbg_is_busy() {
    return cmd != USBDBG_NONE;
}

void usbdbg_set_script_running(bool running) {
    script_running = running;
}

inline void usbdbg_set_irq_enabled(bool enabled) {
    if (enabled) {
        NVIC_EnableIRQ(OMV_USB_IRQN);
    } else {
        NVIC_DisableIRQ(OMV_USB_IRQN);
    }
    __DSB(); __ISB();
    irq_enabled = enabled;
}

static void usbdbg_interrupt_vm(bool ready) {
    // Set script ready flag
    script_ready = ready;

    // Set script running flag
    script_running = ready;

    // Disable IDE IRQ (re-enabled by pyexec or in main).
    usbdbg_set_irq_enabled(false);

    // Abort the VM.
    mp_sched_vm_abort();

    // When the VM runs again it will raise a KeyboardInterrupt.
    mp_sched_keyboard_interrupt();
}

bool usbdbg_get_irq_enabled() {
    return irq_enabled;
}

uint32_t ticks_diff_ms(uint32_t start_ms) {
    uint32_t current_ms = mp_hal_ticks_ms();
    if (current_ms >= start_ms) {
        return current_ms - start_ms;
    } else {
        // Handle wraparound
        return (UINT32_MAX - start_ms) + current_ms + 1;
    }
}

void usbdbg_data_in(uint32_t size, usbdbg_write_callback_t write_callback) {
    switch (cmd) {
        case USBDBG_FW_VERSION: {
            uint32_t buffer[3] = {
                FIRMWARE_VERSION_MAJOR,
                FIRMWARE_VERSION_MINOR,
                FIRMWARE_VERSION_PATCH,
            };
            cmd = USBDBG_NONE;
            write_callback(&buffer, sizeof(buffer));
            break;
        }

        case USBDBG_TX_BUF_LEN: {
            uint32_t tx_buf_len = usb_cdc_buf_len();
            cmd = USBDBG_NONE;
            write_callback(&tx_buf_len, sizeof(tx_buf_len));
            break;
        }

        case USBDBG_SENSOR_ID: {
            uint32_t buffer = 0xFF;
            #if MICROPY_PY_CSI
            if (omv_csi_is_detected() == true) {
                buffer = omv_csi_get_id();
            }
            #endif
            cmd = USBDBG_NONE;
            write_callback(&buffer, sizeof(buffer));
            break;
        }

        case USBDBG_TX_BUF: {
            uint8_t buffer[size];
            size = usb_cdc_get_buf(buffer, size);
            xfer_offs += size;
            if (xfer_offs == xfer_size) {
                cmd = USBDBG_NONE;
            }
            if (size) {
                write_callback(buffer, size);
            }
            break;
        }

        case USBDBG_FRAME_SIZE: {
            // Return 0 if FB is locked or not ready.
            uint32_t buffer[3] = { 0 };
            // Try to lock FB. If header size == 0 frame is not ready
            if (mutex_try_lock_alternate(&JPEG_FB()->lock, MUTEX_TID_IDE)) {
                // If header size == 0 frame is not ready
                if (JPEG_FB()->size == 0) {
                    // unlock FB
                    mutex_unlock(&JPEG_FB()->lock, MUTEX_TID_IDE);
                } else {
                    // Return header w, h and size/bpp
                    buffer[0] = JPEG_FB()->w;
                    buffer[1] = JPEG_FB()->h;
                    buffer[2] = JPEG_FB()->size;
                }
            }
            cmd = USBDBG_NONE;
            write_callback(&buffer, sizeof(buffer));
            break;
        }

        case USBDBG_FRAME_DUMP:
            if (xfer_offs < xfer_size) {
                write_callback(JPEG_FB()->pixels + xfer_offs, size);
                xfer_offs += size;
                if (xfer_offs == xfer_size) {
                    cmd = USBDBG_NONE;
                    JPEG_FB()->w = 0; JPEG_FB()->h = 0; JPEG_FB()->size = 0;
                    mutex_unlock(&JPEG_FB()->lock, MUTEX_TID_IDE);
                }
            }
            break;

        case USBDBG_ARCH_STR: {
            uint8_t buffer[64];
            unsigned int uid[3] = {
                #if (OMV_BOARD_UID_SIZE == 2)
                0U,
                #else
                *((unsigned int *) (OMV_BOARD_UID_ADDR + OMV_BOARD_UID_OFFSET * 2)),
                #endif
                *((unsigned int *) (OMV_BOARD_UID_ADDR + OMV_BOARD_UID_OFFSET * 1)),
                *((unsigned int *) (OMV_BOARD_UID_ADDR + OMV_BOARD_UID_OFFSET * 0)),
            };
            snprintf((char *) buffer, 64, "%s [%s:%08X%08X%08X]",
                     OMV_BOARD_ARCH, OMV_BOARD_TYPE, uid[0], uid[1], uid[2]);
            cmd = USBDBG_NONE;
            write_callback(&buffer, sizeof(buffer));
            break;
        }

        case USBDBG_SCRIPT_RUNNING: {
            cmd = USBDBG_NONE;
            uint32_t buffer = script_running;
            write_callback(&buffer, sizeof(buffer));
            break;
        }

        case USBDBG_GET_STATE: {
            // IDE will request 63/511 bytes of data to avoid ZLP packets.
            // 64/512 packets still work too but generate ZLP packets.
            uint8_t byte_buffer[size];
            memset(byte_buffer, 0, size);
            uint32_t *buffer = (uint32_t *) byte_buffer;
            static uint32_t last_update_ms = 0;

            // Set script running flag
            if (script_running) {
                buffer[0] |= USBDBG_STATE_FLAGS_SCRIPT;
            }

            // Set text buf valid flag.
            uint32_t tx_buf_len = usb_cdc_buf_len();
            if (tx_buf_len) {
                buffer[0] |= USBDBG_STATE_FLAGS_TEXT;
            }

            // Limit the frames sent over USB to 20Hz.
            if (ticks_diff_ms(last_update_ms) > 50 &&
                mutex_try_lock_alternate(&JPEG_FB()->lock, MUTEX_TID_IDE)) {
                // If header size == 0 frame is not ready
                if (JPEG_FB()->size == 0) {
                    // unlock FB
                    mutex_unlock(&JPEG_FB()->lock, MUTEX_TID_IDE);
                } else {
                    // Set valid frame flag.
                    buffer[0] |= USBDBG_STATE_FLAGS_FRAME;

                    // Set frame width, height and size/bpp
                    buffer[1] = JPEG_FB()->w;
                    buffer[2] = JPEG_FB()->h;
                    buffer[3] = JPEG_FB()->size;
                    last_update_ms = mp_hal_ticks_ms();
                }
            }

            // The rest of this packet is packed with text buffer.
            if (tx_buf_len) {
                const uint32_t hdr = 16;
                tx_buf_len = OMV_MIN(tx_buf_len, (size - hdr - 1));
                usb_cdc_get_buf(byte_buffer + hdr, tx_buf_len);
                byte_buffer[hdr + tx_buf_len] = 0; // Null-terminate
            }

            cmd = USBDBG_NONE;
            write_callback(&byte_buffer, sizeof(byte_buffer));
            break;
        }

        default: /* error */
            break;
    }
}

void usbdbg_data_out(uint32_t size, usbdbg_read_callback_t read_callback) {
    switch (cmd) {
        case USBDBG_FB_ENABLE: {
            read_callback(&(JPEG_FB()->enabled), 4);
            if (JPEG_FB()->enabled == 0) {
                // When disabling framebuffer, the IDE might still be holding FB lock.
                // If the IDE is not the current lock owner, this operation is ignored.
                mutex_unlock(&JPEG_FB()->lock, MUTEX_TID_IDE);
            }
            cmd = USBDBG_NONE;
            break;
        }

        case USBDBG_SCRIPT_EXEC:
            // check if GC is locked before allocating memory for vstr. If GC was locked
            // at least once before the script is fully uploaded xfer_offs will be less
            // than the total size (xfer_size) and the script will Not be executed.
            if (!script_running) {
                nlr_buf_t nlr;
                if (!gc_is_locked() && nlr_push(&nlr) == 0) {
                    char *buffer = vstr_add_len(&script_buf, size);
                    read_callback(buffer, size);
                    nlr_pop();
                }
                xfer_offs += size;
                if (xfer_offs == xfer_size) {
                    cmd = USBDBG_NONE;
                    // Schedule the IDE exception to interrupt the VM.
                    usbdbg_interrupt_vm(true);
                }
            }
            break;

        case USBDBG_TEMPLATE_SAVE: {
            #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
            image_t image;
            framebuffer_init_image(&image);

            size = MIN(128, size);
            char buffer[size];
            read_callback(buffer, size);
            buffer[size - 1] = 0;

            rectangle_t *roi = (rectangle_t *) buffer;
            char *path = (char *) buffer + sizeof(rectangle_t);

            imlib_save_image(&image, path, roi, 50);
            #endif  //IMLIB_ENABLE_IMAGE_FILE_IO
            break;
        }

        case USBDBG_DESCRIPTOR_SAVE: {
            #if defined(IMLIB_ENABLE_IMAGE_FILE_IO) \
            && defined(IMLIB_ENABLE_KEYPOINTS)
            image_t image;
            framebuffer_init_image(&image);

            size = MIN(128, size);
            char buffer[size];
            read_callback(buffer, size);
            buffer[size - 1] = 0;

            rectangle_t *roi = (rectangle_t *) buffer;
            char *path = (char *) buffer + sizeof(rectangle_t);

            py_image_descriptor_from_roi(&image, path, roi);
            #endif  //IMLIB_ENABLE_IMAGE_FILE_IO && IMLIB_ENABLE_KEYPOINTS
            cmd = USBDBG_NONE;
            break;
        }

        case USBDBG_ATTR_WRITE: {
            #if MICROPY_PY_CSI
            struct {
                int32_t name;
                int32_t value;
            }
            attr;
            read_callback(&attr, sizeof(attr));
            switch (attr.name) {
                case OMV_CSI_ATTR_CONTRAST:
                    omv_csi_set_contrast(attr.value);
                    break;
                case OMV_CSI_ATTR_BRIGHTNESS:
                    omv_csi_set_brightness(attr.value);
                    break;
                case OMV_CSI_ATTR_SATURATION:
                    omv_csi_set_saturation(attr.value);
                    break;
                case OMV_CSI_ATTR_GAINCEILING:
                    omv_csi_set_gainceiling(attr.value);
                    break;
                default:
                    break;
            }
            #endif
            cmd = USBDBG_NONE;
            break;
        }

        case USBDBG_SET_TIME: {
            // TODO implement
            uint8_t buffer[32];
            read_callback(&buffer, sizeof(buffer));
            cmd = USBDBG_NONE;
            break;
        }

        case USBDBG_TX_INPUT: {
            // TODO implement
            cmd = USBDBG_NONE;
            break;
        }
        default: /* error */
            break;
    }
}

void usbdbg_control(void *buffer, uint8_t request, uint32_t size) {
    cmd = (enum usbdbg_cmd) request;
    switch (cmd) {
        case USBDBG_FW_VERSION:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_FRAME_SIZE:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_FRAME_DUMP:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_ARCH_STR:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_SCRIPT_EXEC:
            xfer_offs = 0;
            xfer_size = size;
            vstr_reset(&script_buf);
            break;

        case USBDBG_SCRIPT_STOP:
            if (script_running) {
                // Reset CDC buffers.
                usb_cdc_reset_buffers();

                // Schedule the IDE exception to interrupt the VM.
                usbdbg_interrupt_vm(false);
            }
            cmd = USBDBG_NONE;
            break;

        case USBDBG_SCRIPT_SAVE:
            // TODO: save running script
            cmd = USBDBG_NONE;
            break;

        case USBDBG_SCRIPT_RUNNING:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_TEMPLATE_SAVE:
        case USBDBG_DESCRIPTOR_SAVE:
            /* save template */
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_ATTR_WRITE:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_SYS_RESET:
            #if defined(OMV_BOARD_RESET)
            OMV_BOARD_RESET();
            #else
            NVIC_SystemReset();
            #endif
            break;

        case USBDBG_SYS_RESET_TO_BL: {
            #if defined(MICROPY_BOARD_ENTER_BOOTLOADER)
            MICROPY_BOARD_ENTER_BOOTLOADER(0, 0);
            #else
            NVIC_SystemReset();
            #endif
            break;
        }

        case USBDBG_FB_ENABLE: {
            xfer_offs = 0;
            xfer_size = size;
            break;
        }

        case USBDBG_TX_BUF:
        case USBDBG_TX_BUF_LEN:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_SENSOR_ID:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_SET_TIME:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_TX_INPUT:
            xfer_offs = 0;
            xfer_size = size;
            break;

        case USBDBG_GET_STATE:
            xfer_offs = 0;
            xfer_size = size;
            break;

        default: /* error */
            cmd = USBDBG_NONE;
            break;
    }
}
