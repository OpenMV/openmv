/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013-2024 OpenMV, LLC.
 * Copyright (c) 2019-2019 Kaizhi Wong <kidswong999@gmail.com>
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
 * TV Python module.
 */
#include "omv_boardconfig.h"

#if MICROPY_PY_TV

#include "py/obj.h"
#include "py/nlr.h"
#include "py/mphal.h"
#include "py/runtime.h"

#include "py_helper.h"
#include "py_image.h"
#include "omv_gpio.h"
#include "omv_spi.h"

#define TV_WIDTH      352
#define TV_HEIGHT     240
#define TV_REFRESH    60

#if ((TV_WIDTH) % 2)
#error "TV_WIDTH not even"
#endif

#if ((TV_HEIGHT) % 2)
#error "TV_HEIGHT not even"
#endif

#ifdef OMV_SPI_DISPLAY_CONTROLLER
/////////////////////////////////////////////////////////////
// http://www.vsdsp-forum.com/phpbb/viewtopic.php?f=14&t=1801
/////////////////////////////////////////////////////////////

// Crystal frequency in MHZ (float, observe accuracy)
#define XTAL_MHZ                     3.579545

// Line length in microseconds (float, observe accuracy)
#define LINE_LENGTH_US               63.556

#define FIXED_VCLK_CYCLES            10
#define FIXED_CSCLK_CYCLES           ((FIXED_VCLK_CYCLES) / 8.0)

// Normal visible picture line sync length is 4.7 us
#define SYNC_US                      4.7
#define SYNC                         ((uint16_t) (((SYNC_US) *(XTAL_MHZ)) - (FIXED_CSCLK_CYCLES) +0.5))

// Color burst starts at 5.3 us
#define BURST_US                     5.3
#define BURST                        ((uint16_t) (((BURST_US) *(XTAL_MHZ)) - (FIXED_CSCLK_CYCLES) +0.5))

// Color burst duration is 2.5 us
#define BURST_DUR_US                 2.5
#define BURST_DUR                    ((uint16_t) (((BURST_DUR_US) *(XTAL_MHZ)) + 0.5))

// Black video starts at 9.4 us
#define BLACK_US                     9.4
#define BLACK                        ((uint16_t) (((BLACK_US) *(XTAL_MHZ)) - (FIXED_CSCLK_CYCLES) +0.5))

// Black video duration is 52.656 us
#define BLACK_DUR_US                 52.656
#define BLACK_DUR                    ((uint16_t) (((BLACK_DUR_US) *(XTAL_MHZ)) + 0.5))

// Define NTSC video timing constants
// NTSC short sync duration is 2.3 us
#define SHORT_SYNC_US                2.3

// For the start of the line, the first 10 extra PLLCLK sync (0) cycles are subtracted.
#define SHORTSYNC                    ((uint16_t) (((SHORT_SYNC_US) *(XTAL_MHZ)) - (FIXED_CSCLK_CYCLES) +0.5))

// For the middle of the line the whole duration of sync pulse is used.
#define SHORTSYNCM                   ((uint16_t) (((SHORT_SYNC_US) *(XTAL_MHZ)) + 0.5))

// NTSC long sync duration is 27.078 us
#define LONG_SYNC_US                 27.078
#define LONGSYNC                     ((uint16_t) (((LONG_SYNC_US) *(XTAL_MHZ)) - (FIXED_CSCLK_CYCLES) +0.5))
#define LONGSYNCM                    ((uint16_t) (((LONG_SYNC_US) *(XTAL_MHZ)) + 0.5))

// Number of lines used after the VSYNC but before visible area.
#define VSYNC_LINES                  9
#define FRONT_PORCH_LINES            13

// Definitions for picture lines
// On which line the picture area begins, the Y direction.
#define STARTLINE                    ((VSYNC_LINES) + (FRONT_PORCH_LINES))

// Frame length in lines (visible lines + nonvisible lines)
// Amount has to be odd for NTSC and RGB colors
#define TOTAL_LINES                  ((STARTLINE) + (TV_HEIGHT) +1)
#if ((TOTAL_LINES) != 263)
#error "Progressive NTSC must have 263 lines!"
#endif

// Width, in PLL clocks, of each pixel.
#define PLLCLKS_PER_PIXEL            4

// The first pixel of the picture area, the X direction.
#define STARTPIX                     ((BLACK) +7)

// The last pixel of the picture area.
#define ENDPIX                       ((uint16_t) ((STARTPIX) + (((PLLCLKS_PER_PIXEL) *(TV_WIDTH)) / 8)))

// Reserve memory for this number of different prototype lines
// (prototype lines are used for sync timing, porch and border area)
#define PROTOLINES                   3

// PLL frequency
#define PLL_MHZ                      ((XTAL_MHZ) * 8)

// 10 first pllclks, which are not in the counters are decremented here
#define PLLCLKS_PER_LINE             ((uint16_t) (((LINE_LENGTH_US) *(PLL_MHZ)) - (FIXED_VCLK_CYCLES)))

