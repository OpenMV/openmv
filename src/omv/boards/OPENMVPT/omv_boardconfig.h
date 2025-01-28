/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2024 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2024 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * Board configuration and pin definitions.
 */
#ifndef __OMV_BOARDCONFIG_H__
#define __OMV_BOARDCONFIG_H__

// Architecture info
#define OMV_BOARD_ARCH                          "OPENMVPT 65536 SDRAM" // 33 chars max
#define OMV_BOARD_TYPE                          "H7"
#define OMV_BOARD_UID_ADDR                      0x1FF1E800   // Unique ID address.
#define OMV_BOARD_UID_SIZE                      3            // Unique ID size in words.
#define OMV_BOARD_UID_OFFSET                    4            // Bytes offset for multi-word UIDs.

// JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE                   (1)
#define OMV_JPEG_QUALITY_LOW                    (50)
#define OMV_JPEG_QUALITY_HIGH                   (90)
#define OMV_JPEG_QUALITY_THRESHOLD              (1920 * 1080 * 2)

// GPU Configuration
#define OMV_GPU_ENABLE                          (1)

// CSI drivers configuration.
#define OMV_OV5640_ENABLE                       (1)
#define OMV_OV5640_AF_ENABLE                    (1)
#define OMV_OV5640_CLK_FREQ                     (24000000)
#define OMV_OV5640_PLL_CTRL2                    (0x64)
#define OMV_OV5640_PLL_CTRL3                    (0x13)
#define OMV_OV5640_REV_Y_CHECK                  (1)
#define OMV_OV5640_REV_Y_FREQ                   (25000000)
#define OMV_OV5640_REV_Y_CTRL2                  (0x54)
#define OMV_OV5640_REV_Y_CTRL3                  (0x13)

// FIR drivers configuration.
#define OMV_FIR_MLX90621_ENABLE                 (1)
#define OMV_FIR_MLX90640_ENABLE                 (1)
#define OMV_FIR_MLX90641_ENABLE                 (1)
#define OMV_FIR_AMG8833_ENABLE                  (1)
#define OMV_FIR_LEPTON_ENABLE                   (1)

// UMM heap block size
#define OMV_UMM_BLOCK_SIZE                      256

// USB IRQn.
#define OMV_USB_IRQN                            (OTG_FS_IRQn)

//PLL1 48MHz for USB, SDMMC and FDCAN
#define OMV_OSC_PLL1M                           (3)
#define OMV_OSC_PLL1N                           (240)
#define OMV_OSC_PLL1P                           (2)
#define OMV_OSC_PLL1Q                           (20)
#define OMV_OSC_PLL1R                           (2)
#define OMV_OSC_PLL1VCI                         (RCC_PLL1VCIRANGE_2)
#define OMV_OSC_PLL1VCO                         (RCC_PLL1VCOWIDE)
#define OMV_OSC_PLL1FRAC                        (0)

// PLL2 200MHz for FMC and QSPI.
#define OMV_OSC_PLL2M                           (3)
#define OMV_OSC_PLL2N                           (100)
#define OMV_OSC_PLL2P                           (2)
#define OMV_OSC_PLL2Q                           (2)
#define OMV_OSC_PLL2R                           (2)
#define OMV_OSC_PLL2VCI                         (RCC_PLL2VCIRANGE_2)
#define OMV_OSC_PLL2VCO                         (RCC_PLL2VCOWIDE)
#define OMV_OSC_PLL2FRAC                        (0)

// PLL3 160MHz for ADC and SPI123
#define OMV_OSC_PLL3M                           (3)
#define OMV_OSC_PLL3N                           (80)
#define OMV_OSC_PLL3P                           (2)
#define OMV_OSC_PLL3Q                           (2)
#define OMV_OSC_PLL3R                           (2)
#define OMV_OSC_PLL3VCI                         (RCC_PLL3VCIRANGE_2)
#define OMV_OSC_PLL3VCO                         (RCC_PLL3VCOWIDE)
#define OMV_OSC_PLL3FRAC                        (0)

// Clock Sources
#define OMV_OSC_PLL_CLKSOURCE                   RCC_PLLSOURCE_HSE
#define OMV_OSC_USB_CLKSOURCE                   RCC_USBCLKSOURCE_PLL
#define OMV_OSC_RNG_CLKSOURCE                   RCC_RNGCLKSOURCE_HSI48
#define OMV_OSC_ADC_CLKSOURCE                   RCC_ADCCLKSOURCE_PLL2
//#define OMV_OSC_RTC_CLKSOURCE                   RCC_RTCCLKSOURCE_LSE
#define OMV_OSC_SPI123_CLKSOURCE                RCC_SPI123CLKSOURCE_PLL2

