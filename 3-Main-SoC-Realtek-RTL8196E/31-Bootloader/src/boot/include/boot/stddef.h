/*
 * boot/stddef.h - Standard definitions for RTL8196E bootloader
 */
#ifndef _BOOT_STDDEF_H
#define _BOOT_STDDEF_H

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif /* _BOOT_STDDEF_H */
