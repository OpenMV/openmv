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
 * SPI Display Python module.
 */
#include "omv_boardconfig.h"

#if MICROPY_PY_DISPLAY && defined(OMV_SPI_DISPLAY_CONTROLLER)

#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "mphal.h"

#include "py_image.h"
#include "omv_gpio.h"
#include "omv_spi.h"
#include "py_display.h"

#define LCD_COMMAND_DISPOFF         (0x28)
#define LCD_COMMAND_DISPON          (0x29)
#define LCD_COMMAND_RAMWR           (0x2C)
#define LCD_COMMAND_SLPOUT          (0x11)
#define LCD_COMMAND_MADCTL          (0x36)
#define LCD_COMMAND_COLMOD          (0x3A)

#if OMV_SPI_DISPLAY_TRIPLE_BUFFER
#define LCD_TRIPLE_BUFFER_DEFAULT   (true)
#else
#define LCD_TRIPLE_BUFFER_DEFAULT   (false)
#endif

static void spi_transmit(py_display_obj_t *self, uint8_t *txdata, uint16_t size) {
    omv_spi_transfer_t spi_xfer = {
        .txbuf = txdata,
        .size = size,
        .timeout = OMV_SPI_MAX_TIMEOUT,
        .flags = OMV_SPI_XFER_BLOCKING
    };

    omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);
    omv_spi_transfer_start(&self->spi_bus, &spi_xfer);
    omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
}

static void spi_transmit_16(py_display_obj_t *self, uint8_t *txdata, uint16_t size) {
    omv_spi_transfer_t spi_xfer = {
        .txbuf = txdata,
        .size = (!self->byte_swap) ? size : (size * 2),
        .timeout = OMV_SPI_MAX_TIMEOUT,
        .flags = OMV_SPI_XFER_BLOCKING,
    };

    omv_spi_transfer_start(&self->spi_bus, &spi_xfer);
}

static void spi_switch_mode(py_display_obj_t *self, int bits, bool dma) {
    omv_spi_deinit(&self->spi_bus);

    omv_spi_config_t spi_config;
    omv_spi_default_config(&spi_config, OMV_SPI_DISPLAY_CONTROLLER);

    spi_config.baudrate = self->spi_baudrate;
    spi_config.datasize = bits;
    spi_config.bus_mode = OMV_SPI_BUS_TX;
    spi_config.nss_enable = false;
    spi_config.dma_flags = dma ? OMV_SPI_DMA_NORMAL : 0;
    omv_spi_init(&self->spi_bus, &spi_config);
}

static int spi_write(py_display_obj_t *self, uint8_t cmd, uint8_t *args, size_t n_args, bool dcs) {
    omv_gpio_write(OMV_SPI_DISPLAY_RS_PIN, 0);
    spi_transmit(self, (uint8_t []) { cmd }, 1);
    omv_gpio_write(OMV_SPI_DISPLAY_RS_PIN, 1);
    if (n_args) {
        spi_transmit(self, args, n_args);
    }
    return 0;
}

static void spi_display_command(py_display_obj_t *self, uint8_t cmd, uint8_t arg) {
    spi_write(self, cmd, &arg, (arg > 0) ? 1 : 0, false);
}

