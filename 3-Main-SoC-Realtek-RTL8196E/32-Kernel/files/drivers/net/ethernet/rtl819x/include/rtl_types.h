/*
 * RTL865x Type Definitions
 * Copyright (c) 2002 Realtek Semiconductor Corporation
 * Adapted for Linux 5.10 & RTL8196E: Jacques Nilo (2025)
 *
 * Basic type definitions and macros for RTL8196E driver.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _RTL_TYPES_H
#define _RTL_TYPES_H

#define RTL_LAYERED_DRIVER_DEBUG 0

/*
 * Internal names for basic integral types.
 * Omit the typedef if not possible for a machine/compiler combination.
 */
#include <linux/version.h>
#include <linux/module.h>

/* ===============================================================================
 * Print Macro
 * =============================================================================== */
#define rtlglue_printf printk  /* Kernel 5.4: panic_printk doesn't exist */

/* ===============================================================================
 * Type Definitions
 * =============================================================================== */
typedef unsigned long long  uint64;
typedef long long           int64;
typedef unsigned int        uint32;

#ifdef int32
#undef int32
#endif
typedef int                 int32;

typedef unsigned short      uint16;
typedef short               int16;
typedef unsigned char       uint8;
typedef char                int8;

typedef uint32              memaddr;
typedef uint32              ipaddr_t;

typedef struct {
    uint16 mac47_32;
    uint16 mac31_16;
    uint16 mac15_0;
} macaddr_t;

#define ETHER_ADDR_LEN      6

typedef struct ether_addr_s {
    uint8 octet[ETHER_ADDR_LEN];
} ether_addr_t;

/* ===============================================================================
 * Constants and Configuration
 * =============================================================================== */
#define RX_OFFSET           2
#define MBUF_LEN            1700
#define CROSS_LAN_MBUF_LEN  (MBUF_LEN + RX_OFFSET + 10)

#define DELAY_REFILL_ETH_RX_BUF      1
#define PRIV_BUF_CAN_USE_KERNEL_BUF  1
#define INIT_RX_RING_ERR_HANDLE      1
/* #define ALLOW_RX_RING_PARTIAL_EMPTY  1 */

/* 
 * CN SD6 Mantis issue #1085: NIC RX can't work correctly after runout.
 * This bug still happened in RTL8196B
 */

/* ===============================================================================
 * Common Definitions
 * =============================================================================== */
#ifndef NULL
#define NULL                0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef SUCCESS
#define SUCCESS             0
#endif

#ifndef FAILED
#define FAILED             -1
#endif

#ifndef OK
#define OK                  0
#endif

#ifndef NOT_OK
#define NOT_OK             1
#endif

#define DEBUG_P(args...)    while(0)

/* ===============================================================================
 * Bit Operations
 * =============================================================================== */
#ifndef CLEARBITS
#define CLEARBITS(a, b)     ((a) &= ~(b))
#endif

#ifndef SETBITS
#define SETBITS(a, b)       ((a) |= (b))
#endif

#ifndef ISSET
#define ISSET(a, b)         (((a) & (b)) != 0)
#endif

#ifndef ISCLEARED
#define ISCLEARED(a, b)     (((a) & (b)) == 0)
#endif

/* ===============================================================================
 * Math Utilities
 * =============================================================================== */
#ifndef max
#define max(a, b)           (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b)           (((a) < (b)) ? (a) : (b))
#endif

// Round down x to multiple of y. Ex: ROUNDDOWN(20, 7) = 14
#ifndef ROUNDDOWN
#define ROUNDDOWN(x, y)     (((x) / (y)) * (y))
#endif

// Round up x to multiple of y. Ex: ROUNDUP(11, 7) = 14
#ifndef ROUNDUP
#define ROUNDUP(x, y)       ((((x) + ((y) - 1)) / (y)) * (y))  /* to any y */
#endif

#ifndef ROUNDUP2
#define ROUNDUP2(x, y)      (((x) + ((y) - 1)) & (~((y) - 1))) /* if y is powers of two */
#endif

#ifndef ROUNDUP4
#define ROUNDUP4(x)         ((1 + (((x) - 1) >> 2)) << 2)
#endif

#ifndef IS4BYTEALIGNED
#define IS4BYTEALIGNED(x)   ((((x) & 0x3) == 0) ? 1 : 0)
#endif

/* ===============================================================================
 * Structure Utilities
 * =============================================================================== */
#ifndef __offsetof
#define __offsetof(type, field)     ((unsigned long)(&((type *)0)->field))
#endif

#ifndef offsetof
#define offsetof(type, field)       __offsetof(type, field)
#endif

/* ===============================================================================
 * Debug and Assert Macros
 * =============================================================================== */
#ifndef RTL_PROC_CHECK
#define RTL_PROC_CHECK(expr, success) \
    do { \
        int __retval; \
        if ((__retval = (expr)) != (success)) { \
            rtlglue_printf("ERROR >>> [%s]:[%d] failed -- return value: %d\n", \
                          __FUNCTION__, __LINE__, __retval); \
            return __retval; \
        } \
    } while(0)
#endif

#ifndef RTL_STREAM_SAME
#define RTL_STREAM_SAME(s1, s2) \
    ((strlen(s1) == strlen(s2)) && (strcmp(s1, s2) == 0))
#endif

#define ASSERT_ISR(x)       if (!(x)) { while(1); }
#define RTL_STATIC_INLINE   static __inline__

#define ASSERT_CSP(x)       if (!(x)) { \
                            rtlglue_printf("\nAssert Fail: %s %d", __FILE__, __LINE__); \
                            while(1); \
                        }

/* ===============================================================================
 * Cache Configuration
 * =============================================================================== */
#if defined(RTL865X_TEST) || defined(RTL865X_MODEL_USER)
#define UNCACHE_MASK        0
#define UNCACHE(addr)       (addr)
#define CACHED(addr)        ((uint32)(addr))
#else
#define UNCACHE_MASK        0x20000000
#define UNCACHE(addr)       ((UNCACHE_MASK) | (uint32)(addr))
#define CACHED(addr)        ((uint32)(addr) & ~(UNCACHE_MASK))
#endif

/* ===============================================================================
 * ASIC Configuration
 * =============================================================================== */
#define RTL8651_OUTPUTQUEUE_SIZE        6
#define TOTAL_VLAN_PRIORITY_NUM         8
#define RTL8651_RATELIMITTBL_SIZE       32

#define CONFIG_RTL_8197D_DYN_THR        1
#define DYN_THR_LINK_UP_PORTS           3

/* IC default values */
#define DYN_THR_DEF_fcON                0xac
#define DYN_THR_DEF_fcOFF               0xa0
#define DYN_THR_DEF_sharedON            0x62
#define DYN_THR_DEF_sharedOFF           0x4a

/* Aggressive values */
#define DYN_THR_AGG_fcON                0xd0
#define DYN_THR_AGG_fcOFF               0xa0  // 0xc0
#define DYN_THR_AGG_sharedON            0x88  // 0xc0
#define DYN_THR_AGG_sharedOFF           0x70  // 0xa8

/* ===============================================================================
 * Logging Macros (Currently Disabled)
 * =============================================================================== */
#define LOG_ERROR(fmt, args...)
#define LOG_MEM_ERROR(fmt, args...)
#define LOG_SKB_ERROR(fmt, args...)
#define LOG_WARN(fmt, args...)
#define LOG_INFO(fmt, args...)

#endif /* _RTL_TYPES_H */
