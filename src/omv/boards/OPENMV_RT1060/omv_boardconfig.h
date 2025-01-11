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
#define OMV_BOARD_ARCH                  "OMVRT1060 32768 SDRAM"    // 33 chars max
#define OMV_BOARD_TYPE                  "IMXRT1060"
#define OMV_BOARD_UID_ADDR              0x401f4410   // Unique ID address.
#define OMV_BOARD_UID_SIZE              3            // Unique ID size in words.
#define OMV_BOARD_UID_OFFSET            12           // Bytes offset for multi-word UIDs.

// JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE           (0)
#define OMV_JPEG_QUALITY_LOW            (50)
#define OMV_JPEG_QUALITY_HIGH           (90)
#define OMV_JPEG_QUALITY_THRESHOLD      (320 * 240 * 2)

// CSI drivers configuration.
#define OMV_OV5640_ENABLE               (1)
#define OMV_OV5640_AF_ENABLE            (1)
#define OMV_OV5640_CLK_FREQ             (24000000)
#define OMV_OV5640_PLL_CTRL2            (0x64)
#define OMV_OV5640_PLL_CTRL3            (0x13)
#define OMV_OV5640_REV_Y_CHECK          (0)
#define OMV_OV5640_REV_Y_FREQ           (25000000)
#define OMV_OV5640_REV_Y_CTRL2          (0x54)
#define OMV_OV5640_REV_Y_CTRL3          (0x13)

#define OMV_OV7725_ENABLE               (1)
#define OMV_OV7725_PLL_CONFIG           (0x41)   // x4
#define OMV_OV7725_BANDING              (0x7F)

#define OMV_MT9M114_ENABLE              (1)
#define OMV_MT9V0XX_ENABLE              (1)
#define OMV_BOSON_ENABLE                (1)
#define OMV_LEPTON_ENABLE               (1)
#define OMV_GENX320_ENABLE              (1)
#define OMV_GENX320_EHC_ENABLE          (1)
#define OMV_PAG7920_ENABLE              (1)
#define OMV_PAJ6100_ENABLE              (1)
#define OMV_FROGEYE2020_ENABLE          (1)

// FIR drivers configuration.
#define OMV_FIR_MLX90621_ENABLE         (1)
#define OMV_FIR_MLX90640_ENABLE         (1)
#define OMV_FIR_MLX90641_ENABLE         (1)
#define OMV_FIR_AMG8833_ENABLE          (1)
#define OMV_FIR_LEPTON_ENABLE           (1)

// Debugging configuration.
#define OMV_TUSBDBG_ENABLE              (1)
#define OMV_TUSBDBG_BUFFER              (2048)

// UMM heap block size
#define OMV_UMM_BLOCK_SIZE              256

// USB config.
#define OMV_USB_IRQN                    (USB_OTG1_IRQn)
#define OMV_USB1_IRQ_HANDLER            (USB_OTG1_IRQHandler)
#define OMV_USB2_IRQ_HANDLER            (USB_OTG2_IRQHandler)

#define OMV_USB_PHY_ID                  (kUSB_ControllerEhci0)
#define OMV_USB_PHY_D_CAL               (0x0CU)
#define OMV_USB_PHY_TXCAL45DP           (0x06U)
#define OMV_USB_PHY_TXCAL45DM           (0x06U)

// Linker script constants (see the linker script template mimxrt10xx.ld.S).
// Note: fb_alloc is a stack-based, dynamically allocated memory on FB.
// The maximum available fb_alloc memory = FB_ALLOC_SIZE + FB_SIZE - (w*h*bpp).
#define OMV_MAIN_MEMORY                 DTCM    // Data/BSS memory
#define OMV_STACK_MEMORY                ITCM1   // stack memory
#define OMV_STACK_SIZE                  (32K)
#define OMV_JPEG_MEMORY                 DRAM    // JPEG buffer memory buffer.
#define OMV_JPEG_SIZE                   (1M)    // IDE JPEG buffer (header + data).
#define OMV_FB_MEMORY                   DRAM    // Framebuffer, fb_alloc
#define OMV_FB_SIZE                     (13M)   // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE               (10M)   // minimum fb alloc size
#define OMV_FB_OVERLAY_MEMORY           OCRM1   // Fast fb_alloc memory.
#define OMV_FB_OVERLAY_SIZE             (512K)
#define OMV_VOSPI_MEMORY                OCRM2   // VoSPI buffer memory.
#define OMV_VOSPI_SIZE                  (38K)
#define OMV_DMA_MEMORY                  DTCM    // Misc DMA buffers memory.
#define OMV_GC_BLOCK0_MEMORY            OCRM2   // Extra GC block 0.
#define OMV_GC_BLOCK0_SIZE              (26K)
#define OMV_GC_BLOCK1_MEMORY            DTCM    // Main GC block
#define OMV_GC_BLOCK1_SIZE              (290K)
#define OMV_GC_BLOCK2_MEMORY            DRAM    // Extra GC block 1.
#define OMV_GC_BLOCK2_SIZE              (8M)
#define OMV_RAMFUNC_MEMORY              ITCM2   // RAM code memory.
#define OMV_LINE_BUF_SIZE               (11 * 1024)  // Image line buffer.

