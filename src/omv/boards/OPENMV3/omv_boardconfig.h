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
#ifndef __OMV_BOARDCONFIG_H__
#define __OMV_BOARDCONFIG_H__

// Architecture info
#define OMV_BOARD_ARCH                        "OMV3 F7 512" // 33 chars max
#define OMV_BOARD_TYPE                        "M7"
#define OMV_BOARD_UID_ADDR                    0x1FF0F420    // Unique ID address.
#define OMV_BOARD_UID_SIZE                    3             // Unique ID size in words.
#define OMV_BOARD_UID_OFFSET                  4             // Bytes offset for multi-word UIDs.

// JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE                 (0)
#define OMV_JPEG_QUALITY_LOW                  (35)
#define OMV_JPEG_QUALITY_HIGH                 (60)
#define OMV_JPEG_QUALITY_THRESHOLD            (160 * 120 * 2)

// GPU Configuration
#define OMV_GPU_ENABLE                        (1)

// CSI drivers configuration.
#define OMV_OV7725_ENABLE                     (1)
#define OMV_OV7725_PLL_CONFIG                 (0x81) // x6
#define OMV_OV7725_BANDING                    (0x8F)

// FIR drivers configuration.
#define OMV_FIR_MLX90621_ENABLE               (1)
#define OMV_FIR_MLX90640_ENABLE               (1)
#define OMV_FIR_MLX90641_ENABLE               (1)
#define OMV_FIR_AMG8833_ENABLE                (1)
#define OMV_FIR_LEPTON_ENABLE                 (1)

// UMM heap block size
#define OMV_UMM_BLOCK_SIZE                    16

// USB IRQn.
#define OMV_USB_IRQN                          (OTG_FS_IRQn)

//PLL1 216MHz/48MHz
#define OMV_OSC_PLL1M                         (12)
#define OMV_OSC_PLL1N                         (432)
#define OMV_OSC_PLL1P                         (2)
#define OMV_OSC_PLL1Q                         (9)
#define OMV_OSC_PLL1R                         (2)

// HSE/HSI/CSI State
#define OMV_OSC_HSE_STATE                     (RCC_HSE_ON)

// Clock Sources
#define OMV_OSC_PLL_CLKSOURCE                 (RCC_PLLSOURCE_HSE)

// Flash Latency
#define OMV_FLASH_LATENCY                     (FLASH_LATENCY_7)

// Linker script constants (see the linker script template stm32.ld.S).
// Note: fb_alloc is a stack-based, dynamically allocated memory on FB.
// The maximum available fb_alloc memory = FB_ALLOC_SIZE + FB_SIZE - (w*h*bpp).
#define OMV_MAIN_MEMORY                       DTCM // Data/BSS memory
#define OMV_STACK_MEMORY                      ITCM // stack memory
#define OMV_STACK_SIZE                        (16K)
#define OMV_FB_MEMORY                         SRAM1 // Framebuffer, fb_alloc
#define OMV_FB_SIZE                           (300K) // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE                     (84K) // minimum fb alloc size
#define OMV_DMA_MEMORY                        DTCM // Misc DMA buffers
#define OMV_GC_BLOCK0_MEMORY                  DTCM // Main GC block
#define OMV_GC_BLOCK0_SIZE                    (53K)
#define OMV_JPEG_SIZE                         (22K) // IDE JPEG buffer (header + data).
#define OMV_MSC_BUF_SIZE                      (2K) // USB MSC bot data
#define OMV_FFS_BUF_SIZE                      (32K) // Flash filesystem cache
#define OMV_LINE_BUF_SIZE                     (3 * 1024) // Image line buffer round(640 * 2BPP * 2 buffers).

// Memory map.
#define OMV_FLASH_ORIGIN                      0x08000000
#define OMV_FLASH_LENGTH                      2048K
#define OMV_DTCM_ORIGIN                       0x20000000
#define OMV_DTCM_LENGTH                       128K // Note DTCM/ITCM memory is not cacheable on M7
#define OMV_ITCM_ORIGIN                       0x00000000
#define OMV_ITCM_LENGTH                       16K
#define OMV_SRAM1_ORIGIN                      0x20020000
#define OMV_SRAM1_LENGTH                      384K

// Flash configuration.
#define OMV_FLASH_BOOT_ORIGIN                 0x08000000
#define OMV_FLASH_BOOT_LENGTH                 32K
#define OMV_FLASH_FFS_ORIGIN                  0x08008000
#define OMV_FLASH_FFS_LENGTH                  96K
#define OMV_FLASH_TXT_ORIGIN                  0x08020000
#define OMV_FLASH_TXT_LENGTH                  1664K

// ROMFS configuration.
#define OMV_ROMFS_PART0_ORIGIN                0x081C0000
#define OMV_ROMFS_PART0_LENGTH                256K

// CSI I2C bus
#define OMV_CSI_I2C_ID                        (1)
#define OMV_CSI_I2C_SPEED                     (OMV_I2C_SPEED_STANDARD)

// FIR I2C bus
#define OMV_FIR_I2C_ID                        (2)
#define OMV_FIR_I2C_SPEED                     (OMV_I2C_SPEED_FULL)

// Soft I2C bus
#define OMV_SOFT_I2C_SIOC_PIN                 (&omv_pin_B10_GPIO)
#define OMV_SOFT_I2C_SIOD_PIN                 (&omv_pin_B11_GPIO)
#define OMV_SOFT_I2C_SPIN_DELAY               24

