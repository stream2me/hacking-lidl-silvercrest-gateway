
#include <boot/interrupt.h>
#include <asm/system.h>
#include <rtl_types.h>
#include <boot/string.h>
#include <stdlib.h>
#include <rtl8196x/swCore.h>

#include "monitor.h"
#include "etherboot.h"
#include "nic.h"

#include <asm/mipsregs.h>	//wei add
#include <asm/delay.h>

#include "spi_flash.h"

#include <rtl8196x/asicregs.h>

#include <asm/rtl8196.h>

#define SWITCH_CMD 1

#ifndef SYS_BASE
#define SYS_BASE 0xb8000000
#endif
#define SYS_INI_STATUS (SYS_BASE +0x04)
#ifndef SYS_HW_STRAP
#define SYS_HW_STRAP (SYS_BASE +0x08)
#endif
#define SYS_CLKMANAGE (SYS_BASE +0x10)
//hw strap
#define ST_SYNC_OCP_OFFSET 9
#ifndef CK_M2X_FREQ_SEL_OFFSET
#define CK_M2X_FREQ_SEL_OFFSET 10
#endif
#ifndef ST_CPU_FREQ_SEL_OFFSET
#define ST_CPU_FREQ_SEL_OFFSET 13
#endif
#ifndef ST_CPU_FREQDIV_SEL_OFFSET
#define ST_CPU_FREQDIV_SEL_OFFSET 19
#endif
#define ST_BOOTPINSEL (1<<0)
#ifndef ST_DRAMTYPE
#define ST_DRAMTYPE (1<<1)
#endif
#ifndef ST_BOOTSEL
#define ST_BOOTSEL (1<<2)
#endif
#ifndef ST_PHYID
#define ST_PHYID (0x3<<3)	//11b
#endif
#define ST_EN_EXT_RST (1<<8)
#define ST_SYNC_OCP (1<<9)
#ifndef CK_M2X_FREQ_SEL
#define CK_M2X_FREQ_SEL (0x7 <<10)
#endif
#ifndef ST_CPU_FREQ_SEL
#define ST_CPU_FREQ_SEL (0xf<<13)
#endif
#ifndef ST_NRFRST_TYPE
#define ST_NRFRST_TYPE (1<<17)
#endif
#define SYNC_LX (1<<18)
#undef ST_CPU_FREQDIV_SEL
#define ST_CPU_FREQDIV_SEL (0x7<<19)
#ifndef ST_EVER_REBOOT_ONCE
#define ST_EVER_REBOOT_ONCE (1<<23)
#endif
#define ST_SYS_DBG_SEL  (0x3f<<24)
#define ST_PINBUS_DBG_SEL (3<<30)
#ifndef SPEED_IRQ_NO
#define SPEED_IRQ_NO 29
#endif
#ifndef SPEED_IRR_NO
#define SPEED_IRR_NO 3
#endif
#ifndef SPEED_IRR_OFFSET
#define SPEED_IRR_OFFSET 20
#endif
#define MAIN_PROMPT						"<RealTek>"
#define putchar(x)	serial_outc(x)
#define IPTOUL(a,b,c,d)	((a << 24)| (b << 16) | (c << 8) | d )

extern unsigned int _end;
extern unsigned char ethfile[20];
extern struct arptable_t arptable[MAX_ARP];

int YesOrNo(void);
int CmdHelp(int argc, char *argv[]);

int CmdDumpWord(int argc, char *argv[]);
int CmdDumpByte(int argc, char *argv[]);	//wei add
int CmdWriteWord(int argc, char *argv[]);
int CmdWriteByte(int argc, char *argv[]);
int CmdWriteHword(int argc, char *argv[]);
int CmdWriteAll(int argc, char *argv[]);
int CmdCmp(int argc, char *argv[]);
int CmdIp(int argc, char *argv[]);
int CmdAuto(int argc, char *argv[]);
int CmdLoad(int argc, char *argv[]);

int CmdCfn(int argc, char *argv[]);

int CmdSFlw(int argc, char *argv[]);
int CmdFlr(int argc, char *argv[]);
extern void auto_spi_memtest_8198(unsigned long DRAM_starting_addr,
				  unsigned int spi_clock_div_num);

#if defined(SUPPORT_TFTP_CLIENT)
int CmdTFTPC(int argc, char *argv[]);
int check_tftp_client_state();
#endif

//int CmdTimer(int argc, char* argv[]);
//int CmdMTC0SR(int argc, char* argv[]);  //wei add
//int CmdMFC0SR(int argc, char* argv[]);  //wei add
//int CmdTFTP(int argc, char* argv[]);  //wei add
//Ziv
#ifdef WRAPPER

int CmdSWB(int argc, char *argv[]);

extern char _bootimg_start, _bootimg_end;
#endif

int CmdCPUSleep(int argc, char *argv[]);
void CmdCPUSleepIMEM(void);

#if SWITCH_CMD
int TestCmd_MDIOR(int argc, char *argv[]);	//wei add
int TestCmd_MDIOW(int argc, char *argv[]);	//wei add
int CmdPHYregR(int argc, char *argv[]);
int CmdPHYregW(int argc, char *argv[]);
#endif

int CmdPortP1Patch(int argc, char *argv[]);

/*Cyrus Tsai*/
/*move to ehterboot.h
#define TFTP_SERVER 0
#define TFTP_CLIENT 1
*/
extern struct arptable_t arptable_tftp[3];
/*Cyrus Tsai*/

//extern int flasherase(unsigned long src, unsigned int length);
//extern int flashwrite(unsigned long dst, unsigned long src, unsigned long length);
//extern int flashread (unsigned long dst, unsigned long src, unsigned long length);

extern int write_data(unsigned long dst, unsigned long length,
		      unsigned char *target);
extern int read_data(unsigned long src, unsigned long length,
		     unsigned char *target);

/*Cyrus Tsai*/
extern unsigned long file_length_to_server;
extern unsigned long file_length_to_client;
extern unsigned long image_address;
/*this is the file length, should extern to flash driver*/
/*Cyrus Tsai*/

