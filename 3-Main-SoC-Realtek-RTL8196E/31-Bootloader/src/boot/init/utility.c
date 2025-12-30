#include "utility.h"
#include "rtk.h"
#include <asm/system.h>

extern int dprintf(char *fmt, ...);
#ifndef prom_printf
#define prom_printf dprintf
#endif
#include <rtl_types.h>
#include <rtl8196x/asicregs.h>
#define NEED_CHKSUM 1

#if READ_LINUX_ONCE
unsigned char *p_kernel_img;
#endif

unsigned int gCHKKEY_HIT=0;
unsigned int gCHKKEY_CNT=0;

unsigned long glexra_clock = 200 * 1000 * 1000;

// return,  0: not found, 1: linux found, 2:linux with root found
int check_system_image(unsigned long addr,IMG_HEADER_Tp pHeader,SETTING_HEADER_Tp setting_header)
{
	// Read header, heck signature and checksum
	int i, ret=0;
	unsigned short sum=0, *word_ptr;
	unsigned short length=0;
	unsigned short temp16=0;
	char image_sig_check[1]={0};
	char image_sig[4]={0};
	char image_sig_root[4]={0};
	if(gCHKKEY_HIT==1)
		return 0;
        /*check firmware image.*/
	word_ptr = (unsigned short *)pHeader;
	for (i=0; i<sizeof(IMG_HEADER_T); i+=2, word_ptr++)
		*word_ptr = rtl_inw(addr + i);	

	memcpy(image_sig, FW_SIGNATURE, SIG_LEN);
	memcpy(image_sig_root, FW_SIGNATURE_WITH_ROOT, SIG_LEN);

	if (!memcmp(pHeader->signature, image_sig, SIG_LEN))
		ret=1;
	else if  (!memcmp(pHeader->signature, image_sig_root, SIG_LEN))
		ret=2;
	else{
		prom_printf("no sys signature at %X!\n",addr-FLASH_BASE);
	}		
//	prom_printf("ret=%d  sys signature at %X!\n",ret,addr-FLASH_BASE);
	if (ret) {

	#if READ_LINUX_ONCE
		p_kernel_img = (unsigned char *)pHeader->startAddr;
		flashread((unsigned long)p_kernel_img,
			  (unsigned int)(addr - FLASH_BASE + sizeof(IMG_HEADER_T)),
			  pHeader->len);
	#endif
	
	for (i=0; i<pHeader->len; i+=2) {
		if((i%0x10000) == 0)
		{
		gCHKKEY_CNT++;
		if( gCHKKEY_CNT>ACCCNT_TOCHKKEY)
		{	gCHKKEY_CNT=0;
			if ( user_interrupt(0)==1 )  //return 1: got ESC Key
			{
//					prom_printf("ret=%d  ------> line %d!\n",ret,__LINE__);
				return 0;
			}
		}
		}
#if defined(NEED_CHKSUM)	
#if READ_LINUX_ONCE
		sum += *(unsigned short*)(p_kernel_img+i);
#else
		sum += rtl_inw(addr + sizeof(IMG_HEADER_T) + i);
#endif
#endif
	}	
#if defined(NEED_CHKSUM)			
		if ( sum ) {
//			prom_printf("ret=%d  ------> line %d!\n",ret,__LINE__);
			ret=0;
		}
#endif		
	}
//	prom_printf("ret=%d  sys signature at %X!\n",ret,addr-FLASH_BASE);

	return (ret);
}
//------------------------------------------------------------------------------------------

