/*
 * RTL865x ASIC Register Definitions
 * Copyright (c) 2002 Realtek Semiconductor Corporation
 * Author: davidhsu
 * Adapted for Linux 5.10 & RTL8196E: Jacques Nilo (2025)
 *
 * Hardware register addresses and bit field definitions.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

/*********************************************************************
 **                                                                 **
 **    Common Parts -- Add Common Definitions Here !                **
 **                                                                 **
 *********************************************************************/
#define UNCACHED_ADDRESS(x) ((void *)(0x20000000 | (uint32)x))
#define CACHED_ADDRESS(x) ((void *)(~0x20000000 & (uint32)x))
#define PHYSICAL_ADDRESS(x) (((uint32)x) & 0x1fffffff)
#define KSEG0_ADDRESS(x) ((void *)(PHYSICAL_ADDRESS(x) | 0x80000000))
#define KSEG1_ADDRESS(x) ((void *)(PHYSICAL_ADDRESS(x) | 0xA0000000))

/*********************************************************************
 **                                                                 **
 ** IC-Dependent Part --Add in the Specific Definitions in its file **
 **                                                                 **
 *********************************************************************/
#include "rtl865xc_asicregs.h"
