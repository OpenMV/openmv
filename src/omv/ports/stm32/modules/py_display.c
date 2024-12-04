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
 * LTDC/DSI display Python module.
 */
#include "omv_boardconfig.h"

#if MICROPY_PY_DISPLAY &&                   \
    (defined(OMV_RGB_DISPLAY_CONTROLLER) || \
    defined(OMV_DSI_DISPLAY_CONTROLLER))

#include "py/obj.h"
#include "py/runtime.h"
#include "mphal.h"

#include "py_helper.h"
#include "py_image.h"
#include "omv_gpio.h"
#include "py_display.h"

#if defined(OMV_DSI_DISPLAY_BL_PIN)
#define OMV_DISPLAY_BL_PIN OMV_DSI_DISPLAY_BL_PIN
#elif defined(OMV_RGB_DISPLAY_BL_PIN)
#define OMV_DISPLAY_BL_PIN OMV_RGB_DISPLAY_BL_PIN
#endif

#if defined(OMV_DSI_DISPLAY_DISP_PIN)
#define OMV_DISPLAY_DISP_PIN OMV_DSI_DISPLAY_DISP_PIN
#elif defined(OMV_RGB_DISPLAY_DISP_PIN)
#define OMV_DISPLAY_DISP_PIN OMV_RGB_DISPLAY_DISP_PIN
#endif

typedef struct display_mode {
    uint32_t hactive;
    uint32_t vactive;
    uint32_t pixel_clock;
    uint32_t hsync_len;
    uint32_t hback_porch;
    uint32_t hfront_porch;
    uint32_t vsync_len;
    uint32_t vback_porch;
    uint32_t vfront_porch;
    uint32_t hpol : 1;
    uint32_t vpol : 1;
} display_mode_t;

typedef struct _display_state {
    py_display_obj_t *self;
    LTDC_HandleTypeDef hltdc;
    #ifdef OMV_DSI_DISPLAY_CONTROLLER
    DSI_HandleTypeDef hdsi;
    #endif
    LTDC_LayerCfgTypeDef framebuffer_layers[FRAMEBUFFER_COUNT];
} display_state_t;

static display_state_t display;

static const display_mode_t display_modes[] = {
    { // QVGA
        .hactive = 320, .vactive = 240, .pixel_clock = 6144,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // TQVGA
        .hactive = 240, .vactive = 320, .pixel_clock = 6426,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // FHVGA
        .hactive = 480, .vactive = 272, .pixel_clock = 9633,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // FHVGA2
        .hactive = 480, .vactive = 128, .pixel_clock = 4799,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // VGA
        .hactive = 640, .vactive = 480, .pixel_clock = 21363,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // THVGA
        .hactive = 320, .vactive = 480, .pixel_clock = 11868,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // FWVGA
        .hactive = 800, .vactive = 480, .pixel_clock = 26110,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // FWVGA2
        .hactive = 800, .vactive = 320, .pixel_clock = 17670,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // TFWVGA
        .hactive = 480, .vactive = 800, .pixel_clock = 27624,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 9
    },
    { // TFWVGA2
        .hactive = 480, .vactive = 480, .pixel_clock = 16615,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // SVGA
        .hactive = 800, .vactive = 600, .pixel_clock = 32597,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 4
    },
    { // WSVGA
        .hactive = 1024, .vactive = 600, .pixel_clock = 40895,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 4
    },
    { // XGA
        .hactive = 1024, .vactive = 768, .pixel_clock = 52277,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 8
    },
    { // SXGA
        .hactive = 1280, .vactive = 1024, .pixel_clock = 85920,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 16
    },
    { // SXGA2
        .hactive = 1280, .vactive = 400, .pixel_clock = 33830,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 1
    },
    { // UXGA
        .hactive = 1600, .vactive = 1200, .pixel_clock = 124364,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 21
    },
    { // HD
        .hactive = 1280, .vactive = 720, .pixel_clock = 60405,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 7
    },
    { // FHD
        .hactive = 1920, .vactive = 1080, .pixel_clock = 133187,
        .hsync_len = 32, .hback_porch = 40, .hfront_porch = 8,
        .vsync_len = 8, .vback_porch = 6, .vfront_porch = 17
    },
};