int check_rootfs_image(unsigned long addr)
{
	// Read header, heck signature and checksum
	int i;
	unsigned short sum=0, *word_ptr;
	unsigned long length=0;
	unsigned char tmpbuf[16];	
	#define SIZE_OF_SQFS_SUPER_BLOCK 640
	#define SIZE_OF_CHECKSUM 2
	#define OFFSET_OF_LEN 2

	if(gCHKKEY_HIT==1)
		return 0;
	
	word_ptr = (unsigned short *)tmpbuf;
	for (i=0; i<16; i+=2, word_ptr++)
		*word_ptr = rtl_inw(addr + i);

	if ( memcmp(tmpbuf, SQSH_SIGNATURE, SIG_LEN) && memcmp(tmpbuf, SQSH_SIGNATURE_LE, SIG_LEN)) {
		prom_printf("no rootfs signature at %X!\n",addr-FLASH_BASE);
		return 0;
	}

	length = *(((unsigned long *)tmpbuf) + OFFSET_OF_LEN) + SIZE_OF_SQFS_SUPER_BLOCK + SIZE_OF_CHECKSUM;

	for (i=0; i<length; i+=2) {
			gCHKKEY_CNT++;
			if( gCHKKEY_CNT>ACCCNT_TOCHKKEY)
			{	gCHKKEY_CNT=0;
				if ( user_interrupt(0)==1 )  //return 1: got ESC Key
					return 0;
			}
#if defined(NEED_CHKSUM)	
		sum += rtl_inw(addr + i);
#endif
	}

#if defined(NEED_CHKSUM)		
	if ( sum ) {
		prom_printf("rootfs checksum error at %X!\n",addr-FLASH_BASE);
		return 0;
	}	
#endif	
	return 1;
}
//------------------------------------------------------------------------------------------

