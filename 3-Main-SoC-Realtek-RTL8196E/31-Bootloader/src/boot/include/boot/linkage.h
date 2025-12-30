/*
 * boot/linkage.h - Linkage definitions for RTL8196E bootloader
 */
#ifndef _BOOT_LINKAGE_H
#define _BOOT_LINKAGE_H

#ifdef __cplusplus
#define CPP_ASMLINKAGE extern "C"
#else
#define CPP_ASMLINKAGE
#endif

/* MIPS doesn't need special calling convention */
#define asmlinkage CPP_ASMLINKAGE

/* Assembly symbol helpers */
#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME(X) X

#ifdef __STDC__
#define SYMBOL_NAME_LABEL(X) X##:
#else
#define SYMBOL_NAME_LABEL(X) X/**/:
#endif

/* MIPS alignment */
#define __ALIGN .align 2
#define __ALIGN_STR ".align 2"

#ifdef __ASSEMBLY__

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#define ENTRY(name) \
  .globl SYMBOL_NAME(name); \
  ALIGN; \
  SYMBOL_NAME_LABEL(name)

#endif /* __ASSEMBLY__ */

#endif /* _BOOT_LINKAGE_H */
