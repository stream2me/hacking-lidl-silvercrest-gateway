/*
 * boot/types.h - Type definitions for RTL8196E bootloader
 *
 * Provides basic types. Architecture-specific types come from asm/types.h
 */
#ifndef _BOOT_TYPES_H
#define _BOOT_TYPES_H

/* Include architecture-specific type definitions */
#include <asm/types.h>

/* Size types (if not already defined) */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef signed int ssize_t;
#endif

/* C99-style fixed-width integer types (if not already defined) */
#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__
typedef u8 uint8_t;
typedef s8 int8_t;
typedef u16 uint16_t;
typedef s16 int16_t;
typedef u32 uint32_t;
typedef s32 int32_t;
typedef u64 uint64_t;
typedef s64 int64_t;
#endif

#endif /* _BOOT_TYPES_H */
