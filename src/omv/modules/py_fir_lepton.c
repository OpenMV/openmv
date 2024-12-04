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
 * FIR Python module.
 */
#include "omv_boardconfig.h"
#if (OMV_FIR_LEPTON_ENABLE == 1)
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "softtimer.h"

#include "crc16.h"
#include "LEPTON_SDK.h"
#include "LEPTON_AGC.h"
#include "LEPTON_SYS.h"
#include "LEPTON_VID.h"
#include "LEPTON_OEM.h"
#include "LEPTON_RAD.h"
#include "LEPTON_I2C_Reg.h"

#include "py_helper.h"
#include "omv_common.h"
#include "omv_gpio.h"
#include "omv_spi.h"

#define FRAMEBUFFER_COUNT    3
static volatile int framebuffer_tail = 0;
static int framebuffer_head = 0;
static uint16_t *framebuffers[FRAMEBUFFER_COUNT] = {};

static int fir_lepton_rad_en = false;
static bool fir_lepton_3 = false;

#if defined(OMV_FIR_LEPTON_MCLK_TIM)
static TIM_HandleTypeDef fir_lepton_mclk_tim_handle = {};
#endif

static LEP_CAMERA_PORT_DESC_T fir_lepton_handle = {};
static omv_spi_t spi_bus = {};

#define VOSPI_HEADER_WORDS          (2) // 16-bits
#define VOSPI_PID_SIZE_PIXELS       (80) // w, 16-bits per pixel
#define VOSPI_PIDS_PER_SID          (60) // h
#define VOSPI_SIDS_PER_FRAME        (4)
#define VOSPI_PACKET_SIZE           (VOSPI_HEADER_WORDS + VOSPI_PID_SIZE_PIXELS) // 16-bits
#define VOSPI_SID_SIZE_PIXELS       (VOSPI_PIDS_PER_SID * VOSPI_PID_SIZE_PIXELS) // 16-bits

#define VOSPI_BUFFER_SIZE           (VOSPI_PACKET_SIZE * 2) // 16-bits
#define VOSPI_CLOCK_SPEED           20000000 // hz
#define VOSPI_SYNC_MS               200 // ms

#define VOSPI_SPECIAL_PACKET        (20)
#define VOSPI_DONT_CARE_PACKET      (0x0F00)
#define VOSPI_HEADER_DONT_CARE(x)   (((x) & VOSPI_DONT_CARE_PACKET) == VOSPI_DONT_CARE_PACKET)
#define VOSPI_HEADER_PID(id)        ((id) & 0x0FFF)
#define VOSPI_HEADER_SID(id)        (((id) >> 12) & 0x7)

static soft_timer_entry_t flir_lepton_spi_rx_timer = {};
static int fir_lepton_spi_rx_cb_tail = 0;
static int fir_lepton_spi_rx_cb_expected_pid = 0;
static int fir_lepton_spi_rx_cb_expected_sid = 0;
static uint16_t OMV_ATTR_SECTION(OMV_ATTR_ALIGNED_DMA(fir_lepton_buf[VOSPI_BUFFER_SIZE]), ".dma_buffer");
static void fir_lepton_spi_callback(omv_spi_t *spi, void *userdata, void *buf);

