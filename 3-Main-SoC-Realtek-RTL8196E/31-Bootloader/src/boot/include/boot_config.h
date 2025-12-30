/*
 * Fixed bootloader configuration (formerly autoconf/autoconf2).
 * Keep this small and explicit for readability.
 */
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

/* Target platform */
#define CONFIG_RTL8196E 1
#define RTL8196 1

/* Flash and memory */
#define CONFIG_SPI_FLASH 1
#define CONFIG_BOOT_SIO_8198 1
#define CONFIG_AUTO_PROBE_LIMITED_SPI_CLK_UNDER_40MHZ 1
#define CONFIG_DDR1_SDRAM 1
#define CONFIG_D32_16 1
#define CONFIG_SW_100M 1
#define CONFIG_SPI_FLASH_NUMBER 1
#define CONFIG_FLASH_NUMBER 0x1
#define CONFIG_RTL_FLASH_MAPPING_ENABLE 1

/* Image layout */
#define CONFIG_LINUX_IMAGE_OFFSET_START 0x20000
#define CONFIG_LINUX_IMAGE_OFFSET_END 0x200000
#define CONFIG_LINUX_IMAGE_OFFSET_STEP 0x10000
#define CONFIG_ROOT_IMAGE_OFFSET_START 0x200000
#define CONFIG_ROOT_IMAGE_OFFSET_END 0x400000
#define CONFIG_ROOT_IMAGE_OFFSET_STEP 0x10000

/* Boot features */
#define CONFIG_LZMA_ENABLE 1
#define CONFIG_BOOT_DEBUG_ENABLE 1
#define CONFIG_BOOT_RESET_ENABLE 1

#endif /* BOOT_CONFIG_H */
