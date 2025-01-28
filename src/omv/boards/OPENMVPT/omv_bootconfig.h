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
 * Board configuration and pin definitions.
 */
// *INDENT-OFF*
#ifndef __OMV_BOOTCONFIG_H__
#define __OMV_BOOTCONFIG_H__
#include STM32_HAL_H
#include "port.h"

// Misc config.
#define OMV_BOOT_VID                (0x37C5)
#define OMV_BOOT_PID                (0x9205)
#define OMV_BOOT_DFU_TIMEOUT        (1500)
#define OMV_BOOT_LED_PIN            (0) // index in omv_boot_pins
#define OMV_BOOT_MAGIC_ADDR         (0x2001FFFCU)

// Flash config.
#define OMV_BOOT_AXI_FLASH_ENABLE   (1)
#define OMV_BOOT_SPI_FLASH_ENABLE   (1)
#define OMV_BOOT_QSPI_FLASH_SIZE    (0x2000000)  // Must always be a power of 2.

// Boot I/O pins.
static const pin_t omv_boot_pins[] = {
  { .gpio = GPIOC, .pin = GPIO_PIN_1,  .speed = GPIO_SPEED_LOW,  .mode = GPIO_MODE_OUTPUT_PP, .pull = GPIO_PULLUP  }, // LED
  { .gpio = GPIOA, .pin = GPIO_PIN_9,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_INPUT,  .pull = GPIO_NOPULL     }, // VBUS
  { .gpio = GPIOA, .pin = GPIO_PIN_11, .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF10_OTG_FS  }, // DM
  { .gpio = GPIOA, .pin = GPIO_PIN_12, .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF10_OTG_FS  }, // DP
  { .gpio = GPIOF, .pin = GPIO_PIN_10, .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF9_QUADSPI  }, // CLK
  { .gpio = GPIOG, .pin = GPIO_PIN_6,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF10_QUADSPI }, // CS
  { .gpio = GPIOF, .pin = GPIO_PIN_8,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF10_QUADSPI }, // D0
  { .gpio = GPIOF, .pin = GPIO_PIN_9,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF10_QUADSPI }, // D1
  { .gpio = GPIOF, .pin = GPIO_PIN_7,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF9_QUADSPI  }, // D2
  { .gpio = GPIOF, .pin = GPIO_PIN_6,  .speed = GPIO_SPEED_HIGH, .mode = GPIO_MODE_AF_PP, .alt = GPIO_AF9_QUADSPI  }, // D3
};
#define OMV_BOOT_PINS_COUNT         (sizeof(omv_boot_pins) / sizeof(omv_boot_pins[0]))

// Boot partitions.
static const partition_t OMV_BOOT_DFU_PARTITIONS[] = {
  { .type = PTYPE_AXI_FLASH, .region =  -1, .rdonly = 1, .start = 0x08000000, .limit = 0x08020000, .attr = 0 }, // Boot
  { .type = PTYPE_AXI_FLASH, .region =  -1, .rdonly = 0, .start = 0x08020000, .limit = 0x08040000, .attr = 0 }, // FFS
  { .type = PTYPE_AXI_FLASH, .region =  -1, .rdonly = 0, .start = 0x08040000, .limit = 0x08200000, .attr = 0 }, // FIRMWARE
  { .type = PTYPE_SPI_FLASH, .region =  -1, .rdonly = 0, .start = 0x00000000, .limit = 0x02000000, .attr = 0 }, // QSPI
  { .type = PTYPE_SPI_FLASH, .region =  -1, .rdonly = 0, .start = 0x01800000, .limit = 0x02000000, .attr = 0 }, // ROMFS
};
#define OMV_BOOT_DFU_PARTITIONS_COUNT   5 // Must be a literal
#define OMV_BOOT_DFU_PARTITIONS_STR     "BOOTLOADER", "FILESYSTEM", "FIRMWARE", "SPI_FLASH", "ROMFS0"
#endif //__OMV_BOOTCONFIG_H__