// HSE/HSI/CSI State
// The LSE/LSI and RTC are managed by micropython's rtc.c.
// #define OMV_OSC_LSE_STATE                      (RCC_LSE_ON)
// #define OMV_OSC_LSE_DRIVE                      (RCC_LSEDRIVE_HIGH)
#define OMV_OSC_HSE_STATE                       (RCC_HSE_ON)
#define OMV_OSC_HSI48_STATE                     (RCC_HSI48_ON)
// Errata
#define OMV_OMVPT_ERRATA_RTC                    (1)

// Flash Latency
#define OMV_FLASH_LATENCY                       (FLASH_LATENCY_2)

// Power supply configuration
#define OMV_PWR_SUPPLY                          (PWR_LDO_SUPPLY)

// Linker script constants (see the linker script template stm32.ld.S).
// Note: fb_alloc is a stack-based, dynamically allocated memory on FB.
// The maximum available fb_alloc memory = FB_ALLOC_SIZE + FB_SIZE - (w*h*bpp).
#define OMV_MAIN_MEMORY                         SRAM1   // Data/BSS memory
#define OMV_STACK_MEMORY                        ITCM    // stack memory
#define OMV_STACK_SIZE                          (64K)
#define OMV_FB_MEMORY                           DRAM    // Framebuffer, fb_alloc
#define OMV_FB_SIZE                             (32M)   // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE                       (23M)   // minimum fb alloc size
#define OMV_FB_OVERLAY_MEMORY                   SRAM0   // Fast fb_alloc memory.
#define OMV_FB_OVERLAY_SIZE                     (496K)  // Fast fb_alloc memory size.
#define OMV_JPEG_MEMORY                         DRAM    // JPEG buffer memory buffer.
#define OMV_JPEG_SIZE                           (1M)    // IDE JPEG buffer (header + data).
#define OMV_VOSPI_MEMORY                        SRAM4   // VoSPI buffer memory.
#define OMV_VOSPI_SIZE                          (38K)
#define OMV_DMA_MEMORY                          SRAM3   // DMA buffers memory.
#define OMV_DMA_MEMORY_D1                       SRAM0   // Domain 1 DMA buffers.
#define OMV_DMA_MEMORY_D2                       SRAM3   // Domain 2 DMA buffers.
#define OMV_DMA_MEMORY_D3                       SRAM4   // Domain 3 DMA buffers.
#define OMV_GC_BLOCK0_MEMORY                    SRAM1   // Main GC block.
#define OMV_GC_BLOCK0_SIZE                      (250K)
#define OMV_GC_BLOCK1_MEMORY                    DRAM    // Extra GC block 0.
#define OMV_GC_BLOCK1_SIZE                      (8M)
#define OMV_SDRAM_SIZE                          (64 * 1024 * 1024)  // This needs to be here for UVC firmware.
#define OMV_MSC_BUF_SIZE                        (2K)    // USB MSC bot data
#define OMV_LINE_BUF_SIZE                       (11 * 1024) // Image line buffer round(2592 * 2BPP * 2 buffers).

// Memory map.
#define OMV_FLASH_ORIGIN                        0x08000000
#define OMV_FLASH_LENGTH                        2048K
#define OMV_DTCM_ORIGIN                         0x20000000 // Note accessible by CPU and MDMA only.
#define OMV_DTCM_LENGTH                         128K
#define OMV_ITCM_ORIGIN                         0x00000000
#define OMV_ITCM_LENGTH                         64K
#define OMV_SRAM0_ORIGIN                        0x24000000
#define OMV_SRAM0_LENGTH                        512K
#define OMV_SRAM1_ORIGIN                        0x30000000
#define OMV_SRAM1_LENGTH                        272K // SRAM1 + SRAM2 + 1/2 SRAM3
#define OMV_SRAM3_ORIGIN                        0x30044000
#define OMV_SRAM3_LENGTH                        16K
#define OMV_SRAM4_ORIGIN                        0x38000000
#define OMV_SRAM4_LENGTH                        64K
#define OMV_DRAM_ORIGIN                         0xC0000000
#define OMV_DRAM_LENGTH                         64M