#define WRITE_MEM32(addr, val)   (*(volatile unsigned int *) (addr)) = (val)
#define WRITE_MEM16(addr, val)   (*(volatile unsigned short *) (addr)) = (val)
#define READ_MEM32(addr)         (*(volatile unsigned int *) (addr))

#ifndef SWCORE_BASE
#define SWCORE_BASE      0xBB800000
#endif
#define PCRAM_BASE       (0x4100+SWCORE_BASE)
#define PITCR                  (0x000+PCRAM_BASE)	/* Port Interface Type Control Register */
#define PCRP0                 (0x004+PCRAM_BASE)	/* Port Configuration Register of Port 0 */
#define PCRP1                 (0x008+PCRAM_BASE)	/* Port Configuration Register of Port 1 */
#define PCRP2                 (0x00C+PCRAM_BASE)	/* Port Configuration Register of Port 2 */
#define PCRP3                 (0x010+PCRAM_BASE)	/* Port Configuration Register of Port 3 */
#define PCRP4                 (0x014+PCRAM_BASE)	/* Port Configuration Register of Port 4 */
#define EnablePHYIf        (1<<0)	/* Enable PHY interface.                    */

COMMAND_TABLE MainCmdTable[] = {
	{"?", 0, CmdHelp,
	 "HELP (?)				    : Print this help message"},
	{"DB", 2, CmdDumpByte, "DB <Address> <Len>"},	//wei add      
	{"DW", 2, CmdDumpWord, "DW <Address> <Len>"},	//same command with ICE, easy use
	{"EB", 2, CmdWriteByte, "EB <Address> <Value1> <Value2>..."},
	{"EW", 2, CmdWriteWord, "EW <Address> <Value1> <Value2>..."},
	{"CMP", 3, CmdCmp, "CMP: CMP <dst><src><length>"},
	{"IPCONFIG", 2, CmdIp, "IPCONFIG:<TargetAddress>"},
	{"AUTOBURN", 1, CmdAuto, "AUTOBURN: 0/1"},
	{"LOADADDR", 1, CmdLoad, "LOADADDR: <Load Address>"},
	{"J", 1, CmdCfn, "J: Jump to <TargetAddress>"},

	{"FLR", 3, CmdFlr, "FLR: FLR <dst><src><length>"},
	{"FLW", 4, CmdSFlw, "FLW <dst_ROM_offset><src_RAM_addr><length_Byte> <SPI cnt#>: Write offset-data to SPI from RAM"},	//JSW
#ifdef WRAPPER
	{"SWB", 1, CmdSWB, "SWB <SPI cnt#> (<0>=1st_chip,<1>=2nd_chip): SPI Flash WriteBack (for MXIC/Spansion)"},	//JSW   
#endif

#if defined(SUPPORT_TFTP_CLIENT)
	{"TFTP", 2, CmdTFTPC, "tftp <memoryaddress> <filename>  "},
#endif

#if SWITCH_CMD
	{"MDIOR", 0, TestCmd_MDIOR, "MDIOR:  MDIOR <phyid> <reg>"},	//wei add,   
	{"MDIOW", 0, TestCmd_MDIOW, "MDIOW:  MDIOW <phyid> <reg> <data>"},	//wei add,   
	{"PHYR", 2, CmdPHYregR, "PHYR: PHYR <PHYID><reg>"},
	{"PHYW", 3, CmdPHYregW, "PHYW: PHYW <PHYID><reg><data>"},
#endif
	{"PORT1", 3, CmdPortP1Patch, "PORT1: port 1 patch for FT2"},

};

//------------------------------------------------------------------------------
/********   caculate CPU clock   ************/
int check_cpu_speed(void);
void timer_init(unsigned long lexra_clock);
static void timer_interrupt(int num, void *ptr, struct pt_regs *reg);
struct irqaction irq_timer = { timer_interrupt, 0, 8, "timer", NULL, NULL };

static volatile unsigned int jiffies = 0;
static void
timer_interrupt(int num, void *ptr, struct pt_regs *reg)
{
	//dprintf("jiffies=%x\r\n",jiffies);
	//flush_WBcache();
	rtl_outl(TCIR, rtl_inl(TCIR));
	jiffies++;

}

volatile int
get_timer_jiffies(void)
{

	return jiffies;
};

//------------------------------------------------------------------------------
void
timer_init(unsigned long lexra_clock)
{
	/* Set timer mode and Enable timer */
	REG32(TCCNR_REG) = (0 << 31) | (0 << 30);	//using time0
	//REG32(TCCNR_REG) = (1<<31) | (0<<30);     //using counter0

#define DIVISOR     0xE
#define DIVF_OFFSET                         16
	REG32(CDBR_REG) = (DIVISOR) << DIVF_OFFSET;

	/* Set timeout per msec */

	int SysClkRate = lexra_clock;	/* CPU 200MHz */

#define TICK_10MS_FREQ  100	/* 100 Hz */
#define TICK_100MS_FREQ 1000	/* 1000 Hz */
#define TICK_FREQ       TICK_10MS_FREQ

	REG32(TC0DATA_REG) = (((SysClkRate / DIVISOR) / TICK_FREQ) + 1) << 4;

	/* Set timer mode and Enable timer */
	REG32(TCCNR_REG) = (1 << 31) | (1 << 30);	//using time0
	/* We must wait n cycles for timer to re-latch the new value of TC1DATA. */
	int c;
	for (c = 0; c < DIVISOR; c++) ;

	/* Set interrupt mask register */
	//REG32(GIMR_REG) |= (1<<8);        //request_irq() will set 

	/* Set interrupt routing register */
	// RTL8196E
	REG32(IRR1_REG) = 0x00050004;	//uart:IRQ5,  time0:IRQ4

	/* Enable timer interrupt */
	REG32(TCIR_REG) = (1 << 31);
}

//------------------------------------------------------------------------------

#ifndef _ASM_DELAY_H
__inline__ void
__delay(unsigned long loops)
{
	__asm__ __volatile__(".set\tnoreorder\n"
			     "1:\tbnez\t%0,1b\n\t"
			     "subu\t%0,1\n\t" ".set\treorder":"=r"(loops)
			     :"0"(loops));
}
#endif