// 10 first pllclks, which are not in the counters are decremented here
#define COLORCLKS_PER_LINE           ((uint16_t) ((((((LINE_LENGTH_US) *(PLL_MHZ)) / 1) + 7) / 8) - (FIXED_CSCLK_CYCLES)))
#define COLORCLKS_LINE_HALF          ((uint16_t) ((((((LINE_LENGTH_US) *(PLL_MHZ)) / 2) + 7) / 8) - (FIXED_CSCLK_CYCLES)))

#define PROTO_AREA_WORDS             ((COLORCLKS_PER_LINE) *(PROTOLINES))
#define INDEX_START_LONGWORDS        (((PROTO_AREA_WORDS) +1) / 2)
#define INDEX_START_BYTES            ((INDEX_START_LONGWORDS) * 4)

// Protoline 0 starts always at address 0
#define PROTOLINE_BYTE_ADDRESS(n)    ((COLORCLKS_PER_LINE) * 2 * (n))
#define PROTOLINE_WORD_ADDRESS(n)    ((COLORCLKS_PER_LINE) * 1 * (n))

// Calculate picture lengths in pixels and bytes, coordinate areas for picture area
#define PICBITS                      12
#define PICLINE_LENGTH_BYTES         (((TV_WIDTH) *(PICBITS)) / 8)

#define LINE_INDEX_BYTE_SIZE         3

// Picture area memory start point
#define PICLINE_START                ((INDEX_START_BYTES) + ((TOTAL_LINES) *(LINE_INDEX_BYTE_SIZE)))

// Picture area line start addresses
#define PICLINE_BYTE_ADDRESS(n)      ((PICLINE_START) + ((PICLINE_LENGTH_BYTES) *(n)))

// Pattern generator microcode
// ---------------------------
// Bits 7:6  a=00|b=01|y=10|-=11
// Bits 5:3  n pick bits 1..8
// bits 2:0  shift 0..6
#define PICK_A                       (0 << 6)
#define PICK_B                       (1 << 6)
#define PICK_Y                       (2 << 6)
#define PICK_NOTHING                 (3 << 6)
#define PICK_BITS(a)                 (((a) - 1) << 3)
#define SHIFT_BITS(a)                (a)

// 16 bits per pixel, U4 V4 Y8
// PICK_B is U
#define OP1                          (PICK_B + PICK_BITS(4) + SHIFT_BITS(4))
// PICK_A is V
#define OP2                          (PICK_A + PICK_BITS(4) + SHIFT_BITS(4))
#define OP3                          (PICK_Y + PICK_BITS(8) + SHIFT_BITS(6))
#define OP4                          (PICK_NOTHING + SHIFT_BITS(2))

// General VS23 commands
#define WRITE_STATUS                 0x01
#define WRITE_SRAM                   0x02
#define WRITE_GPIO                   0x82
#define READ_GPIO                    0x84
#define WRITE_MULTIIC                0xb8
#define WRITE_BLOCKMVC1              0x34

// Bit definitions
#define VDCTRL1                      0x2B
#define VDCTRL1_UVSKIP               (1 << 0)
#define VDCTRL1_PLL_ENABLE           (1 << 12)
#define VDCTRL2                      0x2D
#define VDCTRL2_LINECOUNT            (1 << 0)
#define VDCTRL2_PIXEL_WIDTH          (1 << 10)
#define VDCTRL2_ENABLE_VIDEO         (1 << 15)
#define BLOCKMVC1_PYF                (1 << 4)

// VS23 video commands
#define PROGRAM                      0x30
#define PICSTART                     0x28
#define PICEND                       0x29
#define LINELEN                      0x2a
#define INDEXSTART                   0x2c

// Sync, blank, burst and white level definitions, here are several options
// These are for proto lines and so format is VVVVUUUUYYYYYYYY

// Sync is always 0
#define SYNC_LEVEL                   0x0000

// 285 mV to 75 ohm load
#define BLANK_LEVEL                  0x0066

// 285 mV burst
#define BURST_LEVEL                  0x0d66

#define SPI_RAM_SIZE                 (128 * 1024)

// COLORCLKS_PER_LINE can't be used in pre-processor logic.
#if ((((((227 * (PROTOLINES)) + 1) / 2) * 4) + ((TOTAL_LINES) *(LINE_INDEX_BYTE_SIZE)) + \
    ((PICLINE_LENGTH_BYTES) *(TV_HEIGHT))) > (SPI_RAM_SIZE))
#error "TV_WIDTH * TV_HEIGHT is too big!"
#endif

#define TV_BAUDRATE                  (TV_WIDTH * TV_HEIGHT * TV_REFRESH * PICBITS)

#if OMV_SPI_DISPLAY_TRIPLE_BUFFER
#define TV_TRIPLE_BUFFER_DEFAULT   (true)
#else
#define TV_TRIPLE_BUFFER_DEFAULT   (false)
#endif

static omv_spi_t spi_bus = {};

