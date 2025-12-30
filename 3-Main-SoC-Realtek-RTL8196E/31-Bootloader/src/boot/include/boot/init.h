/*
 * boot/init.h - Initialization macros for RTL8196E bootloader
 *
 * In a bootloader context, these are mostly no-ops since we don't
 * have the kernel's section-based init/exit mechanism.
 */
#ifndef _BOOT_INIT_H
#define _BOOT_INIT_H

#include <boot/config.h>

/* Section markers - bootloader uses them for code organization */
#define __init      __attribute__ ((__section__ (".text.init")))
#define __exit      __attribute__ ((unused, __section__(".text.exit")))
#define __initdata  __attribute__ ((__section__ (".data.init")))
#define __exitdata  __attribute__ ((unused, __section__ (".data.exit")))

/* Assembly section markers */
#ifdef __ASSEMBLY__
#define __INIT      .section ".text.init","ax"
#define __FINIT     .previous
#define __INITDATA  .section ".data.init","aw"
#endif

/* Unused in bootloader but provided for compatibility */
#define __devinit   __init
#define __devexit   __exit
#define __devinitdata __initdata
#define __devexitdata __exitdata

#endif /* _BOOT_INIT_H */