void LTDC_IRQHandler() {
    HAL_LTDC_IRQHandler(&display.hltdc);
}

#ifdef OMV_DSI_DISPLAY_CONTROLLER
void DSI_IRQHandler(void) {
    HAL_DSI_IRQHandler(&display.hdsi);
}
#endif

static void pll_config(int framesize, int refresh) {
    uint32_t pixel_clock = (display_modes[framesize].pixel_clock * refresh) / 60;

    for (uint32_t divm = 1; divm <= 63; divm++) {
        for (uint32_t divr = 1; divr <= 128; divr++) {
            uint32_t vci = 0;
            uint32_t ref_clk = (HSE_VALUE / 1000) / divm;

            if (1000 <= ref_clk && ref_clk <= 2000) {
                vci = RCC_PLL3VCIRANGE_0;
            } else if (2000 <= ref_clk && ref_clk <= 4000) {
                vci = RCC_PLL3VCIRANGE_1;
            } else if (4000 <= ref_clk && ref_clk <= 8000) {
                vci = RCC_PLL3VCIRANGE_2;
            } else if (8000 <= ref_clk && ref_clk <= 16000) {
                vci = RCC_PLL3VCIRANGE_3;
            } else {
                continue;
            }

            uint32_t vco = 0;
            uint32_t pll_clk = pixel_clock * divr;

            if (150000 <= pll_clk && pll_clk <= 420000) {
                vco = RCC_PLL3VCOMEDIUM;
            } else if (192000 <= pll_clk && pll_clk <= 836000) {
                vco = RCC_PLL3VCOWIDE;
            } else {
                continue;
            }

            uint32_t divn = pll_clk / ref_clk;
            if (divn < 4 || 512 < divn) {
                continue;
            }

            uint32_t frac = ((pll_clk % ref_clk) * 8192) / ref_clk;

            RCC_PeriphCLKInitTypeDef init;
            init.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
            init.PLL3.PLL3M = divm;
            init.PLL3.PLL3N = divn;
            init.PLL3.PLL3P = 128;
            init.PLL3.PLL3Q = 128;
            init.PLL3.PLL3R = divr;
            init.PLL3.PLL3RGE = vci;
            init.PLL3.PLL3VCOSEL = vco;
            init.PLL3.PLL3FRACN = frac;

            if (HAL_RCCEx_PeriphCLKConfig(&init) == HAL_OK) {
                return;
            }
        }
    }

    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Unable to initialize LTDC PLL!"));
}

static void get_display_mode(display_mode_t *dm_out, uint32_t framesize, bool portrait) {
    const display_mode_t *dm = &display_modes[framesize];
    if (portrait == false) {
        *dm_out = *dm;
    } else {
        dm_out->hactive = dm->vactive;
        dm_out->vactive = dm->hactive;
        dm_out->hsync_len = dm->vsync_len;
        dm_out->vsync_len = dm->hsync_len;
        dm_out->hback_porch = dm->vback_porch;
        dm_out->vback_porch = dm->hback_porch;
        dm_out->hfront_porch = dm->vfront_porch;
        dm_out->vfront_porch = dm->hfront_porch;
        dm_out->pixel_clock = dm->pixel_clock;
    }
}