static void SpiTransmitReceivePacket(uint8_t *txdata, uint8_t *rxdata, uint16_t size, bool end) {
    omv_spi_transfer_t spi_xfer = {
        .txbuf = txdata,
        .rxbuf = rxdata,
        .size = size,
        .timeout = OMV_SPI_MAX_TIMEOUT,
        .flags = OMV_SPI_XFER_BLOCKING
    };

    omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);
    omv_spi_transfer_start(&spi_bus, &spi_xfer);

    if (end) {
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
    }
}

static void SpiRamWriteByteRegister(int opcode, int data) {
    uint8_t packet[2] = {opcode, data};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static int SpiRamReadByteRegister(int opcode) {
    uint8_t packet[2] = {opcode, 0};
    SpiTransmitReceivePacket(packet, packet, sizeof(packet), true);
    return packet[1];
}

static void SpiRamWriteWordRegister(int opcode, int data) {
    uint8_t packet[3] = {opcode, data >> 8, data};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static void SpiClearRam() {
    uint8_t packet[4] = {WRITE_SRAM, 0, 0, 0};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), false);
    packet[0] = 0;

    for (int i = 0; i < (SPI_RAM_SIZE / sizeof(packet)); i++) {
        SpiTransmitReceivePacket(packet, NULL, sizeof(packet), (i + 1) == (SPI_RAM_SIZE / sizeof(packet)));
    }
}

