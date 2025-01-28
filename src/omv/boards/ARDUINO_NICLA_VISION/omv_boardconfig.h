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
#define OMV_BOARD_ARCH                        "NICLAV H7 1024"  // 33 chars max
#define OMV_BOARD_TYPE                        "NICLAV"
#define OMV_BOARD_UID_ADDR                    0x1FF1E800    // Unique ID address.
#define OMV_BOARD_UID_SIZE                    3             // Unique ID size in words.
#define OMV_BOARD_UID_OFFSET                  4             // Bytes offset for multi-word UIDs.

// JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE                 (1)
#define OMV_JPEG_QUALITY_LOW                  (50)
#define OMV_JPEG_QUALITY_HIGH                 (90)
#define OMV_JPEG_QUALITY_THRESHOLD            (320 * 240 * 2)

// GPU Configuration
#define OMV_GPU_ENABLE                        (1)

// CSI drivers configuration.
#define OMV_GC2145_ENABLE                     (1)
#define OMV_GC2145_ROTATE                     (1)

// FIR drivers configuration.
#define OMV_FIR_MLX90621_ENABLE               (1)
#define OMV_FIR_MLX90640_ENABLE               (1)
#define OMV_FIR_MLX90641_ENABLE               (1)
#define OMV_FIR_AMG8833_ENABLE                (1)
#define OMV_FIR_LEPTON_ENABLE                 (1)

// UMM heap block size
#define OMV_UMM_BLOCK_SIZE                    16

// USB IRQn.
#define OMV_USB_IRQN                          (OTG_HS_IRQn)
#define OMV_USB_ULPI                          (1)
#define OMV_USB_ULPI_STP_PIN                  (&omv_pin_C0_OTG_HS)
#define OMV_USB_ULPI_DIR_PIN                  (&omv_pin_C2_OTG_HS)

// Defined for cpu frequency scaling to override the revid.
#define OMV_MAX_CPU_FREQ                      (400)

// PLL1 400MHz/40MHz for SDMMC and FDCAN
// USB and RNG are clocked from the HSI48
#define OMV_OSC_PLL1M                         (5)
#define OMV_OSC_PLL1N                         (160)
#define OMV_OSC_PLL1P                         (2)
#define OMV_OSC_PLL1Q                         (16)
#define OMV_OSC_PLL1R                         (2)
#define OMV_OSC_PLL1VCI                       (RCC_PLL1VCIRANGE_2)
#define OMV_OSC_PLL1VCO                       (RCC_PLL1VCOWIDE)
#define OMV_OSC_PLL1FRAC                      (0)

// PLL2 200MHz for FMC and QSPI.
#define OMV_OSC_PLL2M                         (5)
#define OMV_OSC_PLL2N                         (80)
#define OMV_OSC_PLL2P                         (2)
#define OMV_OSC_PLL2Q                         (2)
#define OMV_OSC_PLL2R                         (2)
#define OMV_OSC_PLL2VCI                       (RCC_PLL2VCIRANGE_2)
#define OMV_OSC_PLL2VCO                       (RCC_PLL2VCOWIDE)
#define OMV_OSC_PLL2FRAC                      (0)

// PLL3 160MHz for ADC and SPI123
#define OMV_OSC_PLL3M                         (5)
#define OMV_OSC_PLL3N                         (160)
#define OMV_OSC_PLL3P                         (2)
#define OMV_OSC_PLL3Q                         (5)
#define OMV_OSC_PLL3R                         (2)
#define OMV_OSC_PLL3VCI                       (RCC_PLL3VCIRANGE_2)
#define OMV_OSC_PLL3VCO                       (RCC_PLL3VCOWIDE)
#define OMV_OSC_PLL3FRAC                      (0)

// Clock Sources
#define OMV_OSC_PLL_CLKSOURCE                 RCC_PLLSOURCE_HSE
#define OMV_OSC_USB_CLKSOURCE                 RCC_USBCLKSOURCE_HSI48
#define OMV_OSC_RNG_CLKSOURCE                 RCC_RNGCLKSOURCE_HSI48
#define OMV_OSC_ADC_CLKSOURCE                 RCC_ADCCLKSOURCE_PLL3
#define OMV_OSC_SPI45_CLKSOURCE               RCC_SPI45CLKSOURCE_PLL3
#define OMV_OSC_SPI123_CLKSOURCE              RCC_SPI123CLKSOURCE_PLL2
#define OMV_OSC_DFSDM1_CLKSOURCE              RCC_DFSDM1CLKSOURCE_D2PCLK1