// Flash configuration.
#define OMV_FLASH_BOOT_ORIGIN                   0x08000000
#define OMV_FLASH_BOOT_LENGTH                   128K
#define OMV_FLASH_FFS_ORIGIN                    0x08020000
#define OMV_FLASH_FFS_LENGTH                    128K
#define OMV_FLASH_TXT_ORIGIN                    0x08040000
#define OMV_FLASH_TXT_LENGTH                    1792K

// ROMFS configuration.
#define OMV_ROMFS_PART0_ORIGIN                  0x91800000
#define OMV_ROMFS_PART0_LENGTH                  8M

// MDMA configuration
#define OMV_MDMA_CHANNEL_DCMI_0                 (0)
#define OMV_MDMA_CHANNEL_DCMI_1                 (1)
#define OMV_MDMA_CHANNEL_JPEG_IN                (7) // in has a lower pri than out
#define OMV_MDMA_CHANNEL_JPEG_OUT               (6) // out has a higher pri than in

// AXI QoS - Low-High (0:15) - default 0
#define OMV_AXI_QOS_MDMA_R_PRI                  14 // Max pri to move data.
#define OMV_AXI_QOS_MDMA_W_PRI                  15 // Max pri to move data.
#define OMV_AXI_QOS_LTDC_R_PRI                  15 // Max pri to read out the frame buffer.

// Enable additional GPIO ports.
#define OMV_GPIO_PORT_F_ENABLE                  (1)
#define OMV_GPIO_PORT_G_ENABLE                  (1)
#define OMV_GPIO_PORT_H_ENABLE                  (1)
#define OMV_GPIO_PORT_I_ENABLE                  (1)
#define OMV_GPIO_PORT_J_ENABLE                  (1)
#define OMV_GPIO_PORT_K_ENABLE                  (1)

// CSI I2C bus
#define OMV_CSI_I2C_ID                          (1)
#define OMV_CSI_I2C_SPEED                       (OMV_I2C_SPEED_STANDARD)

// FIR I2C bus
#define OMV_FIR_I2C_ID                          (2)
#define OMV_FIR_I2C_SPEED                       (OMV_I2C_SPEED_FULL)

// Soft I2C bus.
#define OMV_SOFT_I2C_SIOC_PIN                   (&omv_pin_B10_GPIO)
#define OMV_SOFT_I2C_SIOD_PIN                   (&omv_pin_B11_GPIO)
#define OMV_SOFT_I2C_SPIN_DELAY                 64

// WINC1500 WiFi module SPI bus
#define OMV_WINC_SPI_ID                         (5)
#define OMV_WINC_SPI_BAUDRATE                   (50000000)
#define OMV_WINC_EN_PIN                         (&omv_pin_A0_GPIO)
#define OMV_WINC_RST_PIN                        (&omv_pin_C3_GPIO)
#define OMV_WINC_WAKE_PIN                       (&omv_pin_A1_GPIO)
#define OMV_WINC_CFG_PIN                        (&omv_pin_I15_GPIO)
#define OMV_WINC_IRQ_PIN                        (&omv_pin_H5_GPIO)

// Camera interface
#define OMV_CSI_CLK_SOURCE                      (OMV_CSI_CLK_SOURCE_TIM)
#define OMV_CSI_CLK_FREQUENCY                   (12000000)
#define OMV_CSI_TIM                             (TIM1)
#define OMV_CSI_TIM_PIN                         (&omv_pin_A8_TIM1)
#define OMV_CSI_TIM_CHANNEL                     (TIM_CHANNEL_1)
#define OMV_CSI_TIM_CLK_ENABLE()                __TIM1_CLK_ENABLE()
#define OMV_CSI_TIM_CLK_DISABLE()               __TIM1_CLK_DISABLE()
#define OMV_CSI_TIM_PCLK_FREQ()                 HAL_RCC_GetPCLK2Freq()
#define OMV_CSI_DMA_MEMCPY_ENABLE               (1)
#define OMV_CSI_HW_CROP_ENABLE                  (1)

#define OMV_CSI_D0_PIN                          (&omv_pin_C6_DCMI)
#define OMV_CSI_D1_PIN                          (&omv_pin_C7_DCMI)
#define OMV_CSI_D2_PIN                          (&omv_pin_G10_DCMI)
#define OMV_CSI_D3_PIN                          (&omv_pin_G11_DCMI)
#define OMV_CSI_D4_PIN                          (&omv_pin_E4_DCMI)
#define OMV_CSI_D5_PIN                          (&omv_pin_B6_DCMI)
#define OMV_CSI_D6_PIN                          (&omv_pin_E5_DCMI)
#define OMV_CSI_D7_PIN                          (&omv_pin_E6_DCMI)