/*
80007988 <__delay>:                                             
80007988:	1480ffff 	bnez	a0,80007988 <__delay>           
8000798c:	2484ffff 	addiu	a0,a0,-1                        
80007990:	03e00008 	jr	ra                                  
*/

//---------------------------------------------------------------------------
unsigned long loops_per_jiffy = (1 << 12);
#define LPS_PREC 8
#define HZ 100
unsigned long loops_per_sec = 2490368 * HZ;	// @CPU 500MHz (this will be update in check_cpu_speed())

int
check_cpu_speed(void)
{
	unsigned long ticks, loopbit;
	int lps_precision = LPS_PREC;

	// RTL8196E
	request_IRQ(8, &irq_timer, NULL);

	extern long glexra_clock;
	timer_init(glexra_clock);

	loops_per_jiffy = (1 << 12);
	while (loops_per_jiffy <<= 1) {
		/* wait for "start of" clock tick */
		ticks = jiffies;
		while (ticks == jiffies)
			/* nothing */ ;
		/* Go .. */
		ticks = jiffies;
		__delay(loops_per_jiffy);
		ticks = jiffies - ticks;
		if (ticks)
			break;
	}
/* Do a binary approximation to get loops_per_jiffy set to equal one clock
   (up to lps_precision bits) */
	loops_per_jiffy >>= 1;
	loopbit = loops_per_jiffy;
	while (lps_precision-- && (loopbit >>= 1)) {
		loops_per_jiffy |= loopbit;
		ticks = jiffies;
		while (ticks == jiffies) ;
		ticks = jiffies;
		__delay(loops_per_jiffy);
		if (jiffies != ticks)	/* longer than 1 tick */
			loops_per_jiffy &= ~loopbit;
	}

	//timer_stop(); //wei del, because not close timer
	//free_IRQ(8);
/* Round the value and print it */
	//prom_printf("cpu run %d.%d MIPS\n", loops_per_jiffy/(500000/HZ),      (loops_per_jiffy/(5000/HZ)) % 100);
	return ((loops_per_jiffy / (500000 / HZ)) + 1);

}

//---------------------------------------------------------------------------

/*
---------------------------------------------------------------------------
;				Monitor
---------------------------------------------------------------------------
*/
extern char **GetArgv(const char *string);

void
monitor(void)
{
	char buffer[MAX_MONITOR_BUFFER + 1];
	int argc;
	char **argv;
	int i, retval;

//      i = &_end;
//      i = (i & (~4095)) + 4096;
	//printf("Free Mem Start=%X\n", i);
	while (1) {
		printf("%s", MAIN_PROMPT);
		memset(buffer, 0, MAX_MONITOR_BUFFER);
		GetLine(buffer, MAX_MONITOR_BUFFER, 1);
		printf("\n");
		argc = GetArgc((const char *)buffer);
		argv = GetArgv((const char *)buffer);
		if (argc < 1)
			continue;
		StrUpr(argv[0]);
		for (i = 0;
		     i < (sizeof (MainCmdTable) / sizeof (COMMAND_TABLE));
		     i++) {

			if (!strcmp(argv[0], MainCmdTable[i].cmd)) {
				retval =
				    MainCmdTable[i].func(argc - 1, argv + 1);
				memset(argv[0], 0, sizeof (argv[0]));
				break;
			}
		}
		if (i == sizeof (MainCmdTable) / sizeof (COMMAND_TABLE))
			printf("Unknown command !\r\n");
	}
}

//---------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------

#ifdef WRAPPER

extern char _bootimg_start, _bootimg_end;
//SPI Write-Back
int
CmdSWB(int argc, char *argv[])
{
	unsigned short auto_spi_clock_div_num;	//0~7
	unsigned int cnt = strtoul((const char *)(argv[0]), (char **)NULL, 16);	//JSW check
	char *start = &_bootimg_start;
	char *end = &_bootimg_end;
	unsigned int length = end - start;
	printf
	    ("SPI Flash #%d will write 0x%X length of embedded boot code from 0x%X to 0x%X\n",
	     cnt + 1, length, start, end);
	printf("(Y)es, (N)o->");
	if (YesOrNo()) {
		spi_pio_init();
#if defined(SUPPORT_SPI_MIO_8198_8196C)
		spi_flw_image_mio_8198(cnt, 0, (unsigned char *)start, length);
#else
		spi_flw_image(cnt, 0, (unsigned char *)start, length);
#endif
		printf("SPI Flash Burn OK!\n");
	} else {
		printf("Abort!\n");
	}
}

#endif

#if defined(SUPPORT_TFTP_CLIENT)
unsigned int tftp_from_command = 0;
char tftpfilename[128];
char errmsg[512];
unsigned short errcode = 0;
unsigned int tftp_client_recvdone = 0;
extern int retry_cnt;
extern int jump_to_test;
extern volatile unsigned int last_sent_time;
int
CmdTFTPC(int argc, char *argv[])
{
	if (argc != 2) {
		dprintf("[usage:] tftp <memroyaddress> <filename>\n");
		tftpd_entry(0);
		return 0;
	}
	unsigned int address =
	    strtoul((const char *)(argv[0]), (char **)NULL, 16);
	unsigned int len = 0;
	image_address = address;
	memset(tftpfilename, 0, 128);
	len = strlen(tftpfilename);
	if (len + 1 > 128) {
		dprintf("filename too long\n");
		return 0;
	}
	memset(errmsg, 0, 512);
	errcode = 0;
	jump_to_test = 0;
	retry_cnt = 0;
	last_sent_time = 0;
	tftp_client_recvdone = 0;
	strcpy(tftpfilename, (char *)(argv[1]));
	tftpd_entry(1);
	int tickStart = 0;
	int ret = 0;

	tftp_from_command = 1;
	tickStart = get_timer_jiffies();
	do {
		ret = pollingDownModeKeyword(ESC);
		if (ret == 1)
			break;
	}
	while ((!tftp_client_recvdone) && (check_tftp_client_state() >= 0 || (get_timer_jiffies() - tickStart) < 2000)	//20s
	    );

	if (!tftp_client_recvdone) {
		if (ret == 1)
			dprintf("canceled by user ESC\n");
		else
			dprintf("TFTP timeout\n");
	}
	tftpd_entry(0);
	retry_cnt = 0;
	tftp_from_command = 0;
	tftp_client_recvdone = 0;
	return 0;
}
#endif
/*/
---------------------------------------------------------------------------
; Ethernet Download
---------------------------------------------------------------------------
*/