// HSE/HSI/CSI State
#define OMV_OSC_HSE_STATE                     (RCC_HSE_BYPASS)
#define OMV_OSC_HSI48_STATE                   (RCC_HSI48_ON)

// Flash Latency
#define OMV_FLASH_LATENCY                     (FLASH_LATENCY_2)

// Power supply configuration
#define OMV_PWR_SUPPLY                        (PWR_LDO_SUPPLY)

// Linker script constants (see the linker script template stm32.ld.S).
// Note: fb_alloc is a stack-based, dynamically allocated memory on FB.
// The maximum available fb_alloc memory = FB_ALLOC_SIZE + FB_SIZE - (w*h*bpp).
#define OMV_MAIN_MEMORY                       DTCM      // Data/BSS memory.
#define OMV_STACK_MEMORY                      DTCM      // stack memory
#define OMV_STACK_SIZE                        (32K)
#define OMV_FB_MEMORY                         SRAM0     // Framebuffer, fb_alloc
#define OMV_FB_SIZE                           (400K)    // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE                     (76K)     // minimum fb alloc size
#define OMV_JPEG_MEMORY                       SRAM0     // JPEG buffer memory buffer.
#define OMV_JPEG_SIZE                         (32K)     // IDE JPEG buffer (header + data).
#define OMV_DMA_MEMORY                        SRAM2     // DMA buffers memory.
#define OMV_DMA_MEMORY_D1                     SRAM0     // Domain 1 DMA buffers.
#define OMV_DMA_MEMORY_D2                     SRAM2     // Domain 2 DMA buffers.
#define OMV_CM4_BOOT_MEMORY                   SRAM4     // Use to boot CM4 for low-power mode.
#define OMV_CM4_BOOT_SIZE                     1K
#define OMV_GC_BLOCK0_MEMORY                  DTCM      // Main GC block 0.
#define OMV_GC_BLOCK0_SIZE                    (32K)
#define OMV_GC_BLOCK1_MEMORY                  SRAM4     // Extra GC block 1.
#define OMV_GC_BLOCK1_SIZE                    (63K)
#define OMV_GC_BLOCK2_MEMORY                  SRAM1     // Extra GC block 2.
#define OMV_GC_BLOCK2_SIZE                    (276K)
#define OMV_MSC_BUF_SIZE                      (2K)      // USB MSC bot data
#define OMV_LINE_BUF_SIZE                     (3 * 1024) // Image line buffer round(640 * 2BPP * 2 buffers).

// Memory map.
#define OMV_FLASH_ORIGIN                      0x08000000
#define OMV_FLASH_LENGTH                      2048K
#define OMV_DTCM_ORIGIN                       0x20000000 // Note accessible by CPU and MDMA only.
#define OMV_DTCM_LENGTH                       128K
#define OMV_ITCM_ORIGIN                       0x00000000
#define OMV_ITCM_LENGTH                       64K
#define OMV_SRAM0_ORIGIN                      0x24000000
#define OMV_SRAM0_LENGTH                      512K
#define OMV_SRAM1_ORIGIN                      0x30000000
#define OMV_SRAM1_LENGTH                      276K
#define OMV_SRAM2_ORIGIN                      0x30045000
#define OMV_SRAM2_LENGTH                      12K
#define OMV_SRAM4_ORIGIN                      0x38000000
#define OMV_SRAM4_LENGTH                      64K

// Flash configuration.
#define OMV_FLASH_FFS_ORIGIN                  0x08020000
#define OMV_FLASH_FFS_LENGTH                  128K
#define OMV_FLASH_TXT_ORIGIN                  0x08040000
#define OMV_FLASH_TXT_LENGTH                  1792K
#define OMV_FLASH_EXT_ORIGIN                  0x90000000
#define OMV_FLASH_EXT_LENGTH                  16M

// ROMFS configuration.
#define OMV_ROMFS_PART0_ORIGIN                0x90B00000
#define OMV_ROMFS_PART0_LENGTH                4M

// MDMA configuration
#define OMV_MDMA_CHANNEL_DCMI_0               (0)
#define OMV_MDMA_CHANNEL_DCMI_1               (1)
#define OMV_MDMA_CHANNEL_JPEG_IN              (7) // in has a lower pri than out
#define OMV_MDMA_CHANNEL_JPEG_OUT             (6) // out has a higher pri than in

// AXI QoS - Low-High (0:15) - default 0
#define OMV_AXI_QOS_MDMA_R_PRI                15 // Max pri to move data.
#define OMV_AXI_QOS_MDMA_W_PRI                15 // Max pri to move data.