static mp_obj_t fir_lepton_spi_resync_callback(mp_obj_t unused) {
    // For triple buffering we are never drawing where tail or head
    // (which may instantly update to be equal to tail) is.
    fir_lepton_spi_rx_cb_tail = (framebuffer_tail + 1) % FRAMEBUFFER_COUNT;
    if (fir_lepton_spi_rx_cb_tail == framebuffer_head) {
        fir_lepton_spi_rx_cb_tail = (fir_lepton_spi_rx_cb_tail + 1) % FRAMEBUFFER_COUNT;
    }

    omv_spi_transfer_t spi_xfer = {
        .rxbuf = fir_lepton_buf,
        .size = VOSPI_BUFFER_SIZE,
        .flags = OMV_SPI_XFER_DMA,
        .callback = fir_lepton_spi_callback,
    };

    omv_gpio_write(OMV_FIR_LEPTON_SSEL_PIN, 0);
    omv_spi_transfer_start(&spi_bus, &spi_xfer);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(fir_lepton_spi_resync_callback_obj, fir_lepton_spi_resync_callback);

static void fir_lepton_spi_resync() {
    flir_lepton_spi_rx_timer.flags = SOFT_TIMER_FLAG_PY_CALLBACK;
    flir_lepton_spi_rx_timer.mode = SOFT_TIMER_MODE_ONE_SHOT;
    flir_lepton_spi_rx_timer.delta_ms = VOSPI_SYNC_MS;
    flir_lepton_spi_rx_timer.py_callback = (mp_obj_t) &fir_lepton_spi_resync_callback_obj;
    soft_timer_insert(&flir_lepton_spi_rx_timer, VOSPI_SYNC_MS);
}

#if defined(OMV_FIR_LEPTON_CHECK_CRC)
static bool fir_lepton_spi_check_crc(const uint16_t *base) {
    int id = base[0];
    int packet_crc = base[1];
    int crc = ByteCRC16((id >> 8) & 0x0F, 0);
    crc = ByteCRC16(id, crc);
    crc = ByteCRC16(0, crc);
    crc = ByteCRC16(0, crc);

    for (int i = VOSPI_HEADER_WORDS; i < VOSPI_PACKET_SIZE; i++) {
        int value = base[i];
        crc = ByteCRC16(value >> 8, crc);
        crc = ByteCRC16(value, crc);
    }

    return packet_crc == crc;
}
#endif

static mp_obj_t fir_lepton_frame_cb = mp_const_none;

void fir_lepton_spi_callback(omv_spi_t *spi, void *userdata, void *buf) {
    const uint16_t *base = (uint16_t *) buf;

    int id = base[0];

    // Ignore don't care packets.
    if (VOSPI_HEADER_DONT_CARE(id)) {
        return;
    }

    int pid = VOSPI_HEADER_PID(id);
    int sid = VOSPI_HEADER_SID(id) - 1;

    // Discard packets with a pid != 0 when waiting for the first packet.
    if ((fir_lepton_spi_rx_cb_expected_pid == 0) && (pid != 0)) {
        return;
    }

    // Discard sidments with a sid != 0 when waiting for the first segment.
    if (fir_lepton_3 && (pid == VOSPI_SPECIAL_PACKET) && (fir_lepton_spi_rx_cb_expected_sid == 0) && (sid != 0)) {
        fir_lepton_spi_rx_cb_expected_pid = 0;
        return;
    }

    // Are we in sync with the flir lepton?
    if ((pid != fir_lepton_spi_rx_cb_expected_pid)
        #if defined(OMV_FIR_LEPTON_CHECK_CRC)
        || (!fir_lepton_spi_check_crc(base))
        #endif
        || (fir_lepton_3 && (pid == VOSPI_SPECIAL_PACKET) && (sid != fir_lepton_spi_rx_cb_expected_sid))) {
        fir_lepton_spi_rx_cb_expected_pid = 0;
        fir_lepton_spi_rx_cb_expected_sid = 0;
        omv_spi_transfer_abort(&spi_bus);
        omv_gpio_write(OMV_FIR_LEPTON_SSEL_PIN, 1);
        fir_lepton_spi_resync();
        return;
    }

    memcpy(framebuffers[fir_lepton_spi_rx_cb_tail]
           + (fir_lepton_spi_rx_cb_expected_pid * VOSPI_PID_SIZE_PIXELS)
           + (fir_lepton_spi_rx_cb_expected_sid * VOSPI_SID_SIZE_PIXELS),
           base + VOSPI_HEADER_WORDS, VOSPI_PID_SIZE_PIXELS * sizeof(uint16_t));

    fir_lepton_spi_rx_cb_expected_pid += 1;
    if (fir_lepton_spi_rx_cb_expected_pid == VOSPI_PIDS_PER_SID) {
        fir_lepton_spi_rx_cb_expected_pid = 0;

        bool frame_ready = false;

        // For the FLIR Lepton 3 we have to receive all the pids in all the segments.
        if (fir_lepton_3) {
            fir_lepton_spi_rx_cb_expected_sid += 1;
            if (fir_lepton_spi_rx_cb_expected_sid == VOSPI_SIDS_PER_FRAME) {
                fir_lepton_spi_rx_cb_expected_sid = 0;
                frame_ready = true;
            }
            // For the FLIR Lepton 1/2 we just have to receive all the pids.
        } else {
            frame_ready = true;
        }

        if (frame_ready) {
            // Update tail which means a new image is ready.
            framebuffer_tail = fir_lepton_spi_rx_cb_tail;

            // For triple buffering we are never drawing where tail or head
            // (which may instantly update to be equal to tail) is.
            fir_lepton_spi_rx_cb_tail = (fir_lepton_spi_rx_cb_tail + 1) % FRAMEBUFFER_COUNT;
            if (fir_lepton_spi_rx_cb_tail == framebuffer_head) {
                fir_lepton_spi_rx_cb_tail = (fir_lepton_spi_rx_cb_tail + 1) % FRAMEBUFFER_COUNT;
            }

            // User should use micropython.schedule() in their callback to process the new frame.
            if (fir_lepton_frame_cb != mp_const_none) {
                mp_call_function_0(fir_lepton_frame_cb);
            }
        }
    }
}

#if defined(OMV_FIR_LEPTON_VSYNC_PIN)
static mp_obj_t fir_lepton_vsync_cb = NULL;

static void fir_lepton_extint_callback(void *data) {
    if (fir_lepton_vsync_cb) {
        mp_call_function_0(fir_lepton_vsync_cb);
    }
}
#endif

void fir_lepton_deinit() {
    omv_spi_transfer_abort(&spi_bus);
    fir_lepton_spi_rx_cb_expected_pid = 0;
    fir_lepton_spi_rx_cb_expected_sid = 0;
    fb_alloc_free_till_mark_past_mark_permanent();

    #if defined(OMV_FIR_LEPTON_MCLK)
    HAL_TIM_PWM_Stop(&fir_lepton_mclk_tim_handle, OMV_FIR_LEPTON_MCLK_TIM_CHANNEL);
    HAL_TIM_PWM_DeInit(&fir_lepton_mclk_tim_handle);
    OMV_FIR_LEPTON_MCLK_TIM_FORCE_RESET();
    OMV_FIR_LEPTON_MCLK_TIM_RELEASE_RESET();
    OMV_FIR_LEPTON_MCLK_TIM_CLK_DISABLE();
    omv_gpio_deinit(OMV_FIR_LEPTON_MCLK_PIN);
    #endif

    omv_spi_deinit(&spi_bus);

    #if defined(OMV_FIR_LEPTON_RESET_PIN)
    omv_gpio_deinit(OMV_FIR_LEPTON_RESET_PIN);
    #endif

    #if defined(OMV_FIR_LEPTON_POWER_PIN)
    omv_gpio_deinit(OMV_FIR_LEPTON_POWER_PIN);
    #endif
}

int fir_lepton_init(omv_i2c_t *bus, int *w, int *h, int *refresh, int *resolution) {
    omv_spi_config_t spi_config;
    omv_spi_default_config(&spi_config, OMV_FIR_LEPTON_SPI_BUS);

    spi_config.baudrate = VOSPI_CLOCK_SPEED;

    spi_config.datasize = 16;
    spi_config.bus_mode = OMV_SPI_BUS_RX;
    spi_config.nss_enable = false;
    spi_config.clk_pol = OMV_SPI_CPOL_HIGH;
    spi_config.clk_pha = OMV_SPI_CPHA_2EDGE;
    spi_config.dma_flags = OMV_SPI_DMA_CIRCULAR | OMV_SPI_DMA_DOUBLE;
    omv_spi_init(&spi_bus, &spi_config);

    omv_gpio_write(OMV_FIR_LEPTON_SSEL_PIN, 1);

    #if defined(OMV_FIR_LEPTON_RESET_PIN)
    omv_gpio_config(OMV_FIR_LEPTON_RESET_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_UP, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_FIR_LEPTON_RESET_PIN, 1);
    #endif

    #if defined(OMV_FIR_LEPTON_POWER_PIN)
    omv_gpio_config(OMV_FIR_LEPTON_POWER_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_UP, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_FIR_LEPTON_POWER_PIN, 1);
    #endif

    #if defined(OMV_FIR_LEPTON_MCLK_TIM)
    int tclk = OMV_FIR_LEPTON_MCLK_TIM_PCLK_FREQ() * 2;
    int period = (tclk / OMV_FIR_LEPTON_MCLK_FREQ) - 1;

    omv_gpio_config(OMV_FIR_LEPTON_MCLK_PIN, OMV_GPIO_MODE_ALT, GPIO_PULLUP, OMV_GPIO_SPEED_MED, -1);

    fir_lepton_mclk_tim_handle.Instance = OMV_FIR_LEPTON_MCLK_TIM;
    fir_lepton_mclk_tim_handle.Init.Prescaler = 0;
    fir_lepton_mclk_tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    fir_lepton_mclk_tim_handle.Init.Period = period;
    fir_lepton_mclk_tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    fir_lepton_mclk_tim_handle.Init.RepetitionCounter = 0;
    fir_lepton_mclk_tim_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_OC_InitTypeDef fir_lepton_mclk_tim_oc_handle;
    fir_lepton_mclk_tim_oc_handle.Pulse = period / 2;
    fir_lepton_mclk_tim_oc_handle.OCMode = TIM_OCMODE_PWM1;
    fir_lepton_mclk_tim_oc_handle.OCPolarity = TIM_OCPOLARITY_HIGH;
    fir_lepton_mclk_tim_oc_handle.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    fir_lepton_mclk_tim_oc_handle.OCFastMode = TIM_OCFAST_DISABLE;
    fir_lepton_mclk_tim_oc_handle.OCIdleState = TIM_OCIDLESTATE_RESET;
    fir_lepton_mclk_tim_oc_handle.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    OMV_FIR_LEPTON_MCLK_TIM_CLK_ENABLE();
    HAL_TIM_PWM_Init(&fir_lepton_mclk_tim_handle);
    HAL_TIM_PWM_ConfigChannel(&fir_lepton_mclk_tim_handle,
                              &fir_lepton_mclk_tim_oc_handle,
                              OMV_FIR_LEPTON_MCLK_TIM_CHANNEL);
    HAL_TIM_PWM_Start(&fir_lepton_mclk_tim_handle, OMV_FIR_LEPTON_MCLK_TIM_CHANNEL);
    #endif

    #if defined(OMV_FIR_LEPTON_POWER_PIN)
    omv_gpio_write(OMV_FIR_LEPTON_POWER_PIN, 0);
    mp_hal_delay_ms(10);

    omv_gpio_write(OMV_FIR_LEPTON_POWER_PIN, 1);
    mp_hal_delay_ms(10);
    #endif

    #if defined(OMV_FIR_LEPTON_RESET_PIN)
    omv_gpio_write(OMV_FIR_LEPTON_RESET_PIN, 0);
    mp_hal_delay_ms(10);

    omv_gpio_write(OMV_FIR_LEPTON_RESET_PIN, 1);
    mp_hal_delay_ms(1000);
    #endif

    LEP_RAD_ENABLE_E rad;
    LEP_AGC_ROI_T roi;

    for (uint32_t start = mp_hal_ticks_ms();; mp_hal_delay_ms(1)) {
        if (LEP_OpenPort(bus, LEP_CCI_TWI, 0, &fir_lepton_handle) == LEP_OK) {
            break;
        }

        if ((mp_hal_ticks_ms() - start) >= 1000) {
            return -1;
        }
    }

    #if (!defined(OMV_FIR_LEPTON_POWER_PIN)) && (!defined(OMV_FIR_LEPTON_RESET_PIN))
    if (LEP_RunOemReboot(&fir_lepton_handle) != LEP_OK) {
        return -2;
    }

    mp_hal_delay_ms(1000);
    #endif

    for (uint32_t start = mp_hal_ticks_ms();; mp_hal_delay_ms(1)) {
        LEP_SDK_BOOT_STATUS_E status;

        if (LEP_GetCameraBootStatus(&fir_lepton_handle, &status) != LEP_OK) {
            return -3;
        }

        if (status == LEP_BOOT_STATUS_BOOTED) {
            break;
        }

        if ((mp_hal_ticks_ms() - start) >= 1000) {
            return -4;
        }
    }

    for (uint32_t start = mp_hal_ticks_ms();; mp_hal_delay_ms(1)) {
        LEP_UINT16 status;

        if (LEP_DirectReadRegister(&fir_lepton_handle, LEP_I2C_STATUS_REG, &status) != LEP_OK) {
            return -5;
        }

        if (!(status & LEP_I2C_STATUS_BUSY_BIT_MASK)) {
            break;
        }

        if ((mp_hal_ticks_ms() - start) >= 1000) {
            return -6;
        }
    }

    if (LEP_GetAgcROI(&fir_lepton_handle, &roi) != LEP_OK) {
        return -7;
    }

    if (LEP_GetRadEnableState(&fir_lepton_handle, &rad) != LEP_OK) {
        return -8;
    }

    int flir_w = roi.endCol + 1;
    int flir_h = roi.endRow + 1;
    fir_lepton_3 = flir_h > VOSPI_PIDS_PER_SID;
    fir_lepton_rad_en = rad == LEP_RAD_ENABLE;
    *w = flir_w;
    *h = flir_h;
    *refresh = fir_lepton_3 ? 27 : 9;
    *resolution = fir_lepton_rad_en ? 16 : 14;

    #if defined(OMV_FIR_LEPTON_VSYNC_PIN)
    if (LEP_SetOemGpioMode(&fir_lepton_handle, LEP_OEM_GPIO_MODE_VSYNC) != LEP_OK) {
        return -9;
    }
    omv_gpio_config(OMV_FIR_LEPTON_VSYNC_PIN, OMV_GPIO_MODE_IT_FALL, OMV_GPIO_PULL_UP, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_irq_register(OMV_FIR_LEPTON_VSYNC_PIN, fir_lepton_extint_callback, NULL);
    #endif

    ///////////////////////////////////////////////////////////////////////

    fb_alloc_mark();

    framebuffer_tail = 0;
    framebuffer_head = 0;

    for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
        framebuffers[i] = (uint16_t *) fb_alloc0(flir_w * flir_h * sizeof(uint16_t));
    }

    fb_alloc_mark_permanent();
    fir_lepton_spi_resync();
    return 0;
}

#if defined(OMV_FIR_LEPTON_VSYNC_PIN)
void fir_lepton_register_vsync_cb(mp_obj_t cb) {
    omv_gpio_irq_enable(OMV_FIR_LEPTON_VSYNC_PIN, false);

    fir_lepton_vsync_cb = cb;

    if (cb != mp_const_none) {
        omv_gpio_irq_enable(OMV_FIR_LEPTON_VSYNC_PIN, true);
    }
}
#endif

mp_obj_t fir_lepton_get_radiometry() {
    return mp_obj_new_bool(fir_lepton_rad_en);
}

void fir_lepton_register_frame_cb(mp_obj_t cb) {
    fir_lepton_frame_cb = cb;
}

mp_obj_t fir_lepton_get_frame_available() {
    return mp_obj_new_bool(framebuffer_tail != framebuffer_head);
}

static const uint16_t *fir_lepton_get_frame(int timeout) {
    int sampled_framebuffer_tail = framebuffer_tail;

    if (timeout >= 0) {
        for (uint32_t start = mp_hal_ticks_ms();;) {
            sampled_framebuffer_tail = framebuffer_tail;

            if (framebuffer_head != sampled_framebuffer_tail) {
                break;
            }

            if ((mp_hal_ticks_ms() - start) >= timeout) {
                mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Timeout!"));
            }

            MICROPY_EVENT_POLL_HOOK
        }
    }

    framebuffer_head = sampled_framebuffer_tail;
    return framebuffers[sampled_framebuffer_tail];
}

static int fir_lepton_get_temperature() {
    LEP_SYS_FPA_TEMPERATURE_KELVIN_T kelvin;

    if (LEP_GetSysFpaTemperatureKelvin(&fir_lepton_handle, &kelvin) != LEP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("FPA Error!"));
    }

    return kelvin;
}

