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

// Board info
#define OMV_BOARD_ARCH                      "GIGA H7 8192 SDRAM"    // 33 chars max
#define OMV_BOARD_TYPE                      "H7"
#define OMV_BOARD_UID_ADDR                  0x1FF1E800  // Unique ID address.
#define OMV_BOARD_UID_SIZE                  3           // Unique ID size in words.
#define OMV_BOARD_UID_OFFSET                4           // Bytes offset for multi-word UIDs.

// JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE               (1)
#define OMV_JPEG_QUALITY_LOW                50
#define OMV_JPEG_QUALITY_HIGH               90
#define OMV_JPEG_QUALITY_THRESHOLD          (320 * 240 * 2)

// GPU Configuration
#define OMV_GPU_ENABLE                      (1)

// CSI drivers configuration.
#define OMV_OV5640_ENABLE                   (1)
#define OMV_OV5640_AF_ENABLE                (1)
#define OMV_OV5640_PLL_CTRL2                (0x90)
#define OMV_OV5640_PLL_CTRL3                (0x13)
#define OMV_OV5640_CLK_FREQ                 (12000000)

#define OMV_OV7670_ENABLE                   (1)
#define OMV_OV7670_VERSION                  (75)
#define OMV_OV7670_CLKRC                    (0)

#define OMV_GC2145_ENABLE                   (1)
#define OMV_GC2145_ROTATE                   (1)

#define OMV_HM01B0_ENABLE                   (1)
#define OMV_HM0360_ENABLE                   (1)
#define OMV_HM0360_CLK_FREQ                 (24000000)
#define OMV_HM0360_PLL1_CONFIG              (0x04)

// FIR drivers configuration.
#define OMV_FIR_MLX90621_ENABLE             (1)
#define OMV_FIR_MLX90640_ENABLE             (1)
#define OMV_FIR_MLX90641_ENABLE             (1)
#define OMV_FIR_AMG8833_ENABLE              (1)
#define OMV_FIR_LEPTON_ENABLE               (0)

// UMM heap block size
#define OMV_UMM_BLOCK_SIZE                  256

// USB IRQn.
#define OMV_USB_IRQN                        (OTG_FS_IRQn)
#define OMV_USB_ULPI                        (0)

// PLL1 480MHz/48MHz SDMMC and FDCAN
// USB and RNG are clocked from the HSI48
#define OMV_OSC_PLL1M                       (4)
#define OMV_OSC_PLL1N                       (240)
#define OMV_OSC_PLL1P                       (2)
#define OMV_OSC_PLL1Q                       (20)
#define OMV_OSC_PLL1R                       (2)
#define OMV_OSC_PLL1VCI                     (RCC_PLL1VCIRANGE_2)
#define OMV_OSC_PLL1VCO                     (RCC_PLL1VCOWIDE)
#define OMV_OSC_PLL1FRAC                    (0)

// PLL2 200MHz for FMC and QSPI.
#define OMV_OSC_PLL2M                       (4)
#define OMV_OSC_PLL2N                       (100)
#define OMV_OSC_PLL2P                       (2)
#define OMV_OSC_PLL2Q                       (2)
#define OMV_OSC_PLL2R                       (2)
#define OMV_OSC_PLL2VCI                     (RCC_PLL2VCIRANGE_2)
#define OMV_OSC_PLL2VCO                     (RCC_PLL2VCOWIDE)
#define OMV_OSC_PLL2FRAC                    (0)

// PLL3 160MHz for ADC and SPI123
#define OMV_OSC_PLL3M                       (4)
#define OMV_OSC_PLL3N                       (80)
#define OMV_OSC_PLL3P                       (2)
#define OMV_OSC_PLL3Q                       (2)
#define OMV_OSC_PLL3R                       (2)
#define OMV_OSC_PLL3VCI                     (RCC_PLL3VCIRANGE_2)
#define OMV_OSC_PLL3VCO                     (RCC_PLL3VCOWIDE)
#define OMV_OSC_PLL3FRAC                    (0)