// Enable additional GPIO ports
#define OMV_GPIO_PORT_F_ENABLE                (1)
#define OMV_GPIO_PORT_G_ENABLE                (1)
#define OMV_GPIO_PORT_H_ENABLE                (1)
#define OMV_GPIO_PORT_I_ENABLE                (1)
#define OMV_GPIO_PORT_J_ENABLE                (1)
#define OMV_GPIO_PORT_K_ENABLE                (1)

// CSI I2C bus
#define OMV_CSI_I2C_ID                        (3)
#define OMV_CSI_I2C_SPEED                     (OMV_I2C_SPEED_STANDARD)

// FIR I2C bus
#define OMV_FIR_I2C_ID                        (1)
#define OMV_FIR_I2C_SPEED                     (OMV_I2C_SPEED_FULL)

// Soft I2C bus
#define OMV_SOFT_I2C_SIOC_PIN                 (&omv_pin_B8_GPIO)
#define OMV_SOFT_I2C_SIOD_PIN                 (&omv_pin_B9_GPIO)
#define OMV_SOFT_I2C_SPIN_DELAY               64

// IMU SPI bus
#define OMV_IMU_SPI_ID                        (5)
#define OMV_IMU_SPI_BAUDRATE                  (500000)
#define OMV_IMU_CHIP_LSM6DSOX                 (1)
#define OMV_IMU_X_Y_ROTATION_DEGREES          90
#define OMV_IMU_MOUNTING_Z_DIRECTION          -1

// Camera Interface
#define OMV_CSI_CLK_SOURCE                    (OMV_CSI_CLK_SOURCE_TIM)
#define OMV_CSI_CLK_FREQUENCY                 (12000000)
#define OMV_CSI_TIM                           (TIM3)
#define OMV_CSI_TIM_PIN                       (&omv_pin_A7_TIM3)
#define OMV_CSI_TIM_CHANNEL                   (TIM_CHANNEL_2)
#define OMV_CSI_TIM_CLK_ENABLE()              __TIM3_CLK_ENABLE()
#define OMV_CSI_TIM_CLK_DISABLE()             __TIM3_CLK_DISABLE()
#define OMV_CSI_TIM_PCLK_FREQ()               HAL_RCC_GetPCLK1Freq()
#define OMV_CSI_DMA_MEMCPY_ENABLE             (1)
#define OMV_CSI_HW_CROP_ENABLE                (1)

#define OMV_CSI_D0_PIN                        (&omv_pin_C6_DCMI)
#define OMV_CSI_D1_PIN                        (&omv_pin_C7_DCMI)
#define OMV_CSI_D2_PIN                        (&omv_pin_E0_DCMI)
#define OMV_CSI_D3_PIN                        (&omv_pin_E1_DCMI)
#define OMV_CSI_D4_PIN                        (&omv_pin_E4_DCMI)
#define OMV_CSI_D5_PIN                        (&omv_pin_D3_DCMI)
#define OMV_CSI_D6_PIN                        (&omv_pin_E5_DCMI)
#define OMV_CSI_D7_PIN                        (&omv_pin_E6_DCMI)

#define OMV_CSI_HSYNC_PIN                     (&omv_pin_A4_DCMI)
#define OMV_CSI_VSYNC_PIN                     (&omv_pin_G9_DCMI)
#define OMV_CSI_PXCLK_PIN                     (&omv_pin_A6_DCMI)
//#define OMV_CSI_RESET_PIN                   (&omv_pin_A10_GPIO)
//#define OMV_CSI_POWER_PIN                   (&omv_pin_G3_GPIO)
//#define OMV_CSI_FSYNC_PIN                   (&omv_pin_B4_GPIO)

// DFSDM1
#define OMV_DFSDM                             (DFSDM1_Channel2)
#define OMV_DFSDM_CHANNEL                     (DFSDM_CHANNEL_2)
// DFSDM output clock is derived from the Aclk (set in SAI1SEL[2:0])
// for SAI1 and DFSDM1, which is clocked from PLL1Q by default (50MHz).
#define OMV_DFSDM_FREQMHZ                     (50)
#define OMV_AUDIO_MAX_CHANNELS                (1) // Maximum number of channels.

#define OMV_DFSDM_CK_PIN                      (&omv_pin_D10_DFSDM1)
#define OMV_DFSDM_D1_PIN                      (&omv_pin_E7_DFSDM1)