static void spi_display_callback(omv_spi_t *spi, void *userdata, void *buf) {
    py_display_obj_t *self = (py_display_obj_t *) userdata;

    static uint8_t *spi_state_write_addr = NULL;
    static size_t spi_state_write_count = 0;

    // If userdata is not null then it means that we are being kicked off.
    if (buf == NULL) {
        spi_state_write_count = 0;
    }

    if (!spi_state_write_count) {
        spi_state_write_addr = (uint8_t *) self->framebuffers[self->framebuffer_tail];
        spi_state_write_count = self->width * self->height;

        if (self->byte_swap) {
            spi_state_write_count *= 2;
        }

        self->framebuffer_head = self->framebuffer_tail;
    }

    size_t spi_state_write_limit = (!self->byte_swap) ? OMV_SPI_MAX_16BIT_XFER : OMV_SPI_MAX_8BIT_XFER;
    uint8_t *addr = spi_state_write_addr;
    size_t count = IM_MIN(spi_state_write_count, spi_state_write_limit);

    spi_state_write_addr += (!self->byte_swap) ? (count * 2) : count;
    spi_state_write_count -= count;

    // When starting the interrupt chain the first transfer is not executed in interrupt context.
    // So, disable interrupts for the first transfer so that it completes first and unlocks the
    // SPI bus before allowing the interrupt it causes to trigger starting the interrupt chain.
    omv_spi_transfer_t spi_xfer = {
        .txbuf = addr,
        .size = count,
        .flags = OMV_SPI_XFER_DMA,
        .userdata = self,
        .callback = spi_display_callback,
    };

    if (buf == NULL) {
        uint32_t irq_state = disable_irq();
        omv_spi_transfer_start(&self->spi_bus, &spi_xfer);
        enable_irq(irq_state);
    } else {
        omv_spi_transfer_start(&self->spi_bus, &spi_xfer);
    }
}

static void spi_display_kick(py_display_obj_t *self) {
    if (!self->spi_tx_running) {
        spi_display_command(self, LCD_COMMAND_RAMWR, 0);
        spi_switch_mode(self, (!self->byte_swap) ? 16 : 8, true);
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);

        // Limit the transfer size to single lines as you cannot send more
        // than 64KB per SPI transaction generally.
        for (int i = 0; i < self->height; i++) {
            uint8_t *buffer = (uint8_t *) (self->framebuffers[self->framebuffer_tail] + (self->width * i));
            spi_transmit_16(self, buffer, self->width);
        }

        spi_switch_mode(self, 8, false);
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
        spi_display_command(self, LCD_COMMAND_DISPON, 0);
        spi_display_command(self, LCD_COMMAND_RAMWR, 0);
        spi_switch_mode(self, (!self->byte_swap) ? 16 : 8, true);
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);

        // Kickoff interrupt driven image update.
        self->spi_tx_running = true;
        spi_display_callback(&self->spi_bus, self, NULL);
    }
}

static void spi_display_draw_image_cb(int x_start, int x_end, int y_row, imlib_draw_row_data_t *data) {
    py_display_obj_t *lcd_self = (py_display_obj_t *) data->callback_arg;
    spi_transmit_16(lcd_self, data->dst_row_override, lcd_self->width);
}