static void SpiRamWriteProgram(int data0, int data1, int data2, int data3) {
    uint8_t packet[5] = {PROGRAM, data3, data2, data1, data0};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static void SpiRamWriteLowPassFilter(int data) {
    uint8_t packet[6] = {WRITE_BLOCKMVC1, 0, 0, 0, 0, data};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static void SpiRamWriteWord(int w_address, int data) {
    int address = w_address * sizeof(uint16_t);
    uint8_t packet[6] = {WRITE_SRAM, address >> 16, address >> 8, address, data >> 8, data};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static void SpiRamWriteVSyncProtoLine(int line, int length_1, int length_2) {
    int w0 = PROTOLINE_WORD_ADDRESS(line);
    for (int i = 0; i < COLORCLKS_PER_LINE; i++) {
        SpiRamWriteWord(w0++, BLANK_LEVEL);
    }

    int w1 = PROTOLINE_WORD_ADDRESS(line);
    for (int i = 0; i < length_1; i++) {
        SpiRamWriteWord(w1++, SYNC_LEVEL);
    }

    int w2 = PROTOLINE_WORD_ADDRESS(line) + COLORCLKS_LINE_HALF;
    for (int i = 0; i < length_2; i++) {
        SpiRamWriteWord(w2++, SYNC_LEVEL);
    }
}

static void SpiRamWriteLine(int line, int index) {
    int address = INDEX_START_BYTES + (line * LINE_INDEX_BYTE_SIZE);
    int data = index << 7;
    uint8_t packet[7] = {WRITE_SRAM, address >> 16, address >> 8, address, data, data >> 8, data >> 16};
    SpiTransmitReceivePacket(packet, NULL, sizeof(packet), true);
}

static void SpiRamVideoInit() {
    // Select the first VS23 for following commands in case there
    // are several VS23 ICs connected to same SPI bus.
    SpiRamWriteByteRegister(WRITE_MULTIIC, 0xe);

    // Set SPI memory address autoincrement
    SpiRamWriteByteRegister(WRITE_STATUS, 0x40);

    // Reset the video display controller
    SpiRamWriteWordRegister(VDCTRL1, 0);
    SpiRamWriteWordRegister(VDCTRL2, 0);

    // Write picture start and end
    SpiRamWriteWordRegister(PICSTART, (STARTPIX - 1));
    SpiRamWriteWordRegister(PICEND, (ENDPIX - 1));

    // Enable PLL clock
    SpiRamWriteWordRegister(VDCTRL1, VDCTRL1_PLL_ENABLE | VDCTRL1_UVSKIP);

    // Clear the video memory
    SpiClearRam();

    // Set length of one complete line (unit: PLL clocks)
    SpiRamWriteWordRegister(LINELEN, PLLCLKS_PER_LINE);

    // Set microcode program for picture lines
    SpiRamWriteProgram(OP1, OP2, OP3, OP4);

    // Define where Line Indexes are stored in memory
    SpiRamWriteWordRegister(INDEXSTART, INDEX_START_LONGWORDS);

    // At this time, the chip would continuously output the proto line 0.
    // This protoline will become our most "normal" horizontal line.
    // For TV-Out, fill the line with black level,
    // and insert a few pixels of sync level (0) and color burst to the beginning.
    // Note that the chip hardware adds black level to all nonproto areas so
    // protolines and normal picture have different meaning for the same Y value.
    // In protolines, Y=0 is at sync level and in normal picture Y=0 is at black level (offset +102).

    // In protolines, each pixel is 8 PLLCLKs, which in TV-out modes means one color
    // subcarrier cycle. Each pixel has 16 bits (one word): VVVVUUUUYYYYYYYY.

    SpiRamWriteVSyncProtoLine(0, SYNC, 0);

    int w = PROTOLINE_WORD_ADDRESS(0) + BURST;
    for (int i = 0; i < BURST_DUR; i++) {
        SpiRamWriteWord(w++, BURST_LEVEL);
    }

    // short_low + long_high + short_low + long_high
    SpiRamWriteVSyncProtoLine(1, SHORTSYNC, SHORTSYNCM);

    // long_low + short_high + long_low + short_high
    SpiRamWriteVSyncProtoLine(2, LONGSYNC, LONGSYNCM);

    for (int i = 0; i <= 2; i++) {
        SpiRamWriteLine(i, PROTOLINE_BYTE_ADDRESS(1)); // short_low + long_high + short_low + long_high
    }

    for (int i = 3; i <= 5; i++) {
        SpiRamWriteLine(i, PROTOLINE_BYTE_ADDRESS(2)); // long_low + short_high + long_low + short_high
    }

    for (int i = 6; i <= 8; i++) {
        SpiRamWriteLine(i, PROTOLINE_BYTE_ADDRESS(1)); // short_low + long_high + short_low + long_high
    }

    // Set pic line indexes to point to protoline 0 and their individual picture line.
    for (int i = 0; i < TV_HEIGHT; i++) {
        SpiRamWriteLine(STARTLINE + i, PICLINE_BYTE_ADDRESS(i));
    }

    // Set number of lines, length of pixel and enable video generation
    SpiRamWriteWordRegister(VDCTRL2, (VDCTRL2_LINECOUNT * (TOTAL_LINES - 1))
                            | (VDCTRL2_PIXEL_WIDTH * (PLLCLKS_PER_PIXEL - 1))
                            | (VDCTRL2_ENABLE_VIDEO));

    // Enable the low-pass Y filter.
    SpiRamWriteLowPassFilter(BLOCKMVC1_PYF);
}
#endif

// TV lines are converted from 16-bit RGB565 to 12-bit YUV.
#define TV_WIDTH_RGB565      ((TV_WIDTH) * 2) // bytes

#if ((PICLINE_LENGTH_BYTES) > (TV_WIDTH_RGB565))
#error "PICLINE_LENGTH_BYTES > TV_WIDTH_RGB565"
#endif

#define FRAMEBUFFER_COUNT    3
static int framebuffer_head = 0;
static volatile int framebuffer_tail = 0;
static uint16_t *framebuffers[FRAMEBUFFER_COUNT] = {};

typedef enum tv_type {
    TV_NONE,
    TV_SHIELD,
} tv_type_t;

static tv_type_t tv_type = TV_NONE;
static bool tv_triple_buffer = false;

#ifdef OMV_SPI_DISPLAY_CONTROLLER
static volatile enum {
    SPI_TX_CB_IDLE,
    SPI_TX_CB_MEMORY_WRITE_CMD,
    SPI_TX_CB_MEMORY_WRITE
}
spi_tx_cb_state = SPI_TX_CB_IDLE;

static void spi_config_deinit() {
    if (tv_triple_buffer) {
        omv_spi_transfer_abort(&spi_bus);
        spi_tx_cb_state = SPI_TX_CB_IDLE;
        fb_alloc_free_till_mark_past_mark_permanent();
    }

    omv_spi_deinit(&spi_bus);
}

static void spi_config_init(bool triple_buffer) {
    omv_spi_config_t spi_config;
    omv_spi_default_config(&spi_config, OMV_SPI_DISPLAY_CONTROLLER);

    spi_config.baudrate = TV_BAUDRATE;
    spi_config.nss_enable = false;
    spi_config.dma_flags = triple_buffer ? OMV_SPI_DMA_NORMAL : 0;
    omv_spi_init(&spi_bus, &spi_config);

    omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);

    SpiRamVideoInit();

    // Set default channel.
    SpiRamWriteByteRegister(WRITE_GPIO, 0x77);

    if (triple_buffer) {
        fb_alloc_mark();

        framebuffer_head = 0;
        framebuffer_tail = 0;

        for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
            framebuffers[i] = (uint16_t *) fb_alloc0(TV_WIDTH_RGB565 * TV_HEIGHT);
        }

        fb_alloc_mark_permanent();
    }
}

static const uint8_t write_sram[] = {
    // Cannot be allocated on the stack.
    WRITE_SRAM,
    (uint8_t) (PICLINE_BYTE_ADDRESS(0) >> 16),
    (uint8_t) (PICLINE_BYTE_ADDRESS(0) >> 8),
    (uint8_t) (PICLINE_BYTE_ADDRESS(0) >> 0)
};

static void spi_tv_callback(omv_spi_t *spi, void *userdata, void *buf) {
    if (tv_type == TV_SHIELD) {
        static uint8_t *spi_tx_cb_state_memory_write_addr = NULL;
        static size_t spi_tx_cb_state_memory_write_count = 0;

        switch (spi_tx_cb_state) {
            case SPI_TX_CB_MEMORY_WRITE_CMD: {
                omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
                spi_tx_cb_state = SPI_TX_CB_MEMORY_WRITE;
                spi_tx_cb_state_memory_write_addr = (uint8_t *) framebuffers[framebuffer_head];
                spi_tx_cb_state_memory_write_count = PICLINE_LENGTH_BYTES * TV_HEIGHT;
                framebuffer_tail = framebuffer_head;
                omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 0);
                // When starting the interrupt chain the first transfer is not executed
                // in interrupt context. So, disable interrupts for the first transfer so
                // that it completes first and unlocks the SPI bus before allowing the interrupt
                // it causes to trigger starting the interrupt chain.
                omv_spi_transfer_t spi_xfer = {
                    .txbuf = (uint8_t *) write_sram,
                    .size = sizeof(write_sram),
                    .flags = OMV_SPI_XFER_NONBLOCK,
                    .callback = spi_tv_callback,
                };
                uint32_t irq_state = MICROPY_BEGIN_ATOMIC_SECTION();
                omv_spi_transfer_start(&spi_bus, &spi_xfer);
                MICROPY_END_ATOMIC_SECTION(irq_state);
                break;
            }
            case SPI_TX_CB_MEMORY_WRITE: {
                uint8_t *addr = spi_tx_cb_state_memory_write_addr;
                size_t count = IM_MIN(spi_tx_cb_state_memory_write_count, OMV_SPI_MAX_8BIT_XFER);
                spi_tx_cb_state = (spi_tx_cb_state_memory_write_count > OMV_SPI_MAX_8BIT_XFER)
                        ? SPI_TX_CB_MEMORY_WRITE
                        : SPI_TX_CB_MEMORY_WRITE_CMD;
                spi_tx_cb_state_memory_write_addr += count;
                spi_tx_cb_state_memory_write_count -= count;
                omv_spi_transfer_t spi_xfer = {
                    .txbuf = addr,
                    .size = count,
                    .flags = OMV_SPI_XFER_DMA,
                    .callback = spi_tv_callback,
                };
                omv_spi_transfer_start(&spi_bus, &spi_xfer);
                break;
            }
            default: {
                break;
            }
        }
    }
}