#define OMV_DFSDM_FLT0                        DFSDM1_Filter0
#define OMV_DFSDM_FLT0_IRQ                    DFSDM1_FLT0_IRQn
#define OMV_DFSDM_FLT0_IRQHandler             DFSDM1_FLT0_IRQHandler
#define OMV_DFSDM_FLT0_DMA_STREAM             DMA1_Stream1
#define OMV_DFSDM_FLT0_DMA_REQUEST            DMA_REQUEST_DFSDM1_FLT0
#define OMV_DFSDM_FLT0_DMA_IRQ                DMA1_Stream1_IRQn
#define OMV_DFSDM_FLT0_DMA_IRQHandler         DMA1_Stream1_IRQHandler

#define OMV_DFSDM_CLK_ENABLE()                __HAL_RCC_DFSDM1_CLK_ENABLE()
#define OMV_DFSDM_CLK_DISABLE()               __HAL_RCC_DFSDM1_CLK_DISABLE()
#define OMV_DFSDM_DMA_CLK_ENABLE()            __HAL_RCC_DMA1_CLK_ENABLE()

// Physical I2C buses.

// I2C bus 1
#define OMV_I2C1_ID                           (1)
#define OMV_I2C1_SCL_PIN                      (&omv_pin_B8_I2C1)
#define OMV_I2C1_SDA_PIN                      (&omv_pin_B9_I2C1)

// I2C bus 3
#define OMV_I2C3_ID                           (3)
#define OMV_I2C3_SCL_PIN                      (&omv_pin_A8_I2C3)
#define OMV_I2C3_SDA_PIN                      (&omv_pin_C9_I2C3)

// Physical SPI buses.

// SPI bus 4
#define OMV_SPI4_ID                           (4)
#define OMV_SPI4_SCLK_PIN                     (&omv_pin_E12_SPI4)
#define OMV_SPI4_MISO_PIN                     (&omv_pin_E13_SPI4)
#define OMV_SPI4_MOSI_PIN                     (&omv_pin_E14_SPI4)
#define OMV_SPI4_SSEL_PIN                     (&omv_pin_E11_SPI4)
#define OMV_SPI4_DMA_TX_CHANNEL               (DMA2_Stream4)
#define OMV_SPI4_DMA_TX_REQUEST               (DMA_REQUEST_SPI4_TX)
#define OMV_SPI4_DMA_RX_CHANNEL               (DMA2_Stream3)
#define OMV_SPI4_DMA_RX_REQUEST               (DMA_REQUEST_SPI4_RX)

// SPI bus 5
#define OMV_SPI5_ID                           (5)
#define OMV_SPI5_SCLK_PIN                     (&omv_pin_F7_SPI5)
#define OMV_SPI5_MISO_PIN                     (&omv_pin_F8_SPI5)
#define OMV_SPI5_MOSI_PIN                     (&omv_pin_F11_SPI5)
#define OMV_SPI5_SSEL_PIN                     (&omv_pin_F6_SPI5)
#define OMV_SPI5_DMA_TX_CHANNEL               (DMA2_Stream4)
#define OMV_SPI5_DMA_TX_REQUEST               (DMA_REQUEST_SPI5_TX)
#define OMV_SPI5_DMA_RX_CHANNEL               (DMA2_Stream3)
#define OMV_SPI5_DMA_RX_REQUEST               (DMA_REQUEST_SPI5_RX)

// SPI LCD Interface
#define OMV_SPI_DISPLAY_CONTROLLER            (OMV_SPI4_ID)
#define OMV_SPI_DISPLAY_MOSI_PIN              (&omv_pin_E14_SPI4)
#define OMV_SPI_DISPLAY_MISO_PIN              (&omv_pin_E13_SPI4)
#define OMV_SPI_DISPLAY_SCLK_PIN              (&omv_pin_E12_SPI4)
#define OMV_SPI_DISPLAY_SSEL_PIN              (&omv_pin_E11_GPIO)

#define OMV_SPI_DISPLAY_RS_PIN                (&omv_pin_G12_GPIO)
#define OMV_SPI_DISPLAY_RST_PIN               (&omv_pin_G1_GPIO)

// FIR Lepton
#define OMV_FIR_LEPTON_I2C_BUS                (OMV_FIR_I2C_ID)
#define OMV_FIR_LEPTON_I2C_BUS_SPEED          (OMV_FIR_I2C_SPEED)

#define OMV_FIR_LEPTON_SPI_BUS                (OMV_SPI4_ID)
#define OMV_FIR_LEPTON_MOSI_PIN               (&omv_pin_E14_SPI4)
#define OMV_FIR_LEPTON_MISO_PIN               (&omv_pin_E13_SPI4)
#define OMV_FIR_LEPTON_SCLK_PIN               (&omv_pin_E12_SPI4)
#define OMV_FIR_LEPTON_SSEL_PIN               (&omv_pin_E11_GPIO)

#endif //__OMV_BOARDCONFIG_H__