#ifdef OMV_DSI_DISPLAY_CONTROLLER
static void dsi_init(py_display_obj_t *self) {
    display_mode_t dm;
    get_display_mode(&dm, self->framesize, self->portrait);
    uint32_t pixel_clock = (dm.pixel_clock * self->refresh) / 60;

    DSI_PLLInitTypeDef dsi_pllinit;
    dsi_pllinit.PLLNDIV = 125;
    dsi_pllinit.PLLIDF = DSI_PLL_IN_DIV4;
    dsi_pllinit.PLLODF = DSI_PLL_OUT_DIV1;
    uint32_t LANE_BYTE_CLOCK = 62500;

    display.hdsi.Instance = DSI;
    display.hdsi.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
    display.hdsi.Init.TXEscapeCkdiv = 4;
    display.hdsi.Init.AutomaticClockLaneControl = DSI_AUTO_CLK_LANE_CTRL_DISABLE;
    HAL_DSI_Init(&display.hdsi, &dsi_pllinit);

    #if OMV_DSI_DISPLAY_TE_ENABLE
    DSI_CmdCfgTypeDef dsi_cmd;
    dsi_cmd.VirtualChannelID = self->vcid;
    dsi_cmd.HSPolarity = DSI_HSYNC_ACTIVE_LOW;
    dsi_cmd.VSPolarity = DSI_VSYNC_ACTIVE_LOW;
    dsi_cmd.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;
    dsi_cmd.ColorCoding = DSI_RGB565;
    dsi_cmd.CommandSize = 0xFFFFU;
    dsi_cmd.TearingEffectSource = DSI_TE_DSILINK;
    dsi_cmd.TearingEffectPolarity = DSI_TE_RISING_EDGE;
    dsi_cmd.VSyncPol = DSI_VSYNC_FALLING;
    dsi_cmd.AutomaticRefresh = DSI_AR_DISABLE;
    dsi_cmd.TEAcknowledgeRequest = DSI_TE_ACKNOWLEDGE_ENABLE;
    HAL_DSI_ConfigAdaptedCommandMode(&display.hdsi, &dsi_cmd);
    #endif

    // Configure DSI PHY HS2LP and LP2HS timings
    DSI_PHY_TimerTypeDef dsi_phyinit;
    dsi_phyinit.ClockLaneHS2LPTime = 35;
    dsi_phyinit.ClockLaneLP2HSTime = 35;
    dsi_phyinit.DataLaneHS2LPTime = 35;
    dsi_phyinit.DataLaneLP2HSTime = 35;
    dsi_phyinit.DataLaneMaxReadTime = 0;
    dsi_phyinit.StopWaitTime = 10;
    HAL_DSI_ConfigPhyTimer(&display.hdsi, &dsi_phyinit);
    HAL_DSI_ConfigFlowControl(&display.hdsi, DSI_FLOW_CONTROL_BTA);
    HAL_DSI_SetLowPowerRXFilter(&display.hdsi, 10000);
    HAL_DSI_ConfigErrorMonitor(&display.hdsi, HAL_DSI_ERROR_NONE);

    // Timing parameters for Video modes
    DSI_VidCfgTypeDef dsi_vidcfg = { 0 };
    dsi_vidcfg.VirtualChannelID = self->vcid;
    dsi_vidcfg.ColorCoding = DSI_RGB565;
    dsi_vidcfg.LooselyPacked = DSI_LOOSELY_PACKED_DISABLE;
    dsi_vidcfg.VSPolarity = DSI_VSYNC_ACTIVE_LOW;
    dsi_vidcfg.HSPolarity = DSI_HSYNC_ACTIVE_LOW;
    dsi_vidcfg.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;
    dsi_vidcfg.Mode = DSI_VID_MODE_BURST;
    dsi_vidcfg.NullPacketSize = 0xFFF;
    dsi_vidcfg.NumberOfChunks = 0;
    dsi_vidcfg.PacketSize = dm.hactive;
    dsi_vidcfg.HorizontalSyncActive = dm.hsync_len * LANE_BYTE_CLOCK / pixel_clock;
    dsi_vidcfg.HorizontalBackPorch = dm.hback_porch * LANE_BYTE_CLOCK / pixel_clock;
    dsi_vidcfg.HorizontalLine = (dm.hactive + dm.hsync_len + dm.hback_porch + dm.hfront_porch)
                                * LANE_BYTE_CLOCK / pixel_clock;
    dsi_vidcfg.VerticalSyncActive = dm.vsync_len;
    dsi_vidcfg.VerticalBackPorch = dm.vback_porch;
    dsi_vidcfg.VerticalFrontPorch = dm.vfront_porch;
    dsi_vidcfg.VerticalActive = dm.vactive;

    // Enable/disable sending LP command while streaming
    dsi_vidcfg.LPCommandEnable = DSI_LP_COMMAND_ENABLE;
    // Largest packet size possible to transmit in LP mode in VSA, VBP, VFP regions
    dsi_vidcfg.LPLargestPacketSize = 0;
    // Largest packet size possible to transmit in LP mode in HFP region during VACT period
    dsi_vidcfg.LPVACTLargestPacketSize = 0;
    // Specify for each region, if the going in LP mode is allowed while streaming
    dsi_vidcfg.LPHorizontalFrontPorchEnable = DSI_LP_HFP_ENABLE;
    dsi_vidcfg.LPHorizontalBackPorchEnable = DSI_LP_HBP_ENABLE;
    dsi_vidcfg.LPVerticalActiveEnable = DSI_LP_VACT_ENABLE;
    dsi_vidcfg.LPVerticalFrontPorchEnable = DSI_LP_VFP_ENABLE;
    dsi_vidcfg.LPVerticalBackPorchEnable = DSI_LP_VBP_ENABLE;
    dsi_vidcfg.LPVerticalSyncActiveEnable = DSI_LP_VSYNC_ENABLE;

    // Configure DSI Video mode timings with settings set above
    HAL_DSI_ConfigVideoMode(&display.hdsi, &dsi_vidcfg);
    HAL_DSI_Start(&display.hdsi);
    HAL_DSI_Refresh(&display.hdsi);
    HAL_DSI_PatternGeneratorStop(&display.hdsi);

    HAL_NVIC_SetPriority(DSI_IRQn, IRQ_PRI_LTDC, 0);
    HAL_NVIC_EnableIRQ(DSI_IRQn);
}
#endif