// Convert a 8-bit Grayscale line of pixels to 12-bit YUV422 with padding (line is 16-bit per pixel).
static void spi_tv_draw_image_cb_convert_grayscale(uint8_t *row_pointer_i, uint8_t *row_pointer_o) {
    for (int i = TV_WIDTH - 2, j = ((TV_WIDTH * 3) / 2) - 3; i >= 0; i -= 2, j -= 3) {
        int y0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_pointer_i, i);
        int y1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_pointer_i, i + 1);
        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j, 0);
        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j + 1, y0);
        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j + 2, y1);
    }
}

// Convert a 16-bit RGB565 line of pixels to 12-bit YUV422 with padding (line is 16-bit per pixel).
static void spi_tv_draw_image_cb_convert_rgb565(uint16_t *row_pointer_i, uint8_t *row_pointer_o) {
    for (int i = 0, j = 0; i < TV_WIDTH; i += 2, j += 3) {
        #if defined(ARM_MATH_DSP)

        int pixels = *((uint32_t *) (row_pointer_i + i));
        int r_pixels = ((pixels >> 8) & 0xf800f8) | ((pixels >> 13) & 0x70007);
        int g_pixels = ((pixels >> 3) & 0xfc00fc) | ((pixels >> 9) & 0x30003);
        int b_pixels = ((pixels << 3) & 0xf800f8) | ((pixels >> 2) & 0x70007);

        int y = ((r_pixels * 38) + (g_pixels * 75) + (b_pixels * 15)) >> 7;
        int u = __SSUB16(b_pixels * 64, (r_pixels * 21) + (g_pixels * 43));
        int v = __SSUB16(r_pixels * 64, (g_pixels * 54) + (b_pixels * 10));

        int y0 = __UXTB_RORn(y, 0), y1 = __UXTB_RORn(y, 16);

        int u_avg = __SMUAD(u, 0x00010001) >> 7;
        int v_avg = __SMUAD(v, 0x00010001) >> 7;

        #else

        int pixel0 = IMAGE_GET_RGB565_PIXEL_FAST(row_pointer_i, i);
        int r0 = COLOR_RGB565_TO_R8(pixel0);
        int g0 = COLOR_RGB565_TO_G8(pixel0);
        int b0 = COLOR_RGB565_TO_B8(pixel0);
        int y0 = COLOR_RGB888_TO_Y(r0, g0, b0);
        int u0 = COLOR_RGB888_TO_U(r0, g0, b0);
        int v0 = COLOR_RGB888_TO_V(r0, g0, b0);

        int pixel1 = IMAGE_GET_RGB565_PIXEL_FAST(row_pointer_i, i + 1);
        int r1 = COLOR_RGB565_TO_R8(pixel1);
        int g1 = COLOR_RGB565_TO_G8(pixel1);
        int b1 = COLOR_RGB565_TO_B8(pixel1);
        int y1 = COLOR_RGB888_TO_Y(r1, g1, b1);
        int u1 = COLOR_RGB888_TO_U(r1, g1, b1);
        int v1 = COLOR_RGB888_TO_V(r1, g1, b1);

        int u_avg = u0 + u1;
        int v_avg = v0 + v1;

        #endif

        int uv = ((u_avg >> 1) & 0xf0) | (((-v_avg) >> 5) & 0xf);

        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j, uv);
        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j + 1, y0);
        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_pointer_o, j + 2, y1);
    }
}