// WINC SPI bus
#define OMV_WINC_SPI_ID                       (2)
#define OMV_WINC_SPI_BAUDRATE                 (27000000)
#define OMV_WINC_EN_PIN                       (&omv_pin_A5_GPIO)
#define OMV_WINC_RST_PIN                      (&omv_pin_D12_GPIO)
#define OMV_WINC_IRQ_PIN                      (&omv_pin_D13_GPIO)

// Camera Interface
#define OMV_CSI_CLK_SOURCE                    (OMV_CSI_CLK_SOURCE_TIM)
#define OMV_CSI_CLK_FREQUENCY                 (9000000)
#define OMV_CSI_TIM                           (TIM1)
#define OMV_CSI_TIM_PIN                       (&omv_pin_A8_TIM1)
#define OMV_CSI_TIM_CHANNEL                   (TIM_CHANNEL_1)
#define OMV_CSI_TIM_CLK_ENABLE()              __TIM1_CLK_ENABLE()
#define OMV_CSI_TIM_CLK_DISABLE()             __TIM1_CLK_DISABLE()
#define OMV_CSI_TIM_PCLK_FREQ()               HAL_RCC_GetPCLK2Freq()
#define OMV_CSI_HW_CROP_ENABLE                (1)

#define OMV_CSI_D0_PIN                        (&omv_pin_C6_DCMI)
#define OMV_CSI_D1_PIN                        (&omv_pin_C7_DCMI)
#define OMV_CSI_D2_PIN                        (&omv_pin_E0_DCMI)
#define OMV_CSI_D3_PIN                        (&omv_pin_E1_DCMI)
#define OMV_CSI_D4_PIN                        (&omv_pin_E4_DCMI)
#define OMV_CSI_D5_PIN                        (&omv_pin_B6_DCMI)
#define OMV_CSI_D6_PIN                        (&omv_pin_E5_DCMI)
#define OMV_CSI_D7_PIN                        (&omv_pin_E6_DCMI)

#define OMV_CSI_HSYNC_PIN                     (&omv_pin_A4_DCMI)
#define OMV_CSI_VSYNC_PIN                     (&omv_pin_B7_DCMI)
#define OMV_CSI_PXCLK_PIN                     (&omv_pin_A6_DCMI)
#define OMV_CSI_RESET_PIN                     (&omv_pin_A10_GPIO)
#define OMV_CSI_POWER_PIN                     (&omv_pin_B5_GPIO)

// Physical I2C buses.

// I2C bus 1
#define OMV_I2C1_ID                           (1)
#define OMV_I2C1_SCL_PIN                      (&omv_pin_B8_I2C1)
#define OMV_I2C1_SDA_PIN                      (&omv_pin_B9_I2C1)

// I2C bus 2
#define OMV_I2C2_ID                           (2)
#define OMV_I2C2_SCL_PIN                      (&omv_pin_B10_I2C2)
#define OMV_I2C2_SDA_PIN                      (&omv_pin_B11_I2C2)

// Physical SPI buses.

// SPI bus 2
#define OMV_SPI2_ID                           (2)
#define OMV_SPI2_SCLK_PIN                     (&omv_pin_B13_SPI2)
#define OMV_SPI2_MISO_PIN                     (&omv_pin_B14_SPI2)
#define OMV_SPI2_MOSI_PIN                     (&omv_pin_B15_SPI2)
#define OMV_SPI2_SSEL_PIN                     (&omv_pin_B12_SPI2)
#define OMV_SPI2_DMA_TX_CHANNEL               (DMA1_Stream4)
#define OMV_SPI2_DMA_TX_REQUEST               (DMA_CHANNEL_0)
#define OMV_SPI2_DMA_RX_CHANNEL               (DMA1_Stream3)
#define OMV_SPI2_DMA_RX_REQUEST               (DMA_CHANNEL_0)

// SPI LCD Interface
#define OMV_SPI_DISPLAY_CONTROLLER            (OMV_SPI2_ID)
#define OMV_SPI_DISPLAY_MOSI_PIN              (&omv_pin_B15_SPI2)
#define OMV_SPI_DISPLAY_MISO_PIN              (&omv_pin_B14_SPI2)
#define OMV_SPI_DISPLAY_SCLK_PIN              (&omv_pin_B13_SPI2)
#define OMV_SPI_DISPLAY_SSEL_PIN              (&omv_pin_B12_GPIO)

#define OMV_SPI_DISPLAY_RS_PIN                (&omv_pin_D13_GPIO)
#define OMV_SPI_DISPLAY_RST_PIN               (&omv_pin_D12_GPIO)
#define OMV_SPI_DISPLAY_BL_PIN                (&omv_pin_A5_GPIO)

// FIR Lepton
#define OMV_FIR_LEPTON_I2C_BUS                (OMV_FIR_I2C_ID)
#define OMV_FIR_LEPTON_I2C_BUS_SPEED          (OMV_FIR_I2C_SPEED)

#define OMV_FIR_LEPTON_SPI_BUS                (OMV_SPI2_ID)
#define OMV_FIR_LEPTON_MOSI_PIN               (&omv_pin_B15_SPI2)
#define OMV_FIR_LEPTON_MISO_PIN               (&omv_pin_B14_SPI2)
#define OMV_FIR_LEPTON_SCLK_PIN               (&omv_pin_B13_SPI2)
#define OMV_FIR_LEPTON_SSEL_PIN               (&omv_pin_B12_GPIO)

#endif //__OMV_BOARDCONFIG_H__