extern unsigned long ETH0_ADD;
int
CmdCfn(int argc, char *argv[])
{
	unsigned long Address;
	void (*jump)(void);
	if (argc > 0) {
		if (!Hex2Val(argv[0], &Address)) {
			printf(" Invalid Address(HEX) value.\n");
			return FALSE;
		}
	}

	dprintf("---Jump to address=%X\n", Address);
	jump = (void *)(Address);
	outl(0, GIMR0);		// mask all interrupt
	cli();
	/* if the jump-Address is BFC00000, then do watchdog reset */
	if (Address == 0xBFC00000) {
		*(volatile unsigned long *)(0xB800311c) = 0;	/*this is to enable 865xc watch dog reset */
		for (;;) ;
	} else {		/*else disable PHY to prevent from ethernet disturb Linux kernel booting */
		WRITE_MEM32(PCRP0, (READ_MEM32(PCRP0) & (~EnablePHYIf)));
		WRITE_MEM32(PCRP1, (READ_MEM32(PCRP1) & (~EnablePHYIf)));
		WRITE_MEM32(PCRP2, (READ_MEM32(PCRP2) & (~EnablePHYIf)));
		WRITE_MEM32(PCRP3, (READ_MEM32(PCRP3) & (~EnablePHYIf)));
		WRITE_MEM32(PCRP4, (READ_MEM32(PCRP4) & (~EnablePHYIf)));
		flush_cache();
	}
	jump();

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
/* This command can be used to configure host ip and target ip	*/

extern char eth0_mac[6];
int
CmdIp(int argc, char *argv[])
{
	unsigned char *ptr;
	unsigned int i;
	int ip[4];

	if (argc == 0) {
		printf(" Target Address=%d.%d.%d.%d\n",
		       arptable_tftp[TFTP_SERVER].ipaddr.ip[0],
		       arptable_tftp[TFTP_SERVER].ipaddr.ip[1],
		       arptable_tftp[TFTP_SERVER].ipaddr.ip[2],
		       arptable_tftp[TFTP_SERVER].ipaddr.ip[3]);
#ifdef HTTP_SERVER
		printf("   Http Address=%d.%d.%d.%d\n",
		       arptable_tftp[HTTPD_ARPENTRY].ipaddr.ip[0],
		       arptable_tftp[HTTPD_ARPENTRY].ipaddr.ip[1],
		       arptable_tftp[HTTPD_ARPENTRY].ipaddr.ip[2],
		       arptable_tftp[HTTPD_ARPENTRY].ipaddr.ip[3]);
#endif
		return 0;
	}

	ptr = argv[0];

	for (i = 0; i < 4; i++) {
		ip[i] = strtol((const char *)ptr, (char **)NULL, 10);
		ptr = strchr(ptr, '.');
		ptr++;
	}
	arptable_tftp[TFTP_SERVER].ipaddr.ip[0] = ip[0];
	arptable_tftp[TFTP_SERVER].ipaddr.ip[1] = ip[1];
	arptable_tftp[TFTP_SERVER].ipaddr.ip[2] = ip[2];
	arptable_tftp[TFTP_SERVER].ipaddr.ip[3] = ip[3];
/*replace the MAC address middle 4 bytes.*/
	eth0_mac[1] = ip[0];
	eth0_mac[2] = ip[1];
	eth0_mac[3] = ip[2];
	eth0_mac[4] = ip[3];
	arptable_tftp[TFTP_SERVER].node[5] = eth0_mac[5];
	arptable_tftp[TFTP_SERVER].node[4] = eth0_mac[4];
	arptable_tftp[TFTP_SERVER].node[3] = eth0_mac[3];
	arptable_tftp[TFTP_SERVER].node[2] = eth0_mac[2];
	arptable_tftp[TFTP_SERVER].node[1] = eth0_mac[1];
	arptable_tftp[TFTP_SERVER].node[0] = eth0_mac[0];
	prom_printf("Now your Target IP is %d.%d.%d.%d\n", ip[0], ip[1], ip[2],
		    ip[3]);
	return 0;
}

int
CmdDumpWord(int argc, char *argv[])
{

	unsigned long src;
	unsigned int len, i;

	if (argc < 1) {
		dprintf("Wrong argument number!\r\n");
		return 0;
	}

	if (argv[0]) {
		src = strtoul((const char *)(argv[0]), (char **)NULL, 16);
		if (src < 0x80000000)
			src |= 0x80000000;
	} else {
		dprintf("Wrong argument number!\r\n");
		return 0;
	}

	if (!argv[1])
		len = 1;
	else
		len = strtoul((const char *)(argv[1]), (char **)NULL, 10);
	while ((src) & 0x03)
		src++;

	for (i = 0; i < len; i += 4, src += 16) {
		dprintf("%08X:	%08X	%08X	%08X	%08X\n",
			src, *(unsigned long *)(src),
			*(unsigned long *)(src + 4),
			*(unsigned long *)(src + 8),
			*(unsigned long *)(src + 12));
	}

	return 0;
}

//---------------------------------------------------------------------------
int
CmdDumpByte(int argc, char *argv[])
{

	unsigned long src;
	unsigned int len, i;

	if (argc < 1) {
		dprintf("Wrong argument number!\r\n");
		return 0;
	}

	src = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	if (!argv[1])
		len = 16;
	else
		len = strtoul((const char *)(argv[1]), (char **)NULL, 10);

	ddump((unsigned char *)src, len);

	return 0;
}

//---------------------------------------------------------------------------
int
CmdWriteWord(int argc, char *argv[])
{

	unsigned long src;
	unsigned int value, i;

	src = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	while ((src) & 0x03)
		src++;

	for (i = 0; i < argc - 1; i++, src += 4) {
		value = strtoul((const char *)(argv[i + 1]), (char **)NULL, 16);
		*(volatile unsigned int *)(src) = value;
	}

}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

int
CmdWriteHword(int argc, char *argv[])
{

	unsigned long src;
	unsigned short value, i;

	src = strtoul((const char *)(argv[0]), (char **)NULL, 16);

	src &= 0xfffffffe;

	for (i = 0; i < argc - 1; i++, src += 2) {
		value = strtoul((const char *)(argv[i + 1]), (char **)NULL, 16);
		*(volatile unsigned short *)(src) = value;
	}

}
//---------------------------------------------------------------------------
int
CmdWriteByte(int argc, char *argv[])
{

	unsigned long src;
	unsigned char value, i;

	src = strtoul((const char *)(argv[0]), (char **)NULL, 16);

	for (i = 0; i < argc - 1; i++, src++) {
		value = strtoul((const char *)(argv[i + 1]), (char **)NULL, 16);
		*(volatile unsigned char *)(src) = value;
	}

}

int
CmdCmp(int argc, char *argv[])
{
	int i;
	unsigned long dst, src;
	unsigned long dst_value, src_value;
	unsigned int length;
	unsigned long error;

	if (argc < 3) {
		printf("Parameters not enough!\n");
		return 1;
	}
	dst = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	src = strtoul((const char *)(argv[1]), (char **)NULL, 16);
	length = strtoul((const char *)(argv[2]), (char **)NULL, 16);
	error = 0;
	for (i = 0; i < length; i += 4) {
		dst_value = *(volatile unsigned int *)(dst + i);
		src_value = *(volatile unsigned int *)(src + i);
		if (dst_value != src_value) {
			printf("%dth data(%x %x) error\n", i, dst_value,
			       src_value);
			error = 1;
		}
	}
	if (!error)
		printf("No error found\n");

}

//---------------------------------------------------------------------------
#ifndef RTL8197B
extern int autoBurn;
int
CmdAuto(int argc, char *argv[])
{
	unsigned long addr;

	if (argv[0][0] == '0')
		autoBurn = 0;
	else
		autoBurn = 1;
	printf("AutoBurning=%d\n", autoBurn);
}
#endif

//---------------------------------------------------------------------------
int
CmdLoad(int argc, char *argv[])
{
	unsigned long addr;

	image_address = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	printf("Set TFTP Load Addr 0x%x\n", image_address);
}

/*
--------------------------------------------------------------------------
Flash Utility
--------------------------------------------------------------------------
*/
int
CmdFlr(int argc, char *argv[])
{
	int i;
	unsigned long dst, src;
	unsigned int length;
	//unsigned char TARGET;
//#define  FLASH_READ_BYTE      4096

	dst = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	src = strtoul((const char *)(argv[1]), (char **)NULL, 16);
	length = strtoul((const char *)(argv[2]), (char **)NULL, 16);
	//length= (length + (FLASH_READ_BYTE - 1)) & FLASH_READ_BYTE;

/*Cyrus Tsai*/
/*file_length_to_server;*/
//length=file_length_to_client;
//length=length & (~0xffff)+0x10000;
//dst=image_address;
	file_length_to_client = length;
/*Cyrus Tsai*/

	printf("Flash read from %X to %X with %X bytes	?\n", src, dst, length);
	printf("(Y)es , (N)o ? --> ");

	if (YesOrNo())
		//for(i=0;i<length;i++)
		//   {
		//    if ( flashread(&TARGET, src+i,1) )
		//      printf("Flash Read Successed!, target %X\n",TARGET);
		//    else
		//      printf("Flash Read Failed!\n");
		//  }   
		if (flashread(dst, src, length))
			printf("Flash Read Successed!\n");
		else
			printf("Flash Read Failed!\n");
	else
		printf("Abort!\n");
//#undef        FLASH_READ_BYTE         4096

}

#ifndef RTL8197B
/* Setting image header */

//---------------------------------------------------------------------------
#endif				//RTL8197B

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

/*
------------------------------------------  ---------------------------------
; Command Help
---------------------------------------------------------------------------
*/

int
CmdHelp(int argc, char *argv[])
{
	int i;

	printf("----------------- COMMAND MODE HELP ------------------\n");
	for (i = 0; i < (sizeof (MainCmdTable) / sizeof (COMMAND_TABLE)); i++) {
		if (MainCmdTable[i].msg) {
			printf("%s\n", MainCmdTable[i].msg);
		}
	}
	/*Cyrus Tsai */

	return TRUE;
}

//---------------------------------------------------------------------------

int
YesOrNo(void)
{
	unsigned char iChar[2];

	GetLine(iChar, 2, 1);
	printf("\n");		//vicadd
	if ((iChar[0] == 'Y') || (iChar[0] == 'y'))
		return 1;
	else
		return 0;
}

//---------------------------------------------------------------------------
int
CmdSFlw(int argc, char *argv[])
{
	unsigned int cnt2 = 0;	//strtoul((const char*)(argv[3]), (char **)NULL, 16);      
	unsigned int dst_flash_addr_offset =
	    strtoul((const char *)(argv[0]), (char **)NULL, 16);
	unsigned int src_RAM_addr =
	    strtoul((const char *)(argv[1]), (char **)NULL, 16);
	unsigned int length =
	    strtoul((const char *)(argv[2]), (char **)NULL, 16);
	unsigned int end_of_RAM_addr = src_RAM_addr + length;
	printf
	    ("Write 0x%x Bytes to SPI flash#%d, offset 0x%x<0x%x>, from RAM 0x%x to 0x%x\n",
	     length, cnt2 + 1, dst_flash_addr_offset,
	     dst_flash_addr_offset + 0xbd000000, src_RAM_addr, end_of_RAM_addr);
	printf("(Y)es, (N)o->");
	if (YesOrNo()) {
		spi_pio_init();
#if defined(SUPPORT_SPI_MIO_8198_8196C)
		spi_flw_image_mio_8198(cnt2, dst_flash_addr_offset,
				       (unsigned char *)src_RAM_addr, length);
#else
		spi_flw_image(cnt2, dst_flash_addr_offset,
			      (unsigned char *)src_RAM_addr, length);
#endif
	}			//end if YES
	else
		printf("Abort!\n");
}
//---------------------------------------------------------------------------
#if SWITCH_CMD
int
TestCmd_MDIOR(int argc, char *argv[])
{
	if (argc < 1) {
		printf("Parameters not enough!\n");
		return 1;
	}

	//      unsigned int phyid = strtoul((const char*)(argv[0]), (char **)NULL, 16);                
	unsigned int reg = strtoul((const char *)(argv[0]), (char **)NULL, 10);
	unsigned int data;
	int i, phyid;
	for (i = 0; i < 32; i++) {
		phyid = i;
		//REG32(PABCDDAT_REG) =  0xffff<<8;
		rtl8651_getAsicEthernetPHYReg(phyid, reg, &data);
		//REG32(PABCDDAT_REG) =  0<<8;  
		dprintf("PhyID=0x%02x Reg=%02d Data =0x%04x\r\n", phyid, reg,
			data);

	}

}

int
TestCmd_MDIOW(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Parameters not enough!\n");
		return 1;
	}

	unsigned int phyid =
	    strtoul((const char *)(argv[0]), (char **)NULL, 16);
	unsigned int reg = strtoul((const char *)(argv[1]), (char **)NULL, 10);
	unsigned int data = strtoul((const char *)(argv[2]), (char **)NULL, 16);

	dprintf("Write PhyID=0x%x Reg=%02d data=0x%x\r\n", phyid, reg, data);
	rtl8651_setAsicEthernetPHYReg(phyid, reg, data);

}