mp_obj_t fir_lepton_read_ta() {
    return mp_obj_new_float((fir_lepton_get_temperature() * 0.01f) - 273.15f);
}

mp_obj_t fir_lepton_read_ir(int w, int h, bool mirror, bool flip, bool transpose, int timeout) {
    int kelvin = fir_lepton_get_temperature();
    mp_obj_list_t *list = (mp_obj_list_t *) mp_obj_new_list(w * h, NULL);
    const uint16_t *data = fir_lepton_get_frame(timeout);
    float min = +FLT_MAX;
    float max = -FLT_MAX;
    int w_1 = w - 1;
    int h_1 = h - 1;

    for (int y = 0; y < h; y++) {
        int y_dst = flip ? (h_1 - y) : y;
        const uint16_t *raw_row = data + (y * w);
        mp_obj_t *list_row = list->items + (y_dst * w);
        mp_obj_t *t_list_row = list->items + y_dst;

        for (int x = 0; x < w; x++) {
            int x_dst = mirror ? (w_1 - x) : x;
            int raw = raw_row[x];

            if (!fir_lepton_rad_en) {
                raw = (raw - 8192) + kelvin;
            }

            float celcius = (raw * 0.01f) - 273.15f;

            if (celcius < min) {
                min = celcius;
            }

            if (celcius > max) {
                max = celcius;
            }

            mp_obj_t f = mp_obj_new_float(celcius);

            if (!transpose) {
                list_row[x_dst] = f;
            } else {
                t_list_row[x_dst * h] = f;
            }
        }
    }

    mp_obj_t tuple[4];
    tuple[0] = mp_obj_new_float((kelvin * 0.01f) - 273.15f);
    tuple[1] = MP_OBJ_FROM_PTR(list);
    tuple[2] = mp_obj_new_float(min);
    tuple[3] = mp_obj_new_float(max);
    return mp_obj_new_tuple(4, tuple);
}