static void spi_display_write(py_display_obj_t *self, image_t *src_img, int dst_x_start, int dst_y_start,
                              float x_scale, float y_scale, rectangle_t *roi, int rgb_channel, int alpha,
                              const uint16_t *color_palette, const uint8_t *alpha_palette, image_hint_t hint) {
    image_t dst_img;
    dst_img.w = self->width;
    dst_img.h = self->height;
    dst_img.pixfmt = PIXFORMAT_RGB565;

    point_t p0, p1;
    imlib_draw_image_get_bounds(&dst_img, src_img, dst_x_start, dst_y_start, x_scale,
                                y_scale, roi, alpha, alpha_palette, hint, &p0, &p1);
    bool black = p0.x == -1;

    if (!self->triple_buffer) {
        dst_img.data = fb_alloc0(self->width * sizeof(uint16_t));

        spi_display_command(self, LCD_COMMAND_RAMWR, 0);
        spi_switch_mode(self, (!self->byte_swap) ? 16 : 8, true);
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);

        if (black) {
            // zero the whole image
            for (int i = 0; i < self->height; i++) {
                spi_transmit_16(self, dst_img.data, self->width);
            }
        } else {
            // Zero the top rows
            for (int i = 0; i < p0.y; i++) {
                spi_transmit_16(self, dst_img.data, self->width);
            }

            // Transmits left/right parts already zeroed...
            imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start,
                             x_scale, y_scale, roi, rgb_channel, alpha, color_palette, alpha_palette,
                             hint | IMAGE_HINT_BLACK_BACKGROUND, spi_display_draw_image_cb, self, dst_img.data);

            // Zero the bottom rows
            if (p1.y < self->height) {
                memset(dst_img.data, 0, self->width * sizeof(uint16_t));
            }

            for (int i = p1.y; i < self->height; i++) {
                spi_transmit_16(self, dst_img.data, self->width);
            }
        }

        spi_switch_mode(self, 8, false);
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
        spi_display_command(self, LCD_COMMAND_DISPON, 0);
        fb_free();
    } else {
        // For triple buffering we are never drawing where tail or head
        // (which may instantly update to to be equal to tail) is.
        int new_framebuffer_tail = (self->framebuffer_tail + 1) % FRAMEBUFFER_COUNT;
        if (new_framebuffer_tail == self->framebuffer_head) {
            new_framebuffer_tail = (new_framebuffer_tail + 1) % FRAMEBUFFER_COUNT;
        }
        dst_img.data = (uint8_t *) self->framebuffers[new_framebuffer_tail];

        if (black) {
            // zero the whole image
            memset(dst_img.data, 0, self->width * self->height * sizeof(uint16_t));
        } else {
            // Zero the top rows
            if (p0.y) {
                memset(dst_img.data, 0, self->width * p0.y * sizeof(uint16_t));
            }

            if (p0.x) {
                for (int i = p0.y; i < p1.y; i++) {
                    // Zero left
                    memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, i), 0, p0.x * sizeof(uint16_t));
                }
            }

            imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start,
                             x_scale, y_scale, roi, rgb_channel, alpha, color_palette,
                             alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL, NULL);

            if (self->width - p1.x) {
                for (int i = p0.y; i < p1.y; i++) {
                    // Zero right
                    memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, i) + p1.x, 0,
                           (self->width - p1.x) * sizeof(uint16_t));
                }
            }

            // Zero the bottom rows
            if (self->height - p1.y) {
                memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, p1.y),
                       0, self->width * (self->height - p1.y) * sizeof(uint16_t));
            }
        }

        #ifdef __DCACHE_PRESENT
        // Flush data for DMA
        SCB_CleanDCache_by_Addr((uint32_t *) dst_img.data, image_size(&dst_img));
        #endif

        // Update tail which means a new image is ready.
        self->framebuffer_tail = new_framebuffer_tail;

        // Kick off an update of the display.
        spi_display_kick(self);
    }
}

static void spi_display_clear(py_display_obj_t *self, bool display_off) {
    if (display_off) {
        // turns the display off (may not be black)
        if (self->spi_tx_running) {
            omv_spi_transfer_abort(&self->spi_bus);
            self->spi_tx_running = false;
            spi_switch_mode(self, 8, false);
            omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
        }
    } else {
        spi_display_command(self, LCD_COMMAND_DISPOFF, 0);
        fb_alloc_mark();
        spi_display_write(self, NULL, 0, 0, 1.f, 1.f, NULL, 0, 0, NULL, NULL, 0);
        fb_alloc_free_till_mark();
    }
}

#ifdef OMV_SPI_DISPLAY_BL_PIN
static void spi_display_set_backlight(py_display_obj_t *self, uint32_t intensity) {
    omv_gpio_config(OMV_SPI_DISPLAY_BL_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_NONE, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_SPI_DISPLAY_BL_PIN, !!intensity);
}
#endif

static void spi_display_deinit(py_display_obj_t *self) {
    if (self->triple_buffer) {
        omv_spi_transfer_abort(&self->spi_bus);
        fb_alloc_free_till_mark_past_mark_permanent();
    }

    omv_spi_deinit(&self->spi_bus);
    omv_gpio_deinit(OMV_SPI_DISPLAY_RS_PIN);
    omv_gpio_deinit(OMV_SPI_DISPLAY_RST_PIN);
    #ifdef OMV_SPI_DISPLAY_BL_PIN
    omv_gpio_deinit(OMV_SPI_DISPLAY_BL_PIN);
    #endif
}