static void ltdc_init(py_display_obj_t *self) {
    display_mode_t dm;
    get_display_mode(&dm, self->framesize, self->portrait);
    uint32_t fb_size = dm.hactive * dm.vactive * sizeof(uint16_t);

    fb_alloc_mark();
    for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
        self->framebuffers[i] = (uint16_t *) fb_alloc0(fb_size, FB_ALLOC_NO_HINT);
        display.framebuffer_layers[i].WindowX0 = 0;
        display.framebuffer_layers[i].WindowX1 = dm.hactive;
        display.framebuffer_layers[i].WindowY0 = 0;
        display.framebuffer_layers[i].WindowY1 = dm.vactive;
        display.framebuffer_layers[i].PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
        display.framebuffer_layers[i].Alpha = 0;
        display.framebuffer_layers[i].Alpha0 = 0;
        display.framebuffer_layers[i].BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
        display.framebuffer_layers[i].BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
        display.framebuffer_layers[i].FBStartAdress = (uint32_t) self->framebuffers[i];
        display.framebuffer_layers[i].ImageWidth = dm.hactive;
        display.framebuffer_layers[i].ImageHeight = dm.vactive;
        display.framebuffer_layers[i].Backcolor.Blue = 0;
        display.framebuffer_layers[i].Backcolor.Green = 0;
        display.framebuffer_layers[i].Backcolor.Red = 0;
    }
    fb_alloc_mark_permanent();

    display.hltdc.Instance = LTDC;
    display.hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AH,
    display.hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL,
    display.hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL,
    display.hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC,

    display.hltdc.Init.HorizontalSync = dm.hsync_len - 1;
    display.hltdc.Init.VerticalSync = dm.vsync_len - 1;
    display.hltdc.Init.AccumulatedHBP = dm.hsync_len + dm.hback_porch - 1;
    display.hltdc.Init.AccumulatedVBP = dm.vsync_len + dm.vback_porch - 1;
    display.hltdc.Init.AccumulatedActiveW = dm.hsync_len + dm.hback_porch + dm.hactive - 1;
    display.hltdc.Init.AccumulatedActiveH = dm.vsync_len + dm.vback_porch + dm.vactive - 1;
    display.hltdc.Init.TotalWidth = dm.hsync_len + dm.hback_porch + dm.hactive + dm.hfront_porch - 1;
    display.hltdc.Init.TotalHeigh = dm.vsync_len + dm.vback_porch + dm.vactive + dm.vfront_porch - 1;

    display.hltdc.Init.Backcolor.Blue = 0;
    display.hltdc.Init.Backcolor.Green = 0;
    display.hltdc.Init.Backcolor.Red = 0;
    HAL_LTDC_Init(&display.hltdc);

    NVIC_SetPriority(LTDC_IRQn, IRQ_PRI_LTDC);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);

    // Start interrupt chain.
    HAL_LTDC_Reload(&display.hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
}