int
CmdPHYregR(int argc, char *argv[])
{
	unsigned long phyid, regnum;
	unsigned int uid, tmp;

	phyid = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	regnum = strtoul((const char *)(argv[1]), (char **)NULL, 16);

	rtl8651_getAsicEthernetPHYReg(phyid, regnum, &tmp);
	uid = tmp;
	prom_printf("PHYID=0x%x, regID=0x%x ,Find PHY Chip! UID=0x%x\r\n",
		    phyid, regnum, uid);

}

int
CmdPHYregW(int argc, char *argv[])
{
	unsigned long phyid, regnum;
	unsigned long data;
	unsigned int uid, tmp;

	phyid = strtoul((const char *)(argv[0]), (char **)NULL, 16);
	regnum = strtoul((const char *)(argv[1]), (char **)NULL, 16);
	data = strtoul((const char *)(argv[2]), (char **)NULL, 16);

	rtl8651_setAsicEthernetPHYReg(phyid, regnum, data);
	rtl8651_getAsicEthernetPHYReg(phyid, regnum, &tmp);
	uid = tmp;
	prom_printf("PHYID=0x%x ,regID=0x%x, Find PHY Chip! UID=0x%x\r\n",
		    phyid, regnum, uid);

}
#endif

#define MAX_SAMPLE  0x8000
//#define START_ADDR  0x100000               //1MB
#define START_ADDR  0x700000	//7MB, 0~7MB can't be tested
//#define END_ADDR      0x800000                //8MB
//#define END_ADDR      0x1000000         //16MB
//#define END_ADDR      0x2000000        //32MB
//#define END_ADDR      0x4000000       //64MB
//#define END_ADDR      0x8000000         //128MB      
#define BURST_COUNTS  256

 /*JSW: Auto set DRAM test range */