#define OMV_CSI_HSYNC_PIN                       (&omv_pin_A4_DCMI)
#define OMV_CSI_VSYNC_PIN                       (&omv_pin_B7_DCMI)
#define OMV_CSI_PXCLK_PIN                       (&omv_pin_A6_DCMI)
#define OMV_CSI_RESET_PIN                       (&omv_pin_A10_GPIO)
#define OMV_CSI_POWER_PIN                       (&omv_pin_D7_GPIO)

// Physical I2C buses.

// I2C bus 1
#define OMV_I2C1_ID                             (1)
#define OMV_I2C1_SCL_PIN                        (&omv_pin_B8_I2C1)
#define OMV_I2C1_SDA_PIN                        (&omv_pin_B9_I2C1)

// I2C bus 2
#define OMV_I2C2_ID                             (2)
#define OMV_I2C2_SCL_PIN                        (&omv_pin_B10_I2C2)
#define OMV_I2C2_SDA_PIN                        (&omv_pin_B11_I2C2)

// Physical SPI buses.

// SPI bus 2
#define OMV_SPI2_ID                             (2)
#define OMV_SPI2_SCLK_PIN                       (&omv_pin_B13_SPI2)
#define OMV_SPI2_MISO_PIN                       (&omv_pin_B14_SPI2)
#define OMV_SPI2_MOSI_PIN                       (&omv_pin_B15_SPI2)
#define OMV_SPI2_SSEL_PIN                       (&omv_pin_B12_GPIO)
#define OMV_SPI2_DMA_TX_CHANNEL                 (DMA1_Stream4)
#define OMV_SPI2_DMA_TX_REQUEST                 (DMA_REQUEST_SPI2_TX)
#define OMV_SPI2_DMA_RX_CHANNEL                 (DMA1_Stream3)
#define OMV_SPI2_DMA_RX_REQUEST                 (DMA_REQUEST_SPI2_RX)

// SPI bus 3
#define OMV_SPI3_ID                             (3)
#define OMV_SPI3_SCLK_PIN                       (&omv_pin_B3_SPI3)
#define OMV_SPI3_MISO_PIN                       (&omv_pin_B4_SPI3)
#define OMV_SPI3_MOSI_PIN                       (&omv_pin_B5_SPI3)
#define OMV_SPI3_SSEL_PIN                       (&omv_pin_A15_GPIO)
#define OMV_SPI3_DMA_TX_CHANNEL                 (DMA1_Stream7)
#define OMV_SPI3_DMA_TX_REQUEST                 (DMA_REQUEST_SPI3_TX)
#define OMV_SPI3_DMA_RX_CHANNEL                 (DMA1_Stream2)
#define OMV_SPI3_DMA_RX_REQUEST                 (DMA_REQUEST_SPI3_RX)

// SPI bus 5
#define OMV_SPI5_ID                             (5)
#define OMV_SPI5_SCLK_PIN                       (&omv_pin_H6_SPI5)
#define OMV_SPI5_MISO_PIN                       (&omv_pin_H7_SPI5)
#define OMV_SPI5_MOSI_PIN                       (&omv_pin_J10_SPI5)
#define OMV_SPI5_SSEL_PIN                       (&omv_pin_K1_GPIO)
#define OMV_SPI5_DMA_TX_CHANNEL                 (DMA2_Stream4)
#define OMV_SPI5_DMA_TX_REQUEST                 (DMA_REQUEST_SPI5_TX)
#define OMV_SPI5_DMA_RX_CHANNEL                 (DMA2_Stream3)
#define OMV_SPI5_DMA_RX_REQUEST                 (DMA_REQUEST_SPI5_RX)

// LCD Interface
#define OMV_RGB_DISPLAY_CONTROLLER              (LTDC)
#define OMV_RGB_DISPLAY_CLK_ENABLE()            __HAL_RCC_LTDC_CLK_ENABLE()
#define OMV_RGB_DISPLAY_CLK_DISABLE()           __HAL_RCC_LTDC_CLK_DISABLE()
#define OMV_RGB_DISPLAY_FORCE_RESET()           __HAL_RCC_LTDC_FORCE_RESET()
#define OMV_RGB_DISPLAY_RELEASE_RESET()         __HAL_RCC_LTDC_RELEASE_RESET()