void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc) {
    py_display_obj_t *self = display.self;

    HAL_LTDC_ConfigLayer_NoReload(&display.hltdc,
                                  &display.framebuffer_layers[self->framebuffer_tail], LTDC_LAYER_1);
    // Continue chain...
    HAL_LTDC_Reload(&display.hltdc, LTDC_RELOAD_VERTICAL_BLANKING);

    #if defined(OMV_DISPLAY_DISP_PIN)
    if (self->display_on && (self->framebuffer_tail != self->framebuffer_head)) {
        // Turn display on if there is a new command.
        omv_gpio_write(OMV_DISPLAY_DISP_PIN, 1);
    }
    #endif
    self->framebuffer_head = self->framebuffer_tail;
}

static void display_write(py_display_obj_t *self, image_t *src_img, int dst_x_start, int dst_y_start,
                          float x_scale, float y_scale, rectangle_t *roi, int rgb_channel, int alpha,
                          const uint16_t *color_palette, const uint8_t *alpha_palette, image_hint_t hint) {
    image_t dst_img;
    dst_img.w = self->width;
    dst_img.h = self->height;
    dst_img.pixfmt = PIXFORMAT_RGB565;

    point_t p0, p1;
    imlib_draw_image_get_bounds(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale,
                                roi, alpha, alpha_palette, hint, &p0, &p1);
    bool black = p0.x == -1;

    // For triple buffering we are never drawing where tail or head (which may instantly update to
    // to be equal to tail) is.
    int tail = (self->framebuffer_tail + 1) % FRAMEBUFFER_COUNT;
    if (tail == self->framebuffer_head) {
        tail = (tail + 1) % FRAMEBUFFER_COUNT;
    }
    dst_img.data = (uint8_t *) self->framebuffers[tail];

    // Set default values for the layer to display the whole framebuffer.
    display.framebuffer_layers[tail].WindowX0 = black ? 0 : p0.x;
    display.framebuffer_layers[tail].WindowX1 = black ? self->width : p1.x;
    display.framebuffer_layers[tail].WindowY0 = black ? 0 : p0.y;
    display.framebuffer_layers[tail].WindowY1 = black ? self->height : p1.y;
    display.framebuffer_layers[tail].Alpha = black ? 0 : alpha;
    display.framebuffer_layers[tail].FBStartAdress =
        black ? ((uint32_t) dst_img.data) : ((uint32_t) (IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, p0.y) + p0.x));
    display.framebuffer_layers[tail].ImageWidth = black ? self->width : dst_img.w;
    display.framebuffer_layers[tail].ImageHeight = black ? self->height : (p1.y - p0.y);

    // Set alpha to 256 here as we will use the layer alpha to blend the image into the background color of black for free.
    if (!black) {
        imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start,
                         x_scale, y_scale, roi, rgb_channel, 255, color_palette,
                         alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL, NULL);
    }

    #ifdef __DCACHE_PRESENT
    // Flush data for DMA
    if (!black) {
        SCB_CleanDCache_by_Addr((uint32_t *) dst_img.data, image_size(&dst_img));
    }
    #endif

    // Update tail which means a new image is ready.
    self->framebuffer_tail = tail;
}