// Memory configuration.
#define OMV_DTCM_ORIGIN                 0x20000000
#define OMV_DTCM_LENGTH                 384K
#define OMV_ITCM1_ORIGIN                0x00000000
#define OMV_ITCM1_LENGTH                32K
#define OMV_ITCM2_ORIGIN                0x00008000
#define OMV_ITCM2_LENGTH                32K
#define OMV_OCRM1_ORIGIN                0x20200000
#define OMV_OCRM1_LENGTH                512K
#define OMV_OCRM2_ORIGIN                0x20280000   // Allocated from FlexRAM.
#define OMV_OCRM2_LENGTH                64K
#define OMV_DRAM_ORIGIN                 0x80000000
#define OMV_DRAM_LENGTH                 32M
#define OMV_FLEXRAM_CONFIG              (0x5AAAAAAF)  // OCRAM: 64K DTCM: 384K ITCM: 64K

// Flash configuration.
#define OMV_FLASH_ORIGIN                0x60000000
#define OMV_FLASH_LENGTH                0x00800000
#define OMV_FLASH_APP_ORIGIN            0x60040000   // 256K reserved for bootloader
#define OMV_FLASH_APP_LENGTH            0x007C0000
#define OMV_FLASH_TXT_ORIGIN            0x60043000
#define OMV_FLASH_TXT_LENGTH            0x00380000
#define OMV_FLASH_FFS_ORIGIN            0x60400000
#define OMV_FLASH_FFS_LENGTH            0x00400000

// ROMFS configuration.
#define OMV_ROMFS_PART0_ORIGIN          0x60800000
#define OMV_ROMFS_PART0_LENGTH          8M

// CSI I2C bus
#define OMV_CSI_I2C_ID                  (1)
#define OMV_CSI_I2C_SPEED               (OMV_I2C_SPEED_STANDARD)

// FIR I2C bus
#define OMV_FIR_I2C_ID                  (4)
#define OMV_FIR_I2C_SPEED               (OMV_I2C_SPEED_FULL)

// CSI SPI bus
#define OMV_CSI_SPI_ID                  (4)

// Physical I2C buses.
// LPI2C1
#define OMV_I2C1_ID                     (1)
#define OMV_I2C1_SCL_PIN                (&omv_pin_LPI2C1_SCL)
#define OMV_I2C1_SDA_PIN                (&omv_pin_LPI2C1_SDA)

// LPI2C2
#define OMV_I2C2_ID                     (2)
#define OMV_I2C2_SCL_PIN                (&omv_pin_LPI2C2_SCL)
#define OMV_I2C2_SDA_PIN                (&omv_pin_LPI2C2_SDA)

// LPI2C4
#define OMV_I2C4_ID                     (4)
#define OMV_I2C4_SCL_PIN                (&omv_pin_LPI2C4_SCL)
#define OMV_I2C4_SDA_PIN                (&omv_pin_LPI2C4_SDA)

// Physical SPI buses.
// LPSPI3
#define OMV_SPI3_ID                     (3)
#define OMV_SPI3_SCLK_PIN               (&omv_pin_LPSPI3_SCLK)
#define OMV_SPI3_MISO_PIN               (&omv_pin_LPSPI3_MISO)
#define OMV_SPI3_MOSI_PIN               (&omv_pin_LPSPI3_MOSI)
#define OMV_SPI3_SSEL_PIN               (&omv_pin_LPSPI3_SSEL)
#define OMV_SPI3_DMA                    (DMA0)
#define OMV_SPI3_DMA_MUX                (DMAMUX)
#define OMV_SPI3_DMA_TX_CHANNEL         (3U)
#define OMV_SPI3_DMA_RX_CHANNEL         (2U)