// Clock Sources
#define OMV_OSC_PLL_CLKSOURCE               RCC_PLLSOURCE_HSE
#define OMV_OSC_USB_CLKSOURCE               RCC_USBCLKSOURCE_HSI48
#define OMV_OSC_RNG_CLKSOURCE               RCC_RNGCLKSOURCE_HSI48
#define OMV_OSC_ADC_CLKSOURCE               RCC_ADCCLKSOURCE_PLL3
#define OMV_OSC_SPI123_CLKSOURCE            RCC_SPI123CLKSOURCE_PLL3
#define OMV_OSC_DFSDM1_CLKSOURCE            RCC_DFSDM1CLKSOURCE_D2PCLK1

// HSE/HSI/CSI State
#define OMV_OSC_HSE_STATE                   (RCC_HSE_ON)
#define OMV_OSC_HSI48_STATE                 (RCC_HSI48_ON)

// Flash Latency
#define OMV_FLASH_LATENCY                   (FLASH_LATENCY_2)

// Power supply configuration
#define OMV_PWR_SUPPLY                      (PWR_LDO_SUPPLY)

// Linker script constants (see the linker script template stm32.ld.S).
// Note: fb_alloc is a stack-based, dynamically allocated memory on FB.
// The maximum available fb_alloc memory = FB_ALLOC_SIZE + FB_SIZE - (w*h*bpp).
#define OMV_MAIN_MEMORY                     SRAM0   // Data, BSS memory.
#define OMV_STACK_MEMORY                    DTCM    // stack memory
#define OMV_STACK_SIZE                      (64K)
#define OMV_FB_MEMORY                       DRAM    // Framebuffer, fb_alloc
#define OMV_FB_SIZE                         (3M)    // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE                   (1M)    // minimum fb alloc size
#define OMV_FB_OVERLAY_MEMORY               SRAM0   // Fast fb_alloc memory.
#define OMV_FB_OVERLAY_SIZE                 (448K)  // Fast fb_alloc memory size.
#define OMV_JPEG_MEMORY                     DRAM    // JPEG buffer memory buffer.
#define OMV_JPEG_SIZE                       (1M)    // IDE JPEG buffer (header + data).
#define OMV_VOSPI_MEMORY                    DTCM    // VoSPI buffer memory.
#define OMV_VOSPI_SIZE                      (38K)
#define OMV_DMA_MEMORY                      SRAM3   // Misc DMA buffers memory.
#define OMV_DMA_MEMORY_D1                   SRAM0   // Domain 1 DMA buffers.
#define OMV_DMA_MEMORY_D2                   SRAM3   // Domain 2 DMA buffers.
#define OMV_OPENAMP_MEMORY                  SRAM4
#define OMV_OPENAMP_SIZE                    (64K)
#define OMV_CORE1_MEMORY                    DRAM
#define OMV_CORE1_SIZE                      (512K)
#define OMV_GC_BLOCK0_MEMORY                SRAM1   // Main GC block.
#define OMV_GC_BLOCK0_SIZE                  (256K)
#define OMV_GC_BLOCK1_MEMORY                DRAM    // Extra GC block 1.
#define OMV_GC_BLOCK1_SIZE                  (2560K)
#define OMV_MSC_BUF_SIZE                    (2K)    // USB MSC bot data
#define OMV_SDRAM_SIZE                      (8 * 1024 * 1024)   // This needs to be here for UVC firmware.
#define OMV_LINE_BUF_SIZE                   (11 * 1024) // Image line buffer round(2592 * 2BPP * 2 buffers).

// Memory map.
#define OMV_FLASH_ORIGIN                    0x08000000
#define OMV_FLASH_LENGTH                    2048K
#define OMV_DTCM_ORIGIN                     0x20000000    // Note accessible by CPU and MDMA only.
#define OMV_DTCM_LENGTH                     128K
#define OMV_SRAM0_ORIGIN                    0x24000000
#define OMV_SRAM0_LENGTH                    512K
#define OMV_SRAM1_ORIGIN                    0x30000000
#define OMV_SRAM1_LENGTH                    256K
#define OMV_SRAM3_ORIGIN                    0x30040000
#define OMV_SRAM3_LENGTH                    32K
#define OMV_SRAM4_ORIGIN                    0x38000000
#define OMV_SRAM4_LENGTH                    64K
#define OMV_DRAM_ORIGIN                     0x60000000
#define OMV_DRAM_LENGTH                     8M