#define END_ADDR 0x02000000	//32MB

 // 1 sec won't sleep,5 secs will sleep
#define MPMR_REG 0xB8001040
int
CmdCPUSleep(int argc, char *argv[])
{

	/*
	   PM_MODE=0 , normal
	   PM_MODE=1 , auto power down
	   PM_MODE=2 , seld refresh
	 */
	if (!argv[0])		//read
	{
		prom_printf("Usage: sleep <0~2>  \r\n");
		prom_printf("sleep <0>:CPU sleep + DRAM Normal mode \r\n");
		prom_printf("sleep <1>:CPU sleep + DRAM Power down  \r\n");
		prom_printf("sleep <2>:CPU sleep + DRAM Self refresh  \r\n");
		prom_printf("sleep <3>:Only DRAM Power down  \r\n");
		prom_printf("sleep <4>:CPUSleep + Self Refresh in IMEM  \r\n");
		return 0;
	}

	unsigned short PM_MODE =
	    strtoul((const char *)(argv[0]), (char **)NULL, 16);

	if (PM_MODE) {

		//set bit[31:30]=0 for default "Normal Mode"
		REG32(MPMR_REG) = 0x3FFFFFFF;

		switch (PM_MODE) {
		case 0:
			prom_printf("\nDRAM : Normal mode\n");
			break;

		case 1:
			prom_printf("\nDRAM :Auto Power Down mode\n");
			REG32(MPMR_REG) = READ_MEM32(MPMR_REG) | (0x1 << 30);
			break;

		case 2:
			prom_printf("\nDRAM : Self Refresh mode\n");
			REG32(MPMR_REG) = 0x3FFFFFFF;
			delay_ms(1000);
			REG32(MPMR_REG) |= (0x2 << 30);
			delay_ms(1000);
			REG32(MPMR_REG) |= (0x2 << 30);

			break;

		case 3:
			prom_printf("\nDRAM :Only Power Down mode!\n");
			REG32(MPMR_REG) = READ_MEM32(MPMR_REG) | (0x1 << 30);
			return 0;

		case 4:
			prom_printf("\nCPUSleep + Self Refresh in IMEM!\n");
			CmdCPUSleepIMEM();

			break;

		default:
			prom_printf("\nError Input,should be 0~3\n");
			break;
		}

		//prom_printf("After setting, MPMR(0xB8001040)=%x\n",READ_MEM32(MPMR_REG) );
	}			//End of DRAMPM

	REG32(GIMR_REG) = 0x0;
	prom_printf("CPU Enter Sleep...\n");

	//cli();    

	//JSW: SLEEP

	__asm__ __volatile__("sleep\n\t");

	//JSW: Make sure CPU do sleep and below won't be printed
	delay_ms(1000);		//delay 1.25 sec in 40MHZ(current OSC), 25/40=1.25 sec
	// After Counter Trigger interrupt
	prom_printf("Counter Trigger interrupt,CPU Leave Sleep...\n");

	return 0;
}