static void display_clear(py_display_obj_t *self, bool off) {
    #if defined(OMV_DISPLAY_DISP_PIN)
    if (self->display_on && off) {
        // turns the display off (may not be black)
        omv_gpio_write(OMV_DISPLAY_DISP_PIN, 0);
        return;
    }
    #endif

    // For triple buffering we are never drawing where tail or head (which may instantly update to
    // to be equal to tail) is.
    int tail = (self->framebuffer_tail + 1) % FRAMEBUFFER_COUNT;
    if (tail == self->framebuffer_head) {
        tail = (tail + 1) % FRAMEBUFFER_COUNT;
    }

    // Set default values for the layer to display the whole framebuffer.
    display.framebuffer_layers[tail].WindowX0 = 0;
    display.framebuffer_layers[tail].WindowX1 = self->width;
    display.framebuffer_layers[tail].WindowY0 = 0;
    display.framebuffer_layers[tail].WindowY1 = self->height;
    display.framebuffer_layers[tail].Alpha = 0;
    display.framebuffer_layers[tail].FBStartAdress = (uint32_t) self->framebuffers[tail];
    display.framebuffer_layers[tail].ImageWidth = self->width;
    display.framebuffer_layers[tail].ImageHeight = self->height;

    // Update tail which means a new image is ready.
    self->framebuffer_tail = tail;
}

#ifdef OMV_DISPLAY_BL_PIN
static void display_set_backlight(py_display_obj_t *self, uint32_t intensity) {
    omv_gpio_config(OMV_DISPLAY_BL_PIN, OMV_GPIO_MODE_OUTPUT, OMV_GPIO_PULL_NONE, OMV_GPIO_SPEED_LOW, -1);
    omv_gpio_write(OMV_DISPLAY_BL_PIN, !!intensity);
}
#endif

#ifdef OMV_DSI_DISPLAY_CONTROLLER
int display_dsi_write(py_display_obj_t *self, uint8_t cmd, uint8_t *args, size_t n_args, bool dcs) {
    HAL_StatusTypeDef status = HAL_ERROR;
    if (n_args == 0) {
        status = HAL_DSI_ShortWrite(&display.hdsi, self->vcid, (dcs == true) ?
                                    DSI_DCS_SHORT_PKT_WRITE_P0 : DSI_GEN_SHORT_PKT_WRITE_P1, cmd, 0x00);
    } else if (n_args == 1) {
        status = HAL_DSI_ShortWrite(&display.hdsi, self->vcid, (dcs == true) ?
                                    DSI_DCS_SHORT_PKT_WRITE_P1 : DSI_GEN_SHORT_PKT_WRITE_P2, cmd, args[0]);
    } else {
        status = HAL_DSI_LongWrite(&display.hdsi, self->vcid, (dcs == true) ?
                                   DSI_DCS_LONG_PKT_WRITE : DSI_GEN_LONG_PKT_WRITE, n_args, cmd, args);
    }
    if (status != HAL_OK) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("DSI write command failed"));
    }
    return 0;
}

int display_dsi_read(py_display_obj_t *self, uint8_t cmd, uint8_t *args,
                     size_t n_args, uint8_t *buf, size_t len, bool dcs) {
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t params[] = { cmd, (args == NULL) ? 0 : args[0] };
    if (n_args == 0) {
        // For generic commands, the HAL expects cmd in ParametersTable[0U]
        status = HAL_DSI_Read(&display.hdsi, self->vcid, buf, len, (dcs == true) ?
                              DSI_DCS_SHORT_PKT_READ : DSI_GEN_SHORT_PKT_READ_P1, cmd, params);
    } else if (n_args == 1) {
        status = HAL_DSI_Read(&display.hdsi, self->vcid,
                              buf, len, DSI_GEN_SHORT_PKT_READ_P2, 0, params);
    }
    if (status != HAL_OK) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("DSI read command failed"));
    }
    return 0;
}
#endif

static void display_deinit(py_display_obj_t *self) {
    #ifdef OMV_DSI_DISPLAY_CONTROLLER
    HAL_DSI_DeInit(&display.hdsi);
    HAL_NVIC_DisableIRQ(DSI_IRQn);
    #endif

    HAL_LTDC_DeInit(&display.hltdc);
    HAL_NVIC_DisableIRQ(LTDC_IRQn);

    __HAL_RCC_PLL3_DISABLE();
    uint32_t tickstart = mp_hal_ticks_ms();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY)) {
        if ((mp_hal_ticks_ms() - tickstart) > PLL_TIMEOUT_VALUE) {
            break;
        }
    }

    #ifdef OMV_DISPLAY_BL_PIN
    omv_gpio_deinit(OMV_DISPLAY_BL_PIN);
    #endif

    fb_alloc_free_till_mark_past_mark_permanent();
}

