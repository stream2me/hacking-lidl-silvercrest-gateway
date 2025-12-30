/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org>
 */
#include <boot/types.h>
#include <boot/init.h>
#include <boot/interrupt.h>
#include <boot/string.h>

#include <asm/mipsregs.h>
#include <asm/system.h>

extern int dprintf(char *fmt, ...);
#ifndef prom_printf
#define prom_printf dprintf
#endif
#include <rtl8196x/asicregs.h>
#include "main.h"
void
invalidate_iram()
{
	__asm__ volatile ("mtc0	 $0, $20\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "li 			 $8,0x00000020\n\t"
			  "mtc0	 $8, $20\n\t" "nop\n\t" "nop\n\t");
}

void
start_kernel(void)
{
	int ret;

	IMG_HEADER_T header;
	SETTING_HEADER_T setting_header;
//-------------------------------------------------------
	setClkInitConsole();

	initHeap();

	initInterrupt();

	initFlash();
	Init_GPIO();

	showBoardInfo();

	if ((REG32(BOND_OPTION) & BOND_ID_MASK) == BOND_8196ES) {
		rtl8196e_gpio_init();
	}

#ifdef SUPPORT_TFTP_CLIENT
	extern volatile unsigned int last_sent_time;
	extern unsigned int tftp_from_command;
	extern int retry_cnt;
	retry_cnt = 0;
	tftp_from_command = 0;
	last_sent_time = 0;
	eth_startup(0);
	sti();
	tftpd_entry(1);
#endif

	return_addr = 0;
	ret = check_image(&header, &setting_header);

	invalidate_iram();
	doBooting(ret, return_addr, &header);
}

//-------------------------------------------------------
//show board info
void
showBoardInfo(void)
{
	volatile int cpu_speed = 0;

#define SYS_ECO_NO 0xb8000000
#define REG32(reg)  (*(volatile unsigned int *)(reg))
	if (REG32(SYS_ECO_NO) == 0x8196e000)
		SettingCPUClk(0, 2, 0);	//CPU Speed to 400MHz
	cpu_speed = check_cpu_speed();

	/* Compact boot banner */
	prom_printf("\n");
	prom_printf("RTL8196E Bootloader %s (%s)\n", B_VERSION, BOOT_CODE_TIME);
	prom_printf("DDR1 32MB | CPU %dMHz\n", cpu_speed);

	/* Watchdog reboot warning */
	if ((*(volatile unsigned int *)(0xb8000008)) & (0x1 << 23))
		prom_printf("! Watchdog reboot detected\n");
}