#define __IRAM_IN_865X      __attribute__ ((section(".iram-rtkwlan")))
#define __IRAM_FASTEXTDEV        __IRAM_IN_865X
__IRAM_FASTEXTDEV void
CmdCPUSleepIMEM()
{

	//while( (*((volatile unsigned int *)(0xb8001050))& 0x40000000) != 0x40000000);

	//prom_printf("\nDRAM : Self Refresh mode IMEM01\n");

	REG32(MPMR_REG) = 0x3FFFFFFF;
	delay_ms(1000);
	REG32(MPMR_REG) |= (0x2 << 30);
	delay_ms(1000);
	REG32(MPMR_REG) |= (0x2 << 30);

	REG32(GIMR_REG) = 0x0;
	//cli();    

	//JSW: SLEEP  
	__asm__ __volatile__("sleep\n\t");

	//JSW: Just make sure CPU do sleep and below won't be printed
	delay_ms(1000);
	prom_printf("Counter Trigger interrupt,CPI Leave Sleep...\n");

}

void
PatchFT2()
{
#define mdcmdio_cmd_write rtl8651_setAsicEthernetPHYReg
#define mdcmdio_cmd_read rtl8651_getAsicEthernetPHYReg
	unsigned int i, g;

	unsigned int total_code_list[] = { 0x5400,
		0x5440, 0x54c0, 0x5480,
		0x5580, 0x55c0, 0x5540, 0x5500,
		0x5700, 0x5740, 0x57c0, 0x5780,
		0x5680, 0x56c0, 0x5640, 0x5600,
		0x5400
	};

	unsigned char port_list[] = { 0, 2, 3, 4 };
	unsigned int p;
	unsigned int value;

	unsigned int *code_list_x, *code_list_y;
#define reg20 0xb20

	for (i = 0; i <= 16; i++) {
		code_list_x = &total_code_list[0];
		code_list_y = &total_code_list[i];

		for (p = 0; p < sizeof (port_list) / sizeof (unsigned char);
		     p++) {
			unsigned int phyID = port_list[p];

			//#p4 to page 1
			mdcmdio_cmd_write(4, 31, 0x1);
			//#enable force gary code
			mdcmdio_cmd_write(4, 20, reg20 + (0x1 << phyID));
			mdcmdio_cmd_read(4, 20, &value);
			//dprintf( "reg4 20 = %x\n", value);

			//#per port to page 1
			mdcmdio_cmd_write(phyID, 31, 0x1);

			for (g = 0; g <= (code_list_y - code_list_x); g++) {
				unsigned int gary_code = code_list_x[g];
				mdcmdio_cmd_write(phyID, 19, gary_code);
				dprintf("i=%d phyid=%d gray_code=%x\n", i,
					phyID, gary_code);
			}
			mdcmdio_cmd_write(phyID, 31, 0x0);
		}
		//#dealy, TBD
		__delay(10000);
	}
//#release force mode
	mdcmdio_cmd_write(4, 31, 0x1);
	mdcmdio_cmd_write(4, 20, 0xb20);
	mdcmdio_cmd_write(4, 31, 0x0);

}

int
CmdPortP1Patch(int argc, char *argv[])
{
	PatchFT2();
}

//=====================================================================

//System register Table
#define SYS_BASE 0xb8000000
#define SYS_INT_STATUS (SYS_BASE +0x04)
#define SYS_HW_STRAP   (SYS_BASE +0x08)
#define SYS_BIST_CTRL   (SYS_BASE +0x14)
#define SYS_DRF_BIST_CTRL   (SYS_BASE +0x18)
#define SYS_BIST_OUT   (SYS_BASE +0x1c)
#define SYS_BIST_DONE   (SYS_BASE +0x20)
#define SYS_BIST_FAIL   (SYS_BASE +0x24)
#define SYS_DRF_BIST_DONE   (SYS_BASE +0x28)
#undef SYS_DRF_BIST_FAIL
#define SYS_DRF_BIST_FAIL   (SYS_BASE +0x2c)
#define SYS_PLL_REG   (SYS_BASE +0x30)

//hw strap register

/* Redefinitions for this section - use local variants */
#undef CK_M2X_FREQ_SEL
#define CK_M2X_FREQ_SEL (0x7 <<10)
#undef ST_CPU_FREQ_SEL
#define ST_CPU_FREQ_SEL (0xf<<13)

#define ST_FW_CPU_FREQDIV_SEL (0x1<<18)	//new
#define ST_CK_CPU_FREQDIV_SEL (0x1<<19)	//new

#undef ST_CLKLX_FROM_CLKM
#define ST_CLKLX_FROM_CLKM (1<<21)
#define ST_CLKLX_FROM_HALFOC (1<<22)

#define ST_CLKOC_FROM_CLKM (1<<24)

#undef CK_M2X_FREQ_SEL_OFFSET
#define CK_M2X_FREQ_SEL_OFFSET 10
#undef ST_CPU_FREQ_SEL_OFFSET
#define ST_CPU_FREQ_SEL_OFFSET 13
#undef ST_CPU_FREQDIV_SEL_OFFSET
#define ST_CPU_FREQDIV_SEL_OFFSET 18
#undef ST_CLKLX_FROM_CLKM_OFFSET
#define ST_CLKLX_FROM_CLKM_OFFSET 21

#undef SPEED_IRQ_NO
#define SPEED_IRQ_NO 27		//PA0
#undef SPEED_IRR_NO
#define SPEED_IRR_NO (SPEED_IRQ_NO/8)	//IRR3
#undef SPEED_IRR_OFFSET
#define SPEED_IRR_OFFSET ((SPEED_IRQ_NO-SPEED_IRR_NO*8)*4)	//12