mp_obj_t display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_framesize, ARG_refresh, ARG_display_on, ARG_triple_buffer,
        ARG_portrait, ARG_channel, ARG_controller, ARG_backlight
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_framesize,     MP_ARG_INT,  {.u_int = DISPLAY_RESOLUTION_FWVGA  } },
        { MP_QSTR_refresh,       MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 60  } },
        { MP_QSTR_display_on,    MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
        { MP_QSTR_triple_buffer, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
        { MP_QSTR_portrait,      MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = false} },
        { MP_QSTR_channel,       MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 0  } },
        { MP_QSTR_controller,    MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_backlight,     MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if ((args[ARG_framesize].u_int < 0) || (args[ARG_framesize].u_int >= DISPLAY_RESOLUTION_MAX)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid Frame Size!"));
    }
    if ((args[ARG_refresh].u_int < 30) || (args[ARG_refresh].u_int > 120)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid Refresh Rate!"));
    }

    py_display_obj_t *self;
    #ifdef OMV_DSI_DISPLAY_CONTROLLER
    self = mp_obj_malloc_with_finaliser(py_display_obj_t, &py_dsi_display_type);
    #else
    self = mp_obj_malloc_with_finaliser(py_display_obj_t, &py_rgb_display_type);
    #endif
    self->vcid = args[ARG_channel].u_int;
    self->refresh = args[ARG_refresh].u_int;
    self->display_on = args[ARG_display_on].u_bool;
    self->bgr = false;
    self->triple_buffer = args[ARG_triple_buffer].u_bool;
    self->framebuffer_tail = 0;
    self->framebuffer_head = 0;
    self->framesize = args[ARG_framesize].u_int;
    self->portrait = args[ARG_portrait].u_bool;
    if (self->portrait) {
        self->width = display_modes[self->framesize].vactive;
        self->height = display_modes[self->framesize].hactive;
    } else {
        self->width = display_modes[self->framesize].hactive;
        self->height = display_modes[self->framesize].vactive;
    }
    self->controller = args[ARG_controller].u_obj;
    self->bl_controller = args[ARG_backlight].u_obj;

    // Store state to access it from IRQ handlers or callbacks
    display.self = self;

    // Configure PLL3 for the selected mode clock.
    pll_config(self->framesize, self->refresh);

    // Init LTDC controller
    ltdc_init(self);

    #ifdef OMV_DSI_DISPLAY_CONTROLLER
    // Init DSI controller
    dsi_init(self);

    // Init the display controller.
    if (self->controller != mp_const_none) {
        mp_obj_t dest[3];
        mp_load_method_maybe(self->controller, MP_QSTR_init, dest);
        if (dest[0] != MP_OBJ_NULL) {
            dest[2] = MP_OBJ_FROM_PTR(self);
            mp_call_method_n_kw(1, 0, dest);
        }
    }
    #endif

    return MP_OBJ_FROM_PTR(self);
}

static const py_display_p_t py_display_p = {
    .deinit = display_deinit,
    .clear = display_clear,
    .write = display_write,
    #ifdef OMV_DISPLAY_BL_PIN
    .set_backlight = display_set_backlight,
    #endif
    #ifdef OMV_DSI_DISPLAY_CONTROLLER
    .bus_write = display_dsi_write,
    .bus_read = display_dsi_read,
    #endif
};

#ifdef OMV_RGB_DISPLAY_CONTROLLER
MP_DEFINE_CONST_OBJ_TYPE(
    py_rgb_display_type,
    MP_QSTR_RGBDisplay,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    protocol, &py_display_p,
    locals_dict, &py_display_locals_dict
    );
#endif

#ifdef OMV_DSI_DISPLAY_CONTROLLER
MP_DEFINE_CONST_OBJ_TYPE(
    py_dsi_display_type,
    MP_QSTR_DSIDisplay,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    protocol, &py_display_p,
    locals_dict, &py_display_locals_dict
    );
#endif

#endif // MICROPY_PY_DISPLAY