// LPSPI4
#define OMV_SPI4_ID                     (4)
#define OMV_SPI4_SCLK_PIN               (&omv_pin_LPSPI4_SCLK)
#define OMV_SPI4_MISO_PIN               (&omv_pin_LPSPI4_MISO)
#define OMV_SPI4_MOSI_PIN               (&omv_pin_LPSPI4_MOSI)
#define OMV_SPI4_SSEL_PIN               (&omv_pin_LPSPI4_SSEL)
#define OMV_SPI4_DMA                    (DMA0)
#define OMV_SPI4_DMA_MUX                (DMAMUX)
#define OMV_SPI4_DMA_TX_CHANNEL         (1U)
#define OMV_SPI4_DMA_RX_CHANNEL         (0U)

// SPI LCD Interface
#define OMV_SPI_DISPLAY_CONTROLLER      (OMV_SPI3_ID)
#define OMV_SPI_DISPLAY_MOSI_PIN        (&omv_pin_LPSPI3_MOSI)
#define OMV_SPI_DISPLAY_MISO_PIN        (&omv_pin_LPSPI3_MISO)
#define OMV_SPI_DISPLAY_SCLK_PIN        (&omv_pin_LPSPI3_SCLK)
#define OMV_SPI_DISPLAY_SSEL_PIN        (&omv_pin_LPSPI3_GPIO)

#define OMV_SPI_DISPLAY_RS_PIN          (&omv_pin_P8_GPIO)
#define OMV_SPI_DISPLAY_RST_PIN         (&omv_pin_P7_GPIO)
#define OMV_SPI_DISPLAY_BL_PIN          (&omv_pin_P6_GPIO)
#define OMV_SPI_DISPLAY_TRIPLE_BUFFER   (0)
#define OMV_SPI_DISPLAY_RX_CLK_DIV      (8)

// FIR Lepton
#define OMV_FIR_LEPTON_I2C_BUS          (OMV_FIR_I2C_ID)
#define OMV_FIR_LEPTON_I2C_BUS_SPEED    (OMV_FIR_I2C_SPEED)

#define OMV_FIR_LEPTON_SPI_BUS          (OMV_SPI3_ID)
#define OMV_FIR_LEPTON_MOSI_PIN         (&omv_pin_LPSPI3_MOSI)
#define OMV_FIR_LEPTON_MISO_PIN         (&omv_pin_LPSPI3_MISO)
#define OMV_FIR_LEPTON_SCLK_PIN         (&omv_pin_LPSPI3_SCLK)
#define OMV_FIR_LEPTON_SSEL_PIN         (&omv_pin_LPSPI3_GPIO)

// Camera interface configuration.
#define OMV_CSI_BASE                    (CSI)
#define OMV_CSI_DMA                     (DMA0)
#define OMV_CSI_DMA_MUX                 (DMAMUX)
#define OMV_CSI_DMA_CHANNEL_START       (4U)
#define OMV_CSI_DMA_CHANNEL_COUNT       (2U)
#define OMV_CSI_CLK_FREQUENCY           (12000000)
#define OMV_CSI_DMA_MEMCPY_ENABLE       (1)
#define OMV_CSI_HW_SWAP_ENABLE          (1)
#define OMV_CSI_HW_CROP_ENABLE          (1)

#define OMV_CSI_D0_PIN                  (&omv_pin_DCMI_D0)
#define OMV_CSI_D1_PIN                  (&omv_pin_DCMI_D1)
#define OMV_CSI_D2_PIN                  (&omv_pin_DCMI_D2)
#define OMV_CSI_D3_PIN                  (&omv_pin_DCMI_D3)
#define OMV_CSI_D4_PIN                  (&omv_pin_DCMI_D4)
#define OMV_CSI_D5_PIN                  (&omv_pin_DCMI_D5)
#define OMV_CSI_D6_PIN                  (&omv_pin_DCMI_D6)
#define OMV_CSI_D7_PIN                  (&omv_pin_DCMI_D7)

#define OMV_CSI_HSYNC_PIN               (&omv_pin_DCMI_HSYNC)
#define OMV_CSI_VSYNC_PIN               (&omv_pin_DCMI_VSYNC)
#define OMV_CSI_PXCLK_PIN               (&omv_pin_DCMI_PXCLK)
#define OMV_CSI_MXCLK_PIN               (&omv_pin_DCMI_MCLK)
#define OMV_CSI_RESET_PIN               (&omv_pin_DCMI_RESET)
#define OMV_CSI_POWER_PIN               (&omv_pin_DCMI_POWER)
#define OMV_CSI_FSYNC_PIN               (&omv_pin_DCMI_FSYNC)

#endif //__OMV_BOARDCONFIG_H__
