/*
 * RTL865x OS Abstraction Layer
 * Copyright (c) 2002 Realtek Semiconductor Corporation
 * Author: Edward Jin-Ru Chen
 * Adapted for Linux 5.10 & RTL8196E: Jacques Nilo (2025)
 *
 * OS abstraction macros and utility functions.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef RTL_GLUE_H
#define RTL_GLUE_H

/*	@doc RTLGLUE_API

        @module rtl_glue.h - Glue interface for Realtek 8651 Home gateway controller driver	|
        This guide documents the glue interface for porting 8651 driver to targeted operating system
        @normal Chun-Feng Liu (cfliu@realtek.com.tw) <date>

        Copyright <cp>2003 Realtek<tm> Semiconductor Cooperation, All Rights Reserved.

        @head3 List of Symbols |
        Here is the list of all functions and variables in this module.

        @index | RTLGLUE_API
*/
/* Kernel 5.4 required includes */
#include <linux/slab.h>      /* for kmalloc(), kfree() */
#include <linux/netdevice.h> /* for dev_kfree_skb_any() */
#include <linux/semaphore.h>
#include <linux/spinlock.h>

/* Kernel 5.4: DECLARE_MUTEX removed, use DEFINE_SEMAPHORE instead */
#define RTL_DECLARE_MUTEX(name) DEFINE_SEMAPHORE(name)

#ifndef RTL865X_DEBUG
#define assert(expr) \
        do           \
        {            \
        } while (0)
#else
#define assert(expr)                                           \
        if (!(expr))                                           \
        {                                                      \
                printk("\033[33;41m%s:%d: assert(%s)\033[m\n", \
                       __FILE__, __LINE__, #expr);             \
        }
#endif

#define RTL_BUG(cause)                                 \
        do                                             \
        {                                              \
                printk(" [= !! BUG !! =] at %s line %d\n\t=> Cause: \
        %s\n\t=>-- system Halt\n",                     \
                       __FUNCTION__, __LINE__, cause); \
                while (1)                              \
                        ;                              \
        } while (0)

#define TBL_MEM_ALLOC(tbl, type, size)                                      \
        {                                                                   \
                (tbl) = (type *)kmalloc((size) * sizeof(type), GFP_ATOMIC); \
                if (!(tbl))                                                 \
                {                                                           \
                        printk("MEM alloc failed at line %d\n", __LINE__);  \
                        while (1)                                           \
                                ;                                           \
                        return FAILED;                                      \
                }                                                           \
        }

#ifndef bzero
#define bzero(p, s) memset(p, 0, s)
#endif

#define RTL_NIC_LOCK_INIT(__rtl_lock__) spin_lock_init(&__rtl_lock__)
#define RTL_NIC_LOCK(__rtl_lock__) spin_lock(&__rtl_lock__)
#define RTL_NIC_UNLOCK(__rtl_lock__) spin_unlock(&__rtl_lock__)

static inline int rtl_down_interruptible(struct semaphore *sem)
{
        return down_interruptible(sem);
}

static inline void rtl_up(struct semaphore *sem)
{
        up(sem);
}

static inline void *rtl_malloc(size_t NBYTES)
{
        if (NBYTES == 0)
                return NULL;
        return (void *)kmalloc(NBYTES, GFP_ATOMIC);
}

static inline void rtl_free(void *APTR)
{
        kfree(APTR);
}

/* ========================================
 * Function Declarations (compiler-driven)
 * ======================================== */

#endif