static void spi_tv_draw_image_cb_grayscale(int x_start, int x_end, int y_row, imlib_draw_row_data_t *data) {
    memset(((uint8_t *) data->dst_row_override) + x_end, 0, TV_WIDTH - x_end); // clear trailing bytes.
    spi_tv_draw_image_cb_convert_grayscale((uint8_t *) data->dst_row_override, (uint8_t *) data->dst_row_override);
    SpiTransmitReceivePacket(data->dst_row_override, NULL, PICLINE_LENGTH_BYTES, false);
}

static void spi_tv_draw_image_cb_rgb565(int x_start, int x_end, int y_row, imlib_draw_row_data_t *data) {
    memset(data->dst_row_override, 0, x_start * sizeof(uint16_t)); // clear leading bytes.
    spi_tv_draw_image_cb_convert_rgb565((uint16_t *) data->dst_row_override, (uint8_t *) data->dst_row_override);
    SpiTransmitReceivePacket(data->dst_row_override, NULL, PICLINE_LENGTH_BYTES, false);
}

static void spi_tv_display(image_t *src_img, int dst_x_start, int dst_y_start, float x_scale, float y_scale,
                           rectangle_t *roi, int rgb_channel, int alpha,
                           const uint16_t *color_palette, const uint8_t *alpha_palette,
                           image_hint_t hint) {
    bool rgb565 = ((rgb_channel == -1) && src_img->is_color) || color_palette;
    imlib_draw_row_callback_t cb = rgb565 ? spi_tv_draw_image_cb_rgb565 : spi_tv_draw_image_cb_grayscale;

    image_t dst_img;
    dst_img.w = TV_WIDTH;
    dst_img.h = TV_HEIGHT;
    dst_img.pixfmt = rgb565 ? PIXFORMAT_RGB565 : PIXFORMAT_GRAYSCALE;

    point_t p0, p1;
    imlib_draw_image_get_bounds(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale,
                                roi, alpha, alpha_palette, hint, &p0, &p1);
    bool black = p0.x == -1;

    if (!tv_triple_buffer) {
        dst_img.data = fb_alloc0(TV_WIDTH_RGB565);

        SpiTransmitReceivePacket((uint8_t *) write_sram, NULL, sizeof(write_sram), false);

        if (black) {
            // zero the whole image
            for (int i = 0; i < TV_HEIGHT; i++) {
                SpiTransmitReceivePacket(dst_img.data, NULL, PICLINE_LENGTH_BYTES, false);
            }
        } else {
            // Zero the top rows
            for (int i = 0; i < p0.y; i++) {
                SpiTransmitReceivePacket(dst_img.data, NULL, PICLINE_LENGTH_BYTES, false);
            }

            // Transmits left/right parts already zeroed...
            imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                             rgb_channel, alpha, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND,
                             cb, NULL, dst_img.data);

            // Zero the bottom rows
            if (p1.y < TV_HEIGHT) {
                memset(dst_img.data, 0, TV_WIDTH_RGB565);
            }

            for (int i = p1.y; i < TV_HEIGHT; i++) {
                SpiTransmitReceivePacket(dst_img.data, NULL, PICLINE_LENGTH_BYTES, false);
            }
        }

        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
        fb_free();
    } else {
        // For triple buffering we are never drawing where head or tail (which may instantly update to
        // to be equal to head) is.
        int new_framebuffer_head = (framebuffer_head + 1) % FRAMEBUFFER_COUNT;
        if (new_framebuffer_head == framebuffer_tail) {
            new_framebuffer_head = (new_framebuffer_head + 1) % FRAMEBUFFER_COUNT;
        }

        dst_img.data = (uint8_t *) framebuffers[new_framebuffer_head];

        if (rgb565) {
            if (black) {
                // zero the whole image
                memset(dst_img.data, 0, TV_WIDTH * TV_HEIGHT * sizeof(uint16_t));
            } else {
                // Zero the top rows
                if (p0.y) {
                    memset(dst_img.data, 0, TV_WIDTH * p0.y * sizeof(uint16_t));
                }

                if (p0.x) {
                    for (int i = p0.y; i < p1.y; i++) {
                        // Zero left
                        memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, i), 0, p0.x * sizeof(uint16_t));
                    }
                }

                imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                                 rgb_channel, alpha, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND,
                                 NULL, NULL, NULL);

                if (TV_WIDTH - p1.x) {
                    for (int i = p0.y; i < p1.y; i++) {
                        // Zero right
                        memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, i) + p1.x, 0,
                               (TV_WIDTH - p1.x) * sizeof(uint16_t));
                    }
                }

                // Zero the bottom rows
                if (TV_HEIGHT - p1.y) {
                    memset(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, p1.y), 0,
                           TV_WIDTH * (TV_HEIGHT - p1.y) * sizeof(uint16_t));
                }
            }

            for (int i = 0; i < TV_HEIGHT; i++) {
                // Convert the image.
                spi_tv_draw_image_cb_convert_rgb565(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&dst_img, i),
                                                    dst_img.data + (PICLINE_LENGTH_BYTES * i));
            }
        } else {
            if (black) {
                // zero the whole image
                memset(dst_img.data, 0, TV_WIDTH * TV_HEIGHT * sizeof(uint8_t));
            } else {
                // Zero the top rows
                if (p0.y) {
                    memset(dst_img.data, 0, TV_WIDTH * p0.y * sizeof(uint8_t));
                }

                if (p0.x) {
                    for (int i = p0.y; i < p1.y; i++) {
                        // Zero left
                        memset(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&dst_img, i), 0, p0.x * sizeof(uint8_t));
                    }
                }

                imlib_draw_image(&dst_img, src_img, dst_x_start, dst_y_start, x_scale, y_scale, roi,
                                 rgb_channel, alpha, color_palette, alpha_palette, hint | IMAGE_HINT_BLACK_BACKGROUND,
                                 NULL, NULL, NULL);

                if (TV_WIDTH - p1.x) {
                    for (int i = p0.y; i < p1.y; i++) {
                        // Zero right
                        memset(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&dst_img, i) + p1.x, 0,
                               (TV_WIDTH - p1.x) * sizeof(uint8_t));
                    }
                }

                // Zero the bottom rows
                if (TV_HEIGHT - p1.y) {
                    memset(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&dst_img, p1.y), 0,
                           TV_WIDTH * (TV_HEIGHT - p1.y) * sizeof(uint8_t));
                }
            }

            for (int i = TV_HEIGHT - 1; i >= 0; i--) {
                // Convert the image.
                spi_tv_draw_image_cb_convert_grayscale(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&dst_img, i),
                                                       dst_img.data + (PICLINE_LENGTH_BYTES * i));
            }
        }

        #ifdef __DCACHE_PRESENT
        // Flush data for DMA
        SCB_CleanDCache_by_Addr((uint32_t *) dst_img.data, image_size(&dst_img));
        #endif

        // Update head which means a new image is ready.
        framebuffer_head = new_framebuffer_head;

        // Kick off an update of the display.
        if (spi_tx_cb_state == SPI_TX_CB_IDLE) {
            spi_tx_cb_state = SPI_TX_CB_MEMORY_WRITE_CMD;
            spi_tv_callback(&spi_bus, NULL, NULL);
        }
    }
}
#endif