#define OMV_RGB_DISPLAY_R0_PIN                  (&omv_pin_G13_LTDC)
#define OMV_RGB_DISPLAY_R1_PIN                  (&omv_pin_A2_LTDC)
#define OMV_RGB_DISPLAY_R2_PIN                  (&omv_pin_J1_LTDC)
#define OMV_RGB_DISPLAY_R3_PIN                  (&omv_pin_J2_LTDC)
#define OMV_RGB_DISPLAY_R4_PIN                  (&omv_pin_J3_LTDC)
#define OMV_RGB_DISPLAY_R5_PIN                  (&omv_pin_J4_LTDC)
#define OMV_RGB_DISPLAY_R6_PIN                  (&omv_pin_J5_LTDC)
#define OMV_RGB_DISPLAY_R7_PIN                  (&omv_pin_J0_LTDC)

#define OMV_RGB_DISPLAY_G0_PIN                  (&omv_pin_J7_LTDC)
#define OMV_RGB_DISPLAY_G1_PIN                  (&omv_pin_J8_LTDC)
#define OMV_RGB_DISPLAY_G2_PIN                  (&omv_pin_J9_LTDC)
#define OMV_RGB_DISPLAY_G3_PIN                  (&omv_pin_J12_LTDC)
#define OMV_RGB_DISPLAY_G4_PIN                  (&omv_pin_J11_LTDC)
#define OMV_RGB_DISPLAY_G5_PIN                  (&omv_pin_K0_LTDC)
#define OMV_RGB_DISPLAY_G6_PIN                  (&omv_pin_I11_LTDC)
#define OMV_RGB_DISPLAY_G7_PIN                  (&omv_pin_D3_LTDC)

#define OMV_RGB_DISPLAY_B0_PIN                  (&omv_pin_G14_LTDC)
#define OMV_RGB_DISPLAY_B1_PIN                  (&omv_pin_G12_LTDC)
#define OMV_RGB_DISPLAY_B2_PIN                  (&omv_pin_D6_LTDC)
#define OMV_RGB_DISPLAY_B3_PIN                  (&omv_pin_J15_LTDC)
#define OMV_RGB_DISPLAY_B4_PIN                  (&omv_pin_K3_LTDC)
#define OMV_RGB_DISPLAY_B5_PIN                  (&omv_pin_K4_LTDC)
#define OMV_RGB_DISPLAY_B6_PIN                  (&omv_pin_K5_LTDC)
#define OMV_RGB_DISPLAY_B7_PIN                  (&omv_pin_K6_LTDC)

#define OMV_RGB_DISPLAY_CLK_PIN                 (&omv_pin_I14_LTDC)
#define OMV_RGB_DISPLAY_DE_PIN                  (&omv_pin_K7_LTDC)
#define OMV_RGB_DISPLAY_HSYNC_PIN               (&omv_pin_I12_LTDC)
#define OMV_RGB_DISPLAY_VSYNC_PIN               (&omv_pin_I13_LTDC)
#define OMV_RGB_DISPLAY_DISP_PIN                (&omv_pin_G9_GPIO)
#define OMV_RGB_DISPLAY_BL_PIN                  (&omv_pin_B0_TIM3)

// SPI LCD Interface
#define OMV_SPI_DISPLAY_CONTROLLER              (OMV_SPI2_ID)
#define OMV_SPI_DISPLAY_MOSI_PIN                (&omv_pin_B15_SPI2)
#define OMV_SPI_DISPLAY_MISO_PIN                (&omv_pin_B14_SPI2)
#define OMV_SPI_DISPLAY_SCLK_PIN                (&omv_pin_B13_SPI2)
#define OMV_SPI_DISPLAY_SSEL_PIN                (&omv_pin_B12_GPIO)

#define OMV_SPI_DISPLAY_RS_PIN                  (&omv_pin_D13_GPIO)
#define OMV_SPI_DISPLAY_RST_PIN                 (&omv_pin_D12_GPIO)
#define OMV_SPI_DISPLAY_BL_PIN                  (&omv_pin_A5_GPIO)
#define OMV_SPI_DISPLAY_TRIPLE_BUFFER           (1)