// Flash configuration.
#define OMV_FLASH_FFS_ORIGIN                0x08020000
#define OMV_FLASH_FFS_LENGTH                128K
#define OMV_FLASH_TXT_ORIGIN                0x08040000
#define OMV_FLASH_TXT_LENGTH                1792K
#define OMV_FLASH_EXT_ORIGIN                0x90000000
#define OMV_FLASH_EXT_LENGTH                16M

// ROMFS configuration.
#define OMV_ROMFS_PART0_ORIGIN              0x90B00000
#define OMV_ROMFS_PART0_LENGTH              4M

// MDMA configuration
#define OMV_MDMA_CHANNEL_DCMI_0             (0)
#define OMV_MDMA_CHANNEL_DCMI_1             (1)
#define OMV_MDMA_CHANNEL_JPEG_IN            (7) // in has a lower pri than out
#define OMV_MDMA_CHANNEL_JPEG_OUT           (6) // out has a higher pri than in

// AXI QoS - Low-High (0:15) - default 0
#define OMV_AXI_QOS_MDMA_R_PRI              15
#define OMV_AXI_QOS_MDMA_W_PRI              15
#define OMV_AXI_QOS_LTDC_R_PRI              14

// Enable additional GPIO ports.
#define OMV_GPIO_PORT_F_ENABLE              (1)
#define OMV_GPIO_PORT_G_ENABLE              (1)
#define OMV_GPIO_PORT_H_ENABLE              (1)
#define OMV_GPIO_PORT_I_ENABLE              (1)
#define OMV_GPIO_PORT_J_ENABLE              (1)
#define OMV_GPIO_PORT_K_ENABLE              (1)

// CSI I2C
#define OMV_CSI_I2C_ID                      (4)
#define OMV_CSI_I2C_SPEED                   (OMV_I2C_SPEED_STANDARD)

// FIR I2C
#define OMV_FIR_I2C_ID                      (1)
#define OMV_FIR_I2C_SPEED                   (OMV_I2C_SPEED_STANDARD)

// Camera Interface
#define OMV_CSI_CLK_SOURCE                  (OMV_CSI_CLK_SOURCE_TIM)
#define OMV_CSI_CLK_FREQUENCY               (12000000)
#define OMV_CSI_TIM                         (TIM1)
#define OMV_CSI_TIM_PIN                     (&omv_pin_J9_TIM1)
#define OMV_CSI_TIM_CHANNEL                 (TIM_CHANNEL_3)
#define OMV_CSI_TIM_CLK_ENABLE()            __TIM1_CLK_ENABLE()
#define OMV_CSI_TIM_CLK_DISABLE()           __TIM1_CLK_DISABLE()
#define OMV_CSI_TIM_PCLK_FREQ()             HAL_RCC_GetPCLK2Freq()
#define OMV_CSI_DMA_MEMCPY_ENABLE           (1)
#define OMV_CSI_HW_CROP_ENABLE              (1)

#define OMV_CSI_D0_PIN                      (&omv_pin_H9_DCMI)
#define OMV_CSI_D1_PIN                      (&omv_pin_H10_DCMI)
#define OMV_CSI_D2_PIN                      (&omv_pin_H11_DCMI)
#define OMV_CSI_D3_PIN                      (&omv_pin_G11_DCMI)
#define OMV_CSI_D4_PIN                      (&omv_pin_H14_DCMI)
#define OMV_CSI_D5_PIN                      (&omv_pin_I4_DCMI)
#define OMV_CSI_D6_PIN                      (&omv_pin_I6_DCMI)
#define OMV_CSI_D7_PIN                      (&omv_pin_I7_DCMI)

#define OMV_CSI_HSYNC_PIN                   (&omv_pin_H8_DCMI)
#define OMV_CSI_VSYNC_PIN                   (&omv_pin_I5_DCMI)
#define OMV_CSI_PXCLK_PIN                   (&omv_pin_A6_DCMI)
#define OMV_CSI_RESET_PIN                   (&omv_pin_A1_GPIO)
#define OMV_CSI_POWER_PIN                   (&omv_pin_D4_GPIO)

