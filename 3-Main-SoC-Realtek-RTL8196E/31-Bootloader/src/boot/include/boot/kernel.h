/*
 * boot/kernel.h - Kernel-like definitions for RTL8196E bootloader
 */
#ifndef _BOOT_KERNEL_H
#define _BOOT_KERNEL_H

#include <stdarg.h>
#include <boot/linkage.h>
#include <boot/stddef.h>

/* Optimization barrier */
#define barrier() __asm__ __volatile__("": : :"memory")

/* Integer limits */
#define INT_MAX     ((int)(~0U>>1))
#define INT_MIN     (-INT_MAX - 1)
#define UINT_MAX    (~0U)
#define LONG_MAX    ((long)(~0UL>>1))
#define LONG_MIN    (-LONG_MAX - 1)
#define ULONG_MAX   (~0UL)

/* Array size helper */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Log level prefixes */
#define KERN_EMERG   "<0>"
#define KERN_ALERT   "<1>"
#define KERN_CRIT    "<2>"
#define KERN_ERR     "<3>"
#define KERN_WARNING "<4>"
#define KERN_NOTICE  "<5>"
#define KERN_INFO    "<6>"
#define KERN_DEBUG   "<7>"

/* Function attributes */
#define NORET_TYPE
#define ATTRIB_NORET  __attribute__((noreturn))
#define NORET_AND     noreturn,

/* Printk - provided by bootloader */
asmlinkage int printk(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

/* IP address display helpers */
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]

#endif /* _BOOT_KERNEL_H */