// HDMI CEC/DDC I/O
#define OMV_DISPLAY_CEC_ENABLE                  (1)
#define OMV_DISPLAY_DDC_ENABLE                  (1)
#define OMV_CEC_PIN                             (&omv_pin_H2_GPIO)
#define OMV_DDC_SCL_PIN                         (pin_H3)
#define OMV_DDC_SDA_PIN                         (pin_H4)

// TFP410 DVI serializer
#define OMV_TFP410_ENABLE                       (1)
#define OMV_TFP410_RESET_PIN                    (&omv_pin_D11_GPIO)
#define OMV_TFP410_SCL_PIN                      (pin_B1)
#define OMV_TFP410_SDA_PIN                      (pin_B2)
#define OMV_TFP410_INT_PIN                      (&omv_pin_I8_GPIO)

// FT5X06 Touch Screen
#define OMV_FT5X06_ENABLE                       (1)
#define OMV_FT5X06_RESET_PIN                    (&omv_pin_K2_GPIO)
#define OMV_FT5X06_SCL_PIN                      (pin_J13)
#define OMV_FT5X06_SDA_PIN                      (pin_J14)
#define OMV_FT5X06_INT_PIN                      (&omv_pin_J6_GPIO)

// FIR Lepton
#define OMV_FIR_LEPTON_I2C_BUS                  (OMV_CSI_I2C_ID)
#define OMV_FIR_LEPTON_I2C_BUS_SPEED            (OMV_CSI_I2C_SPEED)

#define OMV_FIR_LEPTON_SPI_BUS                  (OMV_SPI3_ID)
#define OMV_FIR_LEPTON_MOSI_PIN                 (&omv_pin_B5_SPI3)
#define OMV_FIR_LEPTON_MISO_PIN                 (&omv_pin_B4_SPI3)
#define OMV_FIR_LEPTON_SCLK_PIN                 (&omv_pin_B3_SPI3)
#define OMV_FIR_LEPTON_SSEL_PIN                 (&omv_pin_A15_GPIO)

#define OMV_FIR_LEPTON_RESET_PIN                (&omv_pin_D5_GPIO)
#define OMV_FIR_LEPTON_POWER_PIN                (&omv_pin_D4_GPIO)
#define OMV_FIR_LEPTON_VSYNC_PIN                (&omv_pin_E3_GPIO)

#define OMV_FIR_LEPTON_MCLK_PIN                 (&omv_pin_A3_TIM15)
#define OMV_FIR_LEPTON_MCLK_FREQ                (24000000)

#define OMV_FIR_LEPTON_MCLK_TIM                 (TIM15)
#define OMV_FIR_LEPTON_MCLK_TIM_CHANNEL         (TIM_CHANNEL_2)
#define OMV_FIR_LEPTON_MCLK_TIM_CLK_ENABLE()    __HAL_RCC_TIM15_CLK_ENABLE()
#define OMV_FIR_LEPTON_MCLK_TIM_CLK_DISABLE()   __HAL_RCC_TIM15_CLK_DISABLE()
#define OMV_FIR_LEPTON_MCLK_TIM_FORCE_RESET()   __HAL_RCC_TIM15_FORCE_RESET()
#define OMV_FIR_LEPTON_MCLK_TIM_RELEASE_RESET() __HAL_RCC_TIM15_RELEASE_RESET()
#define OMV_FIR_LEPTON_MCLK_TIM_PCLK_FREQ()     HAL_RCC_GetPCLK2Freq()

// Buzzer
#define OMV_BUZZER_PIN                          (&omv_pin_A1_TIM2)
#define OMV_BUZZER_FREQ                         (4000)

#define OMV_BUZZER_TIM                          (TIM2)
#define OMV_BUZZER_TIM_CHANNEL                  (TIM_CHANNEL_2)
#define OMV_BUZZER_TIM_CLK_ENABLE()             __HAL_RCC_TIM2_CLK_ENABLE()
#define OMV_BUZZER_TIM_CLK_DISABLE()            __HAL_RCC_TIM2_CLK_DISABLE()
#define OMV_BUZZER_TIM_FORCE_RESET()            __HAL_RCC_TIM2_FORCE_RESET()
#define OMV_BUZZER_TIM_RELEASE_RESET()          __HAL_RCC_TIM2_RELEASE_RESET()
#define OMV_BUZZER_TIM_PCLK_FREQ()              HAL_RCC_GetPCLK1Freq()

#endif //__OMV_BOARDCONFIG_H__