// Physical I2C buses.
// I2C bus 1
#define OMV_I2C1_ID                         (1)
#define OMV_I2C1_SCL_PIN                    (&omv_pin_B8_I2C1)
#define OMV_I2C1_SDA_PIN                    (&omv_pin_B9_I2C1)

// I2C bus 4
#define OMV_I2C4_ID                         (4)
#define OMV_I2C4_SCL_PIN                    (&omv_pin_B6_I2C4)
#define OMV_I2C4_SDA_PIN                    (&omv_pin_H12_I2C4)

// DFSDM1
#define OMV_DFSDM                           (DFSDM1_Channel0)
#define OMV_DFSDM_CHANNEL                   (DFSDM_CHANNEL_0)
// DFSDM output clock is derived from the Aclk (set in SAI1SEL[2:0])
// for SAI1 and DFSDM1, which is clocked from PLL1Q by default (48MHz).
#define OMV_DFSDM_FREQMHZ                   (48)
#define OMV_AUDIO_MAX_CHANNELS              (1) // Maximum number of channels.

#define OMV_DFSDM_CK_PIN                    (&omv_pin_D3_DFSDM1)
#define OMV_DFSDM_D1_PIN                    (&omv_pin_C1_DFSDM1)

#define OMV_DFSDM_FLT0                      DFSDM1_Filter0
#define OMV_DFSDM_FLT0_IRQ                  DFSDM1_FLT0_IRQn
#define OMV_DFSDM_FLT0_IRQHandler           DFSDM1_FLT0_IRQHandler
#define OMV_DFSDM_FLT0_DMA_STREAM           DMA1_Stream1
#define OMV_DFSDM_FLT0_DMA_REQUEST          DMA_REQUEST_DFSDM1_FLT0
#define OMV_DFSDM_FLT0_DMA_IRQ              DMA1_Stream1_IRQn
#define OMV_DFSDM_FLT0_DMA_IRQHandler       DMA1_Stream1_IRQHandler

#define OMV_DFSDM_CLK_ENABLE()              __HAL_RCC_DFSDM1_CLK_ENABLE()
#define OMV_DFSDM_CLK_DISABLE()             __HAL_RCC_DFSDM1_CLK_DISABLE()
#define OMV_DFSDM_DMA_CLK_ENABLE()          __HAL_RCC_DMA1_CLK_ENABLE()

// LCD Interface
#define OMV_RGB_DISPLAY_CONTROLLER          (LTDC)
#define OMV_RGB_DISPLAY_BL_PIN              (&omv_pin_B12_GPIO)
#define OMV_RGB_DISPLAY_CLK_ENABLE()        __HAL_RCC_LTDC_CLK_ENABLE()
#define OMV_RGB_DISPLAY_CLK_DISABLE()       __HAL_RCC_LTDC_CLK_DISABLE()
#define OMV_RGB_DISPLAY_FORCE_RESET()       __HAL_RCC_LTDC_FORCE_RESET()
#define OMV_RGB_DISPLAY_RELEASE_RESET()     __HAL_RCC_LTDC_RELEASE_RESET()

// DSI Interface
#define OMV_DISPLAY_ST7701_ENABLE           (1)
#define OMV_DSI_DISPLAY_CONTROLLER          (DSI)
#define OMV_DSI_DISPLAY_CONTROLLER_INIT     st7701_init
#define OMV_DSI_DISPLAY_TE_ENABLE           (1)
#define OMV_DSI_DISPLAY_BL_PIN              (&omv_pin_B12_GPIO)
#define OMV_DSI_DISPLAY_CLK_ENABLE()        __HAL_RCC_DSI_CLK_ENABLE()
#define OMV_DSI_DISPLAY_CLK_DISABLE()       __HAL_RCC_DSI_CLK_DISABLE()
#define OMV_DSI_DISPLAY_FORCE_RESET()       __HAL_RCC_DSI_FORCE_RESET()
#define OMV_DSI_DISPLAY_RELEASE_RESET()     __HAL_RCC_DSI_RELEASE_RESET()

#endif //__OMV_BOARDCONFIG_H__
