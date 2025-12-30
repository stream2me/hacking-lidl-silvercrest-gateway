/*
 * boot/interrupt.h - IRQ handling for RTL8196E bootloader
 */
#ifndef _BOOT_INTERRUPT_H
#define _BOOT_INTERRUPT_H

#include <boot/config.h>
#include <asm/ptrace.h>

/* Number of IRQs for RTL8196E */
#ifndef NR_IRQS
#define NR_IRQS 64
#endif

/* CPU ID for single-processor bootloader */
#ifndef smp_processor_id
#define smp_processor_id() 0
#endif

#ifndef NR_CPUS
#define NR_CPUS 1
#endif

/* IRQ action structure */
struct irqaction {
	void (*handler)(int, void *, struct pt_regs *);
	unsigned long flags;
	unsigned long mask;
	const char *name;
	void *dev_id;
	struct irqaction *next;
};

/* IRQ registration */
extern int request_IRQ(unsigned long irq, struct irqaction *action,
		       void *dev_id);

#endif /* _BOOT_INTERRUPT_H */