static int check_image_header(IMG_HEADER_Tp pHeader,SETTING_HEADER_Tp psetting_header,unsigned long bank_offset)
{
	int i,ret=0;
	//flash mapping
	return_addr = (unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET+bank_offset;
	printf("%s  return_addr:%x bank_offset:%x\r\n",__FUNCTION__,return_addr,bank_offset);
	ret = check_system_image((unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET+bank_offset,pHeader, psetting_header);

	if(ret==0) {
		return_addr = (unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET2+bank_offset;		
		ret=check_system_image((unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET2+bank_offset,  pHeader, psetting_header);
	}
	if(ret==0) {
		return_addr = (unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET3+bank_offset;				
		ret=check_system_image((unsigned long)FLASH_BASE+CODE_IMAGE_OFFSET3+bank_offset,  pHeader, psetting_header);
	}	

	i=CONFIG_LINUX_IMAGE_OFFSET_START;	
	while(i<=CONFIG_LINUX_IMAGE_OFFSET_END && (0==ret))
	{
		return_addr=(unsigned long)FLASH_BASE+i+bank_offset; 
		if(CODE_IMAGE_OFFSET == i || CODE_IMAGE_OFFSET2 == i || CODE_IMAGE_OFFSET3 == i){
			i += CONFIG_LINUX_IMAGE_OFFSET_STEP; 
			continue;
		}
		ret = check_system_image((unsigned long)FLASH_BASE+i+bank_offset, pHeader, psetting_header);
		i += CONFIG_LINUX_IMAGE_OFFSET_STEP; 
	}

	if(ret==2)
        {
                ret=check_rootfs_image((unsigned long)FLASH_BASE+ROOT_FS_OFFSET+bank_offset);
                if(ret==0)
                	ret=check_rootfs_image((unsigned long)FLASH_BASE+ROOT_FS_OFFSET+ROOT_FS_OFFSET_OP1+bank_offset);
                if(ret==0)
                	ret=check_rootfs_image((unsigned long)FLASH_BASE+ROOT_FS_OFFSET+ROOT_FS_OFFSET_OP1+ROOT_FS_OFFSET_OP2+bank_offset);

		i = CONFIG_ROOT_IMAGE_OFFSET_START;
		while((i <= CONFIG_ROOT_IMAGE_OFFSET_END) && (0==ret))
		{
			if( ROOT_FS_OFFSET == i ||
			    (ROOT_FS_OFFSET + ROOT_FS_OFFSET_OP1) == i ||
		            (ROOT_FS_OFFSET + ROOT_FS_OFFSET_OP1 + ROOT_FS_OFFSET_OP2) == i){
				i += CONFIG_ROOT_IMAGE_OFFSET_STEP;
				continue;
			}
			ret = check_rootfs_image((unsigned long)FLASH_BASE+i+bank_offset);
			i += CONFIG_ROOT_IMAGE_OFFSET_STEP;
		}
	}
	return ret;
}
//------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------

int check_image(IMG_HEADER_Tp pHeader,SETTING_HEADER_Tp psetting_header)
{
	int ret=0;
 	//only one bank

 	ret=check_image_header(pHeader,psetting_header,0); 

	return ret;
}

//------------------------------------------------------------------------------------------
//monitor user interrupt
int pollingDownModeKeyword(int key)
{
	int i;
	if  (Check_UART_DataReady() )
	{
		i=Get_UART_Data();
		Get_UART_Data();
		if( i == key )
		{ 	
#if defined(UTILITY_DEBUG)		
			dprintf("User Press ESC Break Key\r\n");
#endif			
			gCHKKEY_HIT=1;
			return 1;
		}
	}
	return 0;
}
//------------------------------------------------------------------------------------------

int pollingPressedButton(int pressedFlag)
{
		// polling if button is pressed --------------------------------------
    		if (pressedFlag == -1 ||  pressedFlag == 1) 
		{

	// vincent: already done in Init_GPIO(). do nothing here
	//		REG32(PEFGHCNR_REG) = REG32(PEFGHCNR_REG)& (~(1<<25) ); //set byte F GPIO7 = gpio
        //     		REG32(PEFGHDIR_REG) = REG32(PEFGHDIR_REG) & (~(1<<25) );  //0 input, 1 out
		
			if ( Get_GPIO_SW_IN() )			
			{// button pressed
#if defined(UTILITY_DEBUG)			
	    			dprintf("User Press GPIO Break Key\r\n");
#endif	    			
				if (pressedFlag == -1) 
				{
					//SET_TIMER(1*CPU_CLOCK); // wait 1 sec
				}
				pressedFlag = 1;
				gCHKKEY_HIT=1;
#if defined(UTILITY_DEBUG)				
				dprintf("User Press Break Button\r\n",__LINE__);
#endif
				return 1;	//jasonwang//wei add				

			}
			else
		      		pressedFlag = 0;
		}
#if defined(UTILITY_DEBUG)
	dprintf("j=%x\r\n",get_timer_jiffies());
#endif

	return pressedFlag;
}
//------------------------------------------------------------------------------------------

//return 0: do nothing; 1: jump to down load mode; 3 jump to debug down load mode
int user_interrupt(unsigned long time)
{
	int i,ret;
	int tickStart=0;
#ifdef SUPPORT_TFTP_CLIENT
	extern int check_tftp_client_state();	
#endif
	
	int button_press_detected=-1;
	
	tickStart=get_timer_jiffies();
#ifdef  SUPPORT_TFTP_CLIENT
	do 
#endif
    {
		ret=pollingDownModeKeyword(ESC);
		if(ret == 1) return 1;
		ret=pollingPressedButton(button_press_detected);
		button_press_detected=ret;
		if(ret > 0) return ret;
	}
#ifdef SUPPORT_TFTP_CLIENT
	while(check_tftp_client_state() >= 0);
#endif

#if defined(UTILITY_DEBUG)
	dprintf("timeout\r\n");
#endif	
	if (button_press_detected>0)
	{   
		gCHKKEY_HIT=1;    
		return 1;
	}
	return 0;
}
//------------------------------------------------------------------------------------------

	       

//------------------------------------------------------------------------------------------
//init gpio[96c not fix gpio, so close first. fix CPU 390MHz cannot boot from flash.]
void Init_GPIO()
{
	REG32(RTL_GPIO_MUX) = (REG32(RTL_GPIO_MUX) & ~(0x7))|0x6;
	REG32(PABCDCNR_REG) = REG32(PABCDCNR_REG)& (~(1<<5) ); //set byte F GPIO7 = gpio
	REG32(PABCDDIR_REG) = REG32(PABCDDIR_REG) & (~(1<<5) );  //0 input, 1 output, set F bit 7 input
}
//------------------------------------------------------------------------------------------
//-------------------------------------------------------
void console_init(unsigned long lexea_clock)
{
	int i;
	unsigned long dl;
	unsigned long dll;     
	unsigned long dlm;       


  	REG32(UART_LCR_REG)=0x03000000;		//Line Control Register  8,n,1
  			
  	REG32( UART_FCR_REG)=0xc7000000;		//FIFO Ccontrol Register
  	REG32( UART_IER_REG)=0x00000000;
  	dl = (lexea_clock /16)/BAUD_RATE-1;
  	*(volatile unsigned long *)(0xa1000000) = dl ; 
  	dll = dl & 0xff;
  	dlm = dl / 0x100;
  	REG32( UART_LCR_REG)=0x83000000;		//Divisor latch access bit=1
  	REG32( UART_DLL_REG)=dll*0x1000000;
   	REG32( UART_DLM_REG)=dlm*0x1000000; 
    	REG32( UART_LCR_REG)=0x83000000& 0x7fffffff;	//Divisor latch access bit=0
   	//rtl_outl( UART_THR,0x41000000);	

	//dprintf("\n\n-------------------------------------------");
	//dprintf("\nUART1 output test ok\n");
}
//-------------------------------------------------------


void goToDownMode()
{


		eth_startup(0);

		dprintf("Ethernet ready\n");
		sti();

		tftpd_entry(0);
		
#ifdef DHCP_SERVER			
		dhcps_entry();
#endif
#ifdef HTTP_SERVER
		httpd_entry();
#endif

	monitor();
	return ;
}
//-------------------------------------------------------

//-------------------------------------------------------

void goToLocalStartMode(unsigned long addr,IMG_HEADER_Tp pheader)
{
	unsigned short *word_ptr;
	void	(*jump)(void);
	int i;
	
	//prom_printf("\n---%X\n",return_addr);
	word_ptr = (unsigned short *)pheader;
	for (i=0; i<sizeof(IMG_HEADER_T); i+=2, word_ptr++)
	*word_ptr = rtl_inw(addr + i);
			
	// move image to SDRAM
  #if READ_LINUX_ONCE
		;//do nothing
  #else
	flashread( pheader->startAddr,	(unsigned int)(addr-FLASH_BASE+sizeof(IMG_HEADER_T)), 	pheader->len-2);
#endif
	if ( !user_interrupt(0) )  // See if user escape during copy image
	{
		outl(0,GIMR0); // mask all interrupt
		Set_GPIO_LED_OFF();

		prom_printf("Booting kernel @ 0x%x\n", pheader->startAddr);
		
		jump = (void *)(pheader->startAddr);

		cli();
		flush_cache(); 
		jump();				 // jump to start
		return ;
	}
	return;
}

//-------------------------------------------------------

//-------------------------------------------------------
//set clk and init console	
void setClkInitConsole(void)
{
	REG32(MCR_REG)=REG32(MCR_REG)|(1<<27);  //new prefetch

	console_init( glexra_clock);
}
//-------------------------------------------------------
//init heap	
void initHeap(void)
{
	/* Initialize malloc mechanism */
	UINT32 heap_addr=((UINT32)dl_heap&(~7))+8 ;
	UINT32 heap_end=heap_addr+sizeof(dl_heap)-8;
    i_alloc((void *)heap_addr, (void *)heap_end);
	cli();  	
	flush_cache(); // david
}
//-------------------------------------------------------
// init interrupt 
void initInterrupt(void)
{
	rtl_outl(GIMR0,0x00);/*mask all interrupt*/
	setup_arch();    /*setup the BEV0,and IRQ */
	exception_init();/*Copy handler to 0x80000080*/
	init_IRQ();      /*Allocate IRQfinder to Exception 0*/
	sti();
}
//-------------------------------------------------------
// init flash 
void initFlash(void)
{
   	spi_probe();                                    //JSW : SPI flash init		

}
//-------------------------------------------------------
//rtk bootcode and enable post
//copy img to sdram and monitor ESC interrupt

void doBooting(int flag, unsigned long addr, IMG_HEADER_Tp pheader)
{
#ifdef SUPPORT_TFTP_CLIENT	
	extern int check_tftp_client_state();

	if(flag || check_tftp_client_state() >= 0)
#else
	if(flag)
#endif		
	{

		switch(user_interrupt(WAIT_TIME_USER_INTERRUPT))
		{
		case LOCALSTART_MODE:
		default:
#ifdef SUPPORT_TFTP_CLIENT
			/* disable Ethernet switch */
			REG32(0xb8000010)= REG32(0xb8000010)&(~(1<<11));
			if (!flag) {
				REG32(GIMR_REG)=0x0;   //add by jiawenjian
				goToDownMode(); 	
			}	
#endif			
			goToLocalStartMode(addr,pheader);			
		case DOWN_MODE:
			dprintf("Entering recovery mode\n");
			//cli();
		REG32(GIMR_REG)=0x0;   //add by jiawenjian

			goToDownMode();	
			break;
		}/*switch case */
	}/*if image correct*/
	else
	{
		REG32(GIMR_REG)=0x0;   //add by jiawenjian
		goToDownMode();		
	}
	return;
}