static mp_obj_t py_tv_deinit() {
    switch (tv_type) {
        #ifdef OMV_SPI_DISPLAY_CONTROLLER
        case TV_SHIELD: {
            spi_config_deinit();
            break;
        }
        #endif
        default:
            break;
    }

    tv_type = TV_NONE;
    tv_triple_buffer = false;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_deinit_obj, py_tv_deinit);

static mp_obj_t py_tv_init(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_type, ARG_triple_buffer };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_type, MP_ARG_INT, {.u_int = TV_SHIELD } },
        { MP_QSTR_triple_buffer, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = TV_TRIPLE_BUFFER_DEFAULT } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    py_tv_deinit();
    tv_type = args[ARG_type].u_int;
    tv_triple_buffer = args[ARG_triple_buffer].u_bool;

    switch (tv_type) {
        #ifdef OMV_SPI_DISPLAY_CONTROLLER
        case TV_SHIELD:
            spi_config_init(tv_triple_buffer);
            break;
        #endif
        default:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to detect a supported TV controller."));
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tv_init_obj, 0, py_tv_init);

static mp_obj_t py_tv_width() {
    if (tv_type != TV_NONE) {
        return mp_obj_new_int(TV_WIDTH);
    }

    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_width_obj, py_tv_width);

static mp_obj_t py_tv_height() {
    if (tv_type != TV_NONE) {
        return mp_obj_new_int(TV_HEIGHT);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_height_obj, py_tv_height);

static mp_obj_t py_tv_type() {
    if (tv_type != TV_NONE) {
        return mp_obj_new_int(tv_type);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_type_obj, py_tv_type);

static mp_obj_t py_tv_triple_buffer() {
    if (tv_type != TV_NONE) {
        return mp_obj_new_int(tv_triple_buffer);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_triple_buffer_obj, py_tv_triple_buffer);

static mp_obj_t py_tv_refresh() {
    if (tv_type != TV_NONE) {
        return mp_obj_new_int(TV_REFRESH);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_refresh_obj, py_tv_refresh);

static mp_obj_t py_tv_channel(uint n_args, const mp_obj_t *args) {
    if (tv_type == TV_NONE) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
    }

    #ifdef OMV_SPI_DISPLAY_CONTROLLER
    if (tv_triple_buffer) {
        omv_spi_transfer_abort(&spi_bus);
        spi_tx_cb_state = SPI_TX_CB_IDLE;
        omv_gpio_write(OMV_SPI_DISPLAY_SSEL_PIN, 1);
    }

    if (n_args) {
        int channel = mp_obj_get_int(*args);

        if ((channel < 1) || (8 < channel)) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Channel ranges between 1 and 8"));
        }

        SpiRamWriteByteRegister(WRITE_GPIO, 0x70 | (channel - 1));
        return mp_const_none;
    } else {
        #ifdef OMV_SPI_DISPLAY_RX_CLK_DIV
        omv_spi_set_baudrate(&spi_bus, TV_BAUDRATE / OMV_SPI_DISPLAY_RX_CLK_DIV);
        #endif
        int channel = SpiRamReadByteRegister(READ_GPIO);
        #ifdef OMV_SPI_DISPLAY_RX_CLK_DIV
        omv_spi_set_baudrate(&spi_bus, TV_BAUDRATE);
        #endif
        return mp_obj_new_int((channel & 0x7) + 1);
    }
    #endif
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(py_tv_channel_obj, 0, 1, py_tv_channel);

static mp_obj_t py_tv_display(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {
        ARG_x, ARG_y, ARG_x_scale, ARG_y_scale, ARG_roi, ARG_channel, ARG_alpha,
        ARG_color_palette, ARG_alpha_palette, ARG_hint
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_x_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_y_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_roi, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_rgb_channel, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
        { MP_QSTR_alpha, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 255 } },
        { MP_QSTR_color_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_alpha_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_hint, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    image_t *image = py_helper_arg_to_image(pos_args[0], 0);
    rectangle_t roi = py_helper_arg_to_roi(args[ARG_roi].u_obj, image);

    if (args[ARG_channel].u_int < -1 || args[ARG_channel].u_int > 2) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("RGB channel can be 0, 1, or 2"));
    }

    if (args[ARG_alpha].u_int < 0 || args[ARG_alpha].u_int > 255) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Alpha ranges between 0 and 255"));
    }

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    py_helper_arg_to_scale(args[ARG_x_scale].u_obj, args[ARG_y_scale].u_obj, &x_scale, &y_scale);

    const uint16_t *color_palette = py_helper_arg_to_palette(args[ARG_color_palette].u_obj, PIXFORMAT_RGB565);
    const uint8_t *alpha_palette = py_helper_arg_to_palette(args[ARG_alpha_palette].u_obj, PIXFORMAT_GRAYSCALE);

    switch (tv_type) {
        #ifdef OMV_SPI_DISPLAY_CONTROLLER
        case TV_SHIELD: {
            fb_alloc_mark();
            spi_tv_display(image, args[ARG_x].u_int, args[ARG_y].u_int, x_scale, y_scale, &roi,
                           args[ARG_channel].u_int, args[ARG_alpha].u_int, color_palette, alpha_palette,
                           args[ARG_hint].u_int);
            fb_alloc_free_till_mark();
            break;
        }
        #endif
        default:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tv_display_obj, 1, py_tv_display);

static mp_obj_t py_tv_clear() {
    switch (tv_type) {
        #ifdef OMV_SPI_DISPLAY_CONTROLLER
        case TV_SHIELD: {
            fb_alloc_mark();
            spi_tv_display(NULL, 0, 0, 1.f, 1.f, NULL,
                           0, 0, NULL, NULL, 0);
            fb_alloc_free_till_mark();
            break;
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("TV controller is not initialized"));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tv_clear_obj, py_tv_clear);

static const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_tv)          },
    { MP_ROM_QSTR(MP_QSTR_TV_NONE),         MP_ROM_INT(TV_NONE)                  },
    { MP_ROM_QSTR(MP_QSTR_TV_SHIELD),       MP_ROM_INT(TV_SHIELD)                },
    { MP_ROM_QSTR(MP_QSTR_init),            MP_ROM_PTR(&py_tv_init_obj)          },
    { MP_ROM_QSTR(MP_QSTR_deinit),          MP_ROM_PTR(&py_tv_deinit_obj)        },
    { MP_ROM_QSTR(MP_QSTR_width),           MP_ROM_PTR(&py_tv_width_obj)         },
    { MP_ROM_QSTR(MP_QSTR_height),          MP_ROM_PTR(&py_tv_height_obj)        },
    { MP_ROM_QSTR(MP_QSTR_type),            MP_ROM_PTR(&py_tv_type_obj)          },
    { MP_ROM_QSTR(MP_QSTR_triple_buffer),   MP_ROM_PTR(&py_tv_triple_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_refresh),         MP_ROM_PTR(&py_tv_refresh_obj)       },
    { MP_ROM_QSTR(MP_QSTR_channel),         MP_ROM_PTR(&py_tv_channel_obj)       },
    { MP_ROM_QSTR(MP_QSTR_display),         MP_ROM_PTR(&py_tv_display_obj)       },
    { MP_ROM_QSTR(MP_QSTR_clear),           MP_ROM_PTR(&py_tv_clear_obj)         },
};

static MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t tv_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

void py_tv_init0() {
    py_tv_deinit();
}

MP_REGISTER_MODULE(MP_QSTR_tv, tv_module);

#endif // MICROPY_PY_TV