#define GICR_BASE                           0xB8003000
#define GIMR_REG                                (0x000 + GICR_BASE)	/* Global interrupt mask */
#define GISR_REG                                (0x004 + GICR_BASE)	/* Global interrupt status */
#define IRR_REG                                 (0x008 + GICR_BASE)	/* Interrupt routing */
#define IRR1_REG                                (0x00C + GICR_BASE)	/* Interrupt routing */
#define IRR2_REG                                (0x010 + GICR_BASE)	/* Interrupt routing */
#define IRR3_REG                                (0x014 + GICR_BASE)	/* Interrupt routing */

static void
SPEED_isr(int irq, void *dev_id, struct pt_regs *regs)
{

	unsigned int isr = REG32(GISR_REG);
	unsigned int cpu_status = REG32(SYS_INT_STATUS);

	//dprintf("=>CPU Wake-up interrupt happen! GISR=%08x \n", isr);

	if ((isr & (1 << SPEED_IRQ_NO)) == 0)	//check isr==1
	{
		dprintf("Fail, ISR=%x bit %d is not 1\n", isr, SPEED_IRQ_NO);
		while (1) ;
	}

	if ((cpu_status & (1 << 1)) == 0)	//check source==1
	{			//dprintf("Fail, Source=%x bit %d is not 1 \n", cpu_status, 1);
		while (1) ;
	}

	REG32(SYS_INT_STATUS) = (1 << 1);	//enable cpu wakeup interrupt mask
//      REG32(GISR_REG)=1<<SPEED_IRQ_NO;        //write to clear, but cannot clear

	REG32(GIMR_REG) = REG32(GIMR_REG) & ~(1 << SPEED_IRQ_NO);	//so, disable interrupt         
}

struct irqaction irq_SPEED =
    { SPEED_isr, (unsigned long)NULL, (unsigned long)SPEED_IRQ_NO, "SPEED",
(void *)NULL, (struct irqaction *)NULL };

//---------------------------------------------------------------------------

int
SettingCPUClk(int clk_sel, int clk_div, int sync_oc)
{
	int clk_curr, clk_exp;
	unsigned int old_clk_sel;
	unsigned int mask;
	unsigned int sysreg;

	//dprintf("\nInput : CLK_SEL=0x%x, DIV=0x%x, SYNC_OC=0x%x \n", clk_sel, clk_div, sync_oc);
	//dprintf("Want to chage to CPU clock %d\r\n",clk_curr/clk_div);

	//clk_curr = check_cpu_speed();
	//dprintf("Now CPU Speed=%d \n",clk_curr);      
	//----------------------------
	REG32(SYS_INT_STATUS) = (1 << 1);	//enable cpu wakeup interrupt mask
	while (REG32(GISR_REG) & (1 << SPEED_IRQ_NO)) ;	//wait speed bit to low.
	//-------------------------------

	mask = REG32(GIMR_REG);
	//open speed irq

	int irraddr = IRR_REG + SPEED_IRR_NO * 4;
	REG32(irraddr) =
	    (REG32(irraddr) & ~(0x0f << SPEED_IRR_OFFSET)) | (3 <<
							      SPEED_IRR_OFFSET);
	request_IRQ(SPEED_IRQ_NO, &irq_SPEED, NULL);

	//be seure open interrupt first.
	REG32(GIMR_REG) = (1 << SPEED_IRQ_NO);	//accept speed interrupt    
	//REG32(GIMR_REG)=(1<<NFBI_IRQ_NO) ;  //only accept NFBI to interrupt   
	//REG32(GIMR_REG)=(1<<NFBI_IRQ_NO) | (1<<SPEED_IRQ_NO);  //accept speed and NFBI to interrupt           

	//-------------
	sysreg = REG32(SYS_HW_STRAP);
	//dprintf("Read  SYS_HW_STRAP=%08x\r\n", sysreg);       
	old_clk_sel = (sysreg & ST_CPU_FREQ_SEL) >> ST_CPU_FREQ_SEL_OFFSET;

	sysreg &= ~(ST_FW_CPU_FREQDIV_SEL);
	sysreg &= ~(ST_CK_CPU_FREQDIV_SEL);
	sysreg &= ~(ST_CPU_FREQ_SEL);
	//sysreg&= ~(ST_SYNC_OCP);

	sysreg |= (clk_div & 0x03) << ST_CPU_FREQDIV_SEL_OFFSET;
	sysreg |= (clk_sel & 0x0f) << ST_CPU_FREQ_SEL_OFFSET;
	//sysreg|=  (sync_oc&01)<<ST_SYNC_OCP_OFFSET;

	//dprintf("Write SYS_HW_STRAP=%08x \n", sysreg);
	REG32(SYS_HW_STRAP) = sysreg;
	//dprintf("Read  SYS_HW_STRAP=%08x \n", REG32(SYS_HW_STRAP));

	//--------------
	if (old_clk_sel != clk_sel) {

		REG32(GISR_REG) = 0xffffffff;
		//dprintf("before sleep, Read  SYS_HW_STRAP=%08x \n", REG32(SYS_HW_STRAP));     
		//dprintf("GISR=%08x \n",REG32(GISR_REG));
		//dprintf("GIMR=%08x \n",REG32(GIMR_REG));      

		REG32(SYS_BIST_CTRL) |= (1 << 2);	//lock bus arb2
		while ((REG32(SYS_BIST_DONE) & (1 << 0)) == 0) ;	//wait bit to 1, is mean lock ok    

		__asm__ volatile ("sleep");
		__asm__ volatile ("nop");

		REG32(SYS_BIST_CTRL) &= ~(1 << 2);	//unlock
		while ((REG32(SYS_BIST_DONE) & (1 << 0)) == (1 << 0)) ;	//wait bit to 0  unlock

		//dprintf("GISR=%08x\r\n",REG32(GISR_REG));
		//dprintf("GIMR=%08x\r\n",REG32(GIMR_REG));             

		//dprintf("after  sleep, Read  SYS_HW_STRAP=%08x  \n", REG32(SYS_HW_STRAP));

	}

	//-----------------------
	REG32(GIMR_REG) = mask;

}

//---------------------------------------------------------------------------
