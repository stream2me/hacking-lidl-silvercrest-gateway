/*
 * init.c: early initialisation code for R39XX Class PDAs
 *
 * Copyright (C) 1999 Harald Koerfgen
 *
 * $Id: init.c,v 1.1 2009/11/13 13:22:47 jasonwang Exp $
 */
#include <boot/types.h>
#include <boot/init.h>
#include <boot/interrupt.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/io.h>

#include <asm/rtl8196.h>

//#include <linux/init.h>
//#include <linux/kernel.h>
//#include <asm/rtl8181.h>
//#include <asm/io.h>

void
serial_outc(char c)
{
	int i;
	i = 0;

	while (1) {
		i++;
		if (i >= 6540)
			break;

		if (rtl_inb(UART_LSR) & 0x60)
			break;
	}

	//for(i=0; i<0xff00;i++);
	rtl_outb(UART_THR, c);

	if (c == 0x0a)
		rtl_outb(UART_THR, 0x0d);
	// ----------------------------------------------------
	// above is UART0, and below is SC16IS7x0 
	// ----------------------------------------------------

}

char
serial_inc()
{
	int i;
	while (1) {
		if (rtl_inb(UART_LSR) & 0x1)
			break;
	}
	i = rtl_inb(UART_RBR);
	return (i & 0xff);
	// ----------------------------------------------------
	// above is UART0, and below is SC16IS7x0 
	// ----------------------------------------------------

}

int
isspace(int ch)
{
	return (unsigned int)(ch - 9) < 5u || ch == ' ';
}

/*
 * Helpful for debugging :-)
 */