void fir_lepton_fill_image(image_t *img, int w, int h, bool auto_range, float min, float max,
                           bool mirror, bool flip, bool transpose, int timeout) {
    int kelvin = fir_lepton_get_temperature();
    const uint16_t *data = fir_lepton_get_frame(timeout);
    int new_min;
    int new_max;

    if (auto_range) {
        new_min = INT_MAX;
        new_max = INT_MIN;

        for (int i = 0, ii = w * h; i < ii; i++) {
            int temp = data[i];

            if (!fir_lepton_rad_en) {
                temp = (temp - 8192) + kelvin;
            }

            if (temp < new_min) {
                new_min = temp;
            }

            if (temp > new_max) {
                new_max = temp;
            }
        }
    } else {
        float tmp = min;
        min = (min < max) ? min : max;
        max = (max > tmp) ? max : tmp;
        new_min = fast_roundf((min + 273.15f) * 100.f); // to kelvin
        new_max = fast_roundf((max + 273.15f) * 100.f); // to kelvin
    }

    float diff = 255.f / (new_max - new_min);
    int w_1 = w - 1;
    int h_1 = h - 1;

    for (int y = 0; y < h; y++) {
        int y_dst = flip ? (h_1 - y) : y;
        const uint16_t *raw_row = data + (y * w);
        uint8_t *row_pointer = ((uint8_t *) img->data) + (y_dst * w);
        uint8_t *t_row_pointer = ((uint8_t *) img->data) + y_dst;

        for (int x = 0; x < w; x++) {
            int x_dst = mirror ? (w_1 - x) : x;
            int raw = raw_row[x];

            if (!fir_lepton_rad_en) {
                raw = (raw - 8192) + kelvin;
            }

            if (raw < new_min) {
                raw = new_min;
            }

            if (raw > new_max) {
                raw = new_max;
            }

            int pixel = fast_roundf((raw - new_min) * diff);
            pixel = __USAT(pixel, 8);

            if (!transpose) {
                row_pointer[x_dst] = pixel;
            } else {
                t_row_pointer[x_dst * h] = pixel;
            }
        }
    }
}

void fir_lepton_trigger_ffc(int timeout) {
    if (LEP_RunSysFFCNormalization(&fir_lepton_handle) != LEP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("FFC Error!"));
    }

    if (timeout >= 0) {
        for (uint32_t start = mp_hal_ticks_ms();;) {
            LEP_SYS_STATUS_E status;

            if (LEP_GetSysFFCStatus(&fir_lepton_handle, &status) != LEP_OK) {
                mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("SYS Error!"));
            }

            if (status == LEP_SYS_STATUS_READY) {
                break;
            }

            if ((mp_hal_ticks_ms() - start) >= timeout) {
                mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Timeout!"));
            }

            mp_hal_delay_ms(1);
        }
    }
}

#endif // OMV_FIR_LEPTON_ENABLE