mp_obj_t spi_display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_width, ARG_height, ARG_refresh, ARG_bgr, ARG_byte_swap, ARG_triple_buffer,
        ARG_controller, ARG_backlight
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_width,         MP_ARG_INT,  {.u_int = 128  } },
        { MP_QSTR_height,        MP_ARG_INT,  {.u_int = 160  } },
        { MP_QSTR_refresh,       MP_ARG_INT,  {.u_int = 60   } },
        { MP_QSTR_bgr,           MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_byte_swap,     MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_triple_buffer, MP_ARG_BOOL, {.u_bool = LCD_TRIPLE_BUFFER_DEFAULT} },
        { MP_QSTR_controller,    MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_backlight,     MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if ((args[ARG_width].u_int <= 0) || (args[ARG_width].u_int > 32767)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid Width!"));
    }
    if ((args[ARG_height].u_int <= 0) || (args[ARG_height].u_int > 32767)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid Height!"));
    }
    if ((args[ARG_refresh].u_int < 30) || (args[ARG_refresh].u_int > 120)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid Refresh Rate!"));
    }

    py_display_obj_t *self = mp_obj_malloc_with_finaliser(py_display_obj_t, &py_spi_display_type);
    self->framebuffer_tail = 0;
    self->framebuffer_head = 0;
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;
    self->refresh = args[ARG_refresh].u_int;
    self->triple_buffer = args[ARG_triple_buffer].u_bool;
    self->bgr = args[ARG_bgr].u_bool;
    self->byte_swap = args[ARG_byte_swap].u_bool;
    self->controller = args[ARG_controller].u_obj;
    self->bl_controller = args[ARG_backlight].u_obj;

    omv_spi_config_t spi_config;
    omv_spi_default_config(&spi_config, OMV_SPI_DISPLAY_CONTROLLER);

    self->spi_baudrate = self->width * self->height * self->refresh * 16;
    spi_config.baudrate = self->spi_baudrate;
    spi_config.bus_mode = OMV_SPI_BUS_TX;
    spi_config.nss_enable = false;
    omv_spi_init(&self->spi_bus, &spi_config);
    omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);

    omv_gpio_config(OMV_SPI_DISPLAY_RST_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_NONE, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_SPI_DISPLAY_RST_PIN, 1);

    omv_gpio_config(OMV_SPI_DISPLAY_RS_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_NONE, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_SPI_DISPLAY_RS_PIN, 1);

    // Reset LCD
    omv_gpio_write(OMV_SPI_DISPLAY_RST_PIN, 0);
    mp_hal_delay_ms(100);
    omv_gpio_write(OMV_SPI_DISPLAY_RST_PIN, 1);
    mp_hal_delay_ms(100);

    // Init the display controller.
    if (self->controller != mp_const_none) {
        mp_obj_t dest[3];
        mp_load_method_maybe(self->controller, MP_QSTR_init, dest);
        if (dest[0] != MP_OBJ_NULL) {
            dest[2] = MP_OBJ_FROM_PTR(self);
            mp_call_method_n_kw(1, 0, dest);
        }
    } else {
        // Sleep out
        spi_display_command(self, LCD_COMMAND_SLPOUT, 0);
        mp_hal_delay_ms(120);
        // Memory data access control
        spi_display_command(self, LCD_COMMAND_MADCTL, self->bgr ? 0xC8 : 0xC0);
        // Interface pixel format
        spi_display_command(self, LCD_COMMAND_COLMOD, 0x05);
    }

    if (self->triple_buffer) {
        fb_alloc_mark();
        uint32_t fb_size = self->width * self->height * sizeof(uint16_t);
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
            self->framebuffers[i] = (uint16_t *) fb_alloc0(fb_size);
        }
        fb_alloc_mark_permanent();
    }

    return MP_OBJ_FROM_PTR(self);
}

static const py_display_p_t py_display_p = {
    .deinit = spi_display_deinit,
    .clear = spi_display_clear,
    .write = spi_display_write,
    #ifdef OMV_SPI_DISPLAY_BL_PIN
    .set_backlight = spi_display_set_backlight,
    #endif
    .bus_write = spi_write,
};

MP_DEFINE_CONST_OBJ_TYPE(
    py_spi_display_type,
    MP_QSTR_SPIDisplay,
    MP_TYPE_FLAG_NONE,
    make_new, spi_display_make_new,
    protocol, &py_display_p,
    locals_dict, &py_display_locals_dict
    );

#endif // MICROPY_PY_DISPLAY
