#include <asm/system.h>
#include <rtl_types.h>
#include <boot/string.h>
#include "etherboot.h"
#include "nic.h"
#include "rtk.h"
#include "spi_common.h"

#include "spi_flash.h"

#include <asm/rtl8196.h>

struct arptable_t arptable_tftp[3];

/*Cyrus Tsai*/
#ifndef JUMP_ADDR
#define FILESTART 0x80300000
#else
#define FILESTART JUMP_ADDR
#endif

/*Cyrus Tsai for vm_test.*/
#define TEST_FILENAME	((char *)"nfjrom")
//#define TESTSTART FILESTART
#define BOOT_FILENAME	((char *)"boot.img")
#define BOOTSTART 0x80000000

#define prom_printf dprintf

extern unsigned long ETH0_ADD;
extern struct spi_flash_type spi_flash_info[2];

int jump_to_test = 0;
void (*jumpF)(void);

#define CODESTART 0x10000

extern volatile int get_timer_jiffies(void);
#define REG32(reg)   (*(volatile unsigned int   *)((unsigned int)reg))
int tftpd_is_ready = 0;
int rx_kickofftime = 0;
Int8 one_tftp_lock = 0;		//Cyrus for version 2

struct nic nic;
/*Data structure shared by ethernet driver and tftpd */
Int8 eth_packet[ETH_FRAME_LEN + 4];
Int8 ETH_BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define IPTOUL(a,b,c,d)	((a << 24)| (b << 16) | (c << 8) | d )

/*Address to store image file*/
Int32 image_address = FILESTART;
Int32 address_to_store;

/*Cyrus Tsai*/
Int32 file_length_to_server;
Int32 file_length_to_client;
/*this is the file length, should extern to flash driver*/
/*Cyrus Tsai*/

#ifdef SUPPORT_TFTP_CLIENT
#define MAX_RETRY_NUM		5
#define WAIT_TIMEOUT		50	/* 50ms */

extern char errmsg[512];
extern unsigned short errcode;
extern unsigned int tftp_from_command;
extern unsigned int tftp_client_recvdone;
extern char tftpfilename[128];
static int tftp_client_enabled;
volatile unsigned int last_sent_time = 0;
int retry_cnt = 0;
#endif

/*store the block number*/
volatile Int16 block_expected;
int it_is_EOF = 0;
#if defined(HTTP_SERVER)
char message[128];
#endif
char filename[TFTP_DEFAULTSIZE_PACKET];

typedef void (*Func_t)(void);
/*there are 3 states in the boot downloader*/
/*Different frames received, these event can
  trigger the state-event machine move.    */

typedef enum BootStateTag
{
	INVALID_BOOT_STATE = -1,
	BOOT_STATE0_INIT_ARP = 0,
	BOOT_STATE1_TFTP_CLIENT_RRQ = 1,
	BOOT_STATE2_TFTP_CLIENT_WRQ = 2,
#ifdef SUPPORT_TFTP_CLIENT
	BOOT_STATE3_TFTP_SERVER_DATA = 3,
	BOOT_STATE4_TFTP_SERVER_DATA = 4,
	NUM_OF_BOOT_STATES = 5,
#else
	NUM_OF_BOOT_STATES = 3
#endif
}
BootState_t;

/*if we are server, we should never receive an OACK    */
/*if we are client, receive an OACK and goto oackTFTP()*/
typedef enum BootEventTag
{
	INVALID_BOOT_EVENT = -1,
	BOOT_EVENT0_ARP_REQ = 0,
	BOOT_EVENT1_ARP_REPLY = 1,
	BOOT_EVENT2_TFTP_RRQ = 2,
	BOOT_EVENT3_TFTP_WRQ = 3,
	BOOT_EVENT4_TFTP_DATA = 4,
	BOOT_EVENT5_TFTP_ACK = 5,
	BOOT_EVENT6_TFTP_ERROR = 6,
	BOOT_EVENT7_TFTP_OACK = 7,
	NUM_OF_BOOT_EVENTS = 8
}
BootEvent_t;

BootState_t bootState;
BootEvent_t bootEvent;

/*State-Event functions*/

static void errorDrop(void);
static void errorTFTP(void);

static void doARPReply(void);
static void updateARPTable(void);
static void setTFTP_RRQ(void);
static void setTFTP_WRQ(void);
static void prepareACK(void);
static void prepareDATA(void);

Int16 CLIENT_port;
Int16 SERVER_port;

/*tftpd server entry point*/
void tftpd_entry(int is_client_mode);

void tftpd_send_ack(Int16 number);
int check_image(Int32 * data_to_transmit);
Int16 ipheader_chksum(Int16 * ip, int len);
void tftpd_send_data(char *filename, Int16 block_number);
void kick_tftpd(void);
/*in misc.c*/
extern void twiddle(void);

extern void prepare_txpkt(int etherport, Int16 type, Int8 * destaddr,
			  Int8 * data, Int16 len);

static const Func_t BootStateEvent[NUM_OF_BOOT_STATES][NUM_OF_BOOT_EVENTS]
    = {
	/*BOOT_STATE0_INIT_ARP */
	{
	 /*BOOT_EVENT0_ARP_REQ */ doARPReply,
	 /*BOOT_EVENT1_ARP_REPLY */ updateARPTable,
	 /*BOOT_EVENT2_TFTP_RRQ */ setTFTP_RRQ,
	 /*BOOT_EVENT3_TFTP_WRQ */ setTFTP_WRQ,
						/*BOOT_EVENT4_TFTP_DATA */ errorDrop,
						/*ERROR in state transition */
						/*BOOT_EVENT5_TFTP_ACK */ errorDrop,
						/*ERROR in state transition */
						/*BOOT_EVENT6_TFTP_ERROR */ errorDrop,
						/*ERROR in state transition */
						/*BOOT_EVENT7_TFTP_OACK */ errorDrop,
						/*ERROR in state transition */
	 },
	/*BOOT_STATE1_TFTP_CLIENT_RRQ */
	{
	 /*BOOT_EVENT0_ARP_REQ */ doARPReply,
	 /*BOOT_EVENT1_ARP_REPLY */ updateARPTable,
	 /*BOOT_EVENT2_TFTP_RRQ */ setTFTP_RRQ,
						/*BOOT_EVENT3_TFTP_WRQ */ errorTFTP,
						/*ERROR in TFTP protocol */
	 /*BOOT_EVENT4_TFTP_DATA */ prepareACK,
	 /*BOOT_EVENT5_TFTP_ACK */ prepareDATA,
						/*BOOT_EVENT6_TFTP_ERROR */ errorTFTP,
						/*ERROR in TFTP protocol */
						/*BOOT_EVENT7_TFTP_OACK */ errorTFTP,
						/*ERROR in TFTP protocol */
	 },
	/*BOOT_STATE2_TFTP_CLIENT_WRQ */
	{
	 /*BOOT_EVENT0_ARP_REQ */ doARPReply,
	 /*BOOT_EVENT1_ARP_REPLY */ updateARPTable,
						/*BOOT_EVENT2_TFTP_RRQ */ errorTFTP,
						/*ERROR in TFTP protocol */
	 /*BOOT_EVENT3_TFTP_WRQ */ setTFTP_WRQ,
	 /*BOOT_EVENT4_TFTP_DATA */ prepareACK,
	 /*BOOT_EVENT5_TFTP_ACK */ prepareDATA,
						/*BOOT_EVENT6_TFTP_ERROR */ errorTFTP,
						/*ERROR in TFTP protocol */
						/*BOOT_EVENT7_TFTP_OACK */ errorTFTP,
						/*ERROR in TFTP protocol */
	 },
#ifdef SUPPORT_TFTP_CLIENT
	/*BOOT_STATE3_TFTP_SERVER_DATA */
	{
	 /*BOOT_EVENT0_ARP_REQ */ doARPReply,
	 /*BOOT_EVENT1_ARP_REPLY */ updateARPTable,
						/*BOOT_EVENT2_TFTP_RRQ */ errorTFTP,
						/*ERROR in TFTP protocol */
	 /*BOOT_EVENT3_TFTP_WRQ */ errorTFTP,
	 /*BOOT_EVENT4_TFTP_DATA */ prepareACK,
	 /*BOOT_EVENT5_TFTP_ACK */ errorTFTP,
						/*BOOT_EVENT6_TFTP_ERROR */ errorTFTP,
						/*ERROR in TFTP protocol */
						/*BOOT_EVENT7_TFTP_OACK */ errorTFTP,
						/*ERROR in TFTP protocol */
	 },
	/*BOOT_STATE4_TFTP_SERVER_DATA */
	{
	 /*BOOT_EVENT0_ARP_REQ */ doARPReply,
	 /*BOOT_EVENT1_ARP_REPLY */ updateARPTable,
						/*BOOT_EVENT2_TFTP_RRQ */ errorTFTP,
						/*ERROR in TFTP protocol */
	 /*BOOT_EVENT3_TFTP_WRQ */ errorTFTP,
	 /*BOOT_EVENT4_TFTP_DATA */ prepareACK,
	 /*BOOT_EVENT5_TFTP_ACK */ errorTFTP,
						/*BOOT_EVENT6_TFTP_ERROR */ errorTFTP,
						/*ERROR in TFTP protocol */
						/*BOOT_EVENT7_TFTP_OACK */ errorTFTP,
						/*ERROR in TFTP protocol */
	 },
#endif
};

#ifdef SUPPORT_TFTP_CLIENT
static void
send_arp_request()
{
	struct arprequest arp_req;

//dprintf("send ARP request\n");

	memset(&arp_req, '\0', sizeof (arp_req));

	arp_req.hwtype = htons(1);
	arp_req.protocol = htons(FRAME_IP);	/*that is 0x0800 */
	arp_req.hwlen = ETH_ALEN;
	arp_req.protolen = 4;
	arp_req.opcode = htons(ARP_REQUEST);

	memcpy(arp_req.shwaddr, arptable_tftp[TFTP_CLIENT].node, ETH_ALEN);
	memcpy(arp_req.thwaddr, arptable_tftp[TFTP_SERVER].node, ETH_ALEN);
	memcpy(arp_req.sipaddr, &arptable_tftp[TFTP_CLIENT].ipaddr,
	       sizeof (in_addr));
	memcpy(arp_req.tipaddr, &arptable_tftp[TFTP_SERVER].ipaddr,
	       sizeof (in_addr));

	prepare_txpkt(0, FRAME_ARP, arp_req.thwaddr, (Int8 *) & arp_req,
		      (Int16) sizeof (arp_req));
}

static void
send_tftp_rrq(char *filename)
{
	struct iphdr *ip;
	struct udphdr *udp;
	struct tftp_t tftp_tx;
	int length;

//dprintf("send TFTP-RRQ request\n");

	memset(&tftp_tx, '\0', sizeof (tftp_tx));

	tftp_tx.opcode = htons(TFTP_RRQ);
	length = strlen(filename) + 1;
	memcpy(tftp_tx.u.rrq, filename, length);
	memcpy(&tftp_tx.u.rrq[length], "octet", 6);
	length += 6;

	ip = (struct iphdr *)&tftp_tx;
	udp = (struct udphdr *)((Int8 *) & tftp_tx + sizeof (struct iphdr));

	/*generate the IP header */
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(30 + length);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = 60;
	ip->protocol = IP_UDP;
	ip->chksum = 0;
	ip->src.s_addr = arptable_tftp[TFTP_CLIENT].ipaddr.s_addr;
	ip->dest.s_addr = arptable_tftp[TFTP_SERVER].ipaddr.s_addr;
	ip->chksum =
	    ipheader_chksum((Int16 *) & tftp_tx, sizeof (struct iphdr));
	/*generate the UDP header */
	udp->src = htons(CLIENT_port);
	udp->dest = htons(SERVER_port);
	udp->len = htons(length + 2 + 8);
	udp->chksum = 0;

	prepare_txpkt(0, FRAME_IP, arptable_tftp[TFTP_SERVER].node,
		      (Int8 *) & tftp_tx,
		      (Int16) sizeof (struct iphdr) + sizeof (struct udphdr) +
		      length + 2);
}

/* return value: -1, tftp client abort, 0 - waiting ARP reply, 1 - waiting first block, 2 - waiting remainding block */
int
check_tftp_client_state()
{
	if (!tftp_client_enabled)
		return -1;

	if (bootState == BOOT_STATE0_INIT_ARP) {	/* wait arp reply */
		if (last_sent_time == 0
		    || get_timer_jiffies() - last_sent_time > WAIT_TIMEOUT) {
			if (retry_cnt++ > MAX_RETRY_NUM)
				return -1;

			send_arp_request();
			last_sent_time = get_timer_jiffies();
		}
		return 0;
	}

	if (bootState == BOOT_STATE3_TFTP_SERVER_DATA) {	/* wait first read block */
		if (get_timer_jiffies() - last_sent_time > WAIT_TIMEOUT) {
			if (++retry_cnt > MAX_RETRY_NUM)
				return -1;

			if (tftp_from_command)
				send_tftp_rrq(tftpfilename);
			else
				send_tftp_rrq(TEST_FILENAME);
			last_sent_time = get_timer_jiffies();
		}
		return 1;
	}

	if (bootState == BOOT_STATE4_TFTP_SERVER_DATA) {	/* wait remainding block */
		if (get_timer_jiffies() - last_sent_time > WAIT_TIMEOUT)
			return -1;

		return 2;
	}

	return -1;
}
#endif				// SUPPORT_TFTP_CLIENT

//----------------------------------------------------------------------------------------
static void
errorDrop(void)
{
	if (!tftpd_is_ready)
		return;
/*no need to change boot state*/
	prom_printf("Boot state error,%d,%d\n", bootState, bootEvent);
//bootState=BOOT_STATE0_INIT_ARP;
/*error in boot state machine*/
}

//----------------------------------------------------------------------------------------
static void
errorTFTP(void)
{
	if (!tftpd_is_ready)
		return;
/*no need to change boot state*/
//prom_printf("TFTP procotol error,%d,%d\n",bootState,bootEvent);
#if defined(SUPPORT_TFTP_CLIENT)
	if (bootState == BOOT_STATE3_TFTP_SERVER_DATA
	    || bootState == BOOT_STATE4_TFTP_SERVER_DATA) {
		dprintf("[errcode from TFTP server:] %d\n", errcode);
		dprintf("[errmsg from TFTP server:] %s\n", errmsg);
		tftp_client_recvdone = 1;
	}
#endif
	bootState = BOOT_STATE0_INIT_ARP;

#ifdef SUPPORT_TFTP_CLIENT
	if (tftp_client_enabled) {
		tftp_client_enabled = 0;
		tftpd_is_ready = 0;
	}
#endif
}

//----------------------------------------------------------------------------------------
static void
doARPReply(void)
{
	/*we receive an ARP request. */
	/*In our configuration, all we need is one TFTP_CLIENT, */
	struct arprequest *arppacket;
	struct arprequest arpreply;
	int i;
	Int8 checkIP[4];
	Int32 targetIP;

//dprintf("execute ARP reply function\n");

	arppacket = (struct arprequest *)&(nic.packet[ETH_HLEN]);

//memcpy(arptable_tftp[TFTP_CLIENT].ipaddr.ip, arppacket->sipaddr, sizeof(in_addr)); 
	//prom_printf("DoARPRpy 2.:update CLIENT ip address %x\n",arptable_tftp[TFTP_CLIENT].ipaddr.s_addr);

	memcpy(&targetIP, arppacket->tipaddr, 4);
#if defined(HTTP_SERVER)
/*we have two pairs of mac and ip. let http ack arp first if http's and tftp's ip are same*/
	if (targetIP == arptable_tftp[HTTPD_ARPENTRY].ipaddr.s_addr) {
		/*Fill in the arp reply payload. */
		arpreply.hwtype = htons(1);
		arpreply.protocol = htons(FRAME_IP);	/*that is 0x0800 */
		arpreply.hwlen = ETH_ALEN;
		arpreply.protolen = 4;
		arpreply.opcode = htons(ARP_REPLY);
		memcpy(&(arpreply.shwaddr),
		       &(arptable_tftp[HTTPD_ARPENTRY].node), ETH_ALEN);
		memcpy(&(arpreply.sipaddr),
		       &(arptable_tftp[HTTPD_ARPENTRY].ipaddr),
		       sizeof (in_addr));
		memcpy(&(arpreply.thwaddr), arppacket->shwaddr, ETH_ALEN);
		memcpy(&(arpreply.tipaddr), arppacket->sipaddr,
		       sizeof (in_addr));
		prepare_txpkt(0, FRAME_ARP, arppacket->shwaddr,
			      (Int8 *) & arpreply, (Int16) sizeof (arpreply));
	} else
		//now lock tftp.. 
		//now we have to check mac address, for safety..
#endif

#ifdef SUPPORT_TFTP_CLIENT
	if ((tftp_client_enabled
		     && (targetIP ==
				 arptable_tftp[TFTP_CLIENT].ipaddr.
				 s_addr)) || (!tftp_client_enabled
						      && (targetIP ==
								  arptable_tftp
								  [TFTP_SERVER].
								  ipaddr.
								  s_addr)))
#else
	if (targetIP == arptable_tftp[TFTP_SERVER].ipaddr.s_addr)	/*that is us */
#endif
	{
		/*Fill in the arp reply payload. */
		arpreply.hwtype = htons(1);
		arpreply.protocol = htons(FRAME_IP);	/*that is 0x0800 */
		arpreply.hwlen = ETH_ALEN;
		arpreply.protolen = 4;
		arpreply.opcode = htons(ARP_REPLY);
#ifdef SUPPORT_TFTP_CLIENT
		if (tftp_client_enabled) {
			memcpy(&(arpreply.shwaddr),
			       &(arptable_tftp[TFTP_CLIENT].node), ETH_ALEN);
			memcpy(&(arpreply.sipaddr),
			       &(arptable_tftp[TFTP_CLIENT].ipaddr),
			       sizeof (in_addr));
		} else
#endif
		{
			memcpy(&(arpreply.shwaddr),
			       &(arptable_tftp[TFTP_SERVER].node), ETH_ALEN);
			memcpy(&(arpreply.sipaddr),
			       &(arptable_tftp[TFTP_SERVER].ipaddr),
			       sizeof (in_addr));
		}
		memcpy(&(arpreply.thwaddr), arppacket->shwaddr, ETH_ALEN);
		memcpy(&(arpreply.tipaddr), arppacket->sipaddr,
		       sizeof (in_addr));

		prepare_txpkt(0, FRAME_ARP, arppacket->shwaddr,
			      (Int8 *) & arpreply, (Int16) sizeof (arpreply));
	}
}

//----------------------------------------------------------------------------------------
static void
updateARPTable(void)
{

#ifdef SUPPORT_TFTP_CLIENT

//dprintf("Rx ARP reply\n");

	if (tftp_client_enabled) {
		/*??? do we really need this in TFTP server operation */
		struct arprequest *arppacket;
		arppacket = (struct arprequest *)&(nic.packet[ETH_HLEN]);
		Int32 sourceIP;

		memcpy(&sourceIP, arppacket->sipaddr, 4);
		if (sourceIP == arptable_tftp[TFTP_SERVER].ipaddr.s_addr) {
			memcpy(arptable_tftp[TFTP_SERVER].node,
			       arppacket->shwaddr, ETH_ALEN);

			if (tftp_from_command) {
				send_tftp_rrq(tftpfilename);
				bootState = BOOT_STATE3_TFTP_SERVER_DATA;
				block_expected = 1;
				address_to_store = image_address;
				file_length_to_server = 0;
				retry_cnt = 0;
				last_sent_time = get_timer_jiffies();
				//tftp_from_command = 0;
				dprintf
				    ("send rrq to TFTP server, [filename:] %s, image_address = 0x%x\n",
				     tftpfilename, image_address);
			} else {
				send_tftp_rrq(TEST_FILENAME);
				bootState = BOOT_STATE3_TFTP_SERVER_DATA;
				block_expected = 1;
				address_to_store = image_address = FILESTART;
				file_length_to_server = 0;
				retry_cnt = 0;
				last_sent_time = get_timer_jiffies();
				//dprintf("--send rrq to TFTP server--, [filename:] %s, image_address = 0x%x\n",TEST_FILENAME,image_address);
			}
		}
	}
#endif
}

//----------------------------------------------------------------------------------------
static void
setTFTP_RRQ(void)
{
	struct udphdr *udpheader;
	struct tftp_t *tftppacket;

	Int16 tftpopcode;
	int find_zero;

	if (!tftpd_is_ready)
		return;
	udpheader =
	    (struct udphdr *)&nic.packet[ETH_HLEN + sizeof (struct iphdr)];
	if (udpheader->dest == htons(TFTP_PORT)) {

		prom_printf("\nFile Start: %x,length=%x\n", image_address,
			    file_length_to_client);
		/*
		   if (memcmp(arptable_tftp[TFTP_CLIENT].node,(Int8*)&(nic.packet[ETH_ALEN]),ETH_ALEN)) {
		   prom_printf("###%s:update CLIENT mac address %02x%02x%02x%02x%02x%02x -> %02x%02x%02x%02x%02x%02x\n",
		   __FUNCTION__, arptable_tftp[TFTP_CLIENT].node[0]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[1]&0xff, arptable_tftp[TFTP_CLIENT].node[2]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[3]&0xff, arptable_tftp[TFTP_CLIENT].node[4]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[5]&0xff,
		   nic.packet[ETH_ALEN]&0xff, nic.packet[ETH_ALEN+1]&0xff,
		   nic.packet[ETH_ALEN+2]&0xff, nic.packet[ETH_ALEN+3]&0xff,
		   nic.packet[ETH_ALEN+4]&0xff, nic.packet[ETH_ALEN+5]&0xff
		   );
		   }
		   if (memcmp(&(arptable_tftp[TFTP_CLIENT].ipaddr.s_addr),(Int8*)&nic.packet[ETH_HLEN+12],4)) {
		   prom_printf("###%s:update CLIENT ip address %d.%d.%d.%d -> %d.%d.%d.%d\n",__FUNCTION__, 
		   arptable_tftp[TFTP_CLIENT].ipaddr.ip[0]&0xff,arptable_tftp[TFTP_CLIENT].ipaddr.ip[1]&0xff,
		   arptable_tftp[TFTP_CLIENT].ipaddr.ip[2]&0xff,arptable_tftp[TFTP_CLIENT].ipaddr.ip[3]&0xff,
		   nic.packet[ETH_HLEN+12]&0xff, nic.packet[ETH_HLEN+13]&0xff,
		   nic.packet[ETH_HLEN+14]&0xff, nic.packet[ETH_HLEN+15]&0xff
		   );
		   }
		 */
		/*memorize CLIENT IP address */
		memcpy(&(arptable_tftp[TFTP_CLIENT].ipaddr.s_addr),
		       (Int8 *) & nic.packet[ETH_HLEN + 12], 4);

		/*memorize CLIENT mac address */
		memcpy(arptable_tftp[TFTP_CLIENT].node,
		       (Int8 *) & (nic.packet[ETH_ALEN]), ETH_ALEN);

		/*memorize CLIENT port */
		CLIENT_port = ntohs(udpheader->src);
		tftppacket = (struct tftp_t *)&nic.packet[ETH_HLEN];
		tftpopcode = tftppacket->opcode;

		for (find_zero = 0; find_zero < TFTP_DEFAULTSIZE_PACKET;
		     find_zero++)
			if (*((Int8 *) (tftppacket->u.rrq) + find_zero) == 0)
				break;

		memcpy(filename, tftppacket->u.rrq, find_zero);
		filename[find_zero] = '\0';
		/*
		   if (memcmp(arptable_tftp[TFTP_CLIENT].node,(Int8*)&(nic.packet[ETH_ALEN]),ETH_ALEN)) {
		   prom_printf("###%s:update CLIENT mac address %02x%02x%02x%02x%02x%02x -> %02x%02x%02x%02x%02x%02x (2)\n",
		   __FUNCTION__, arptable_tftp[TFTP_CLIENT].node[0]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[1]&0xff, arptable_tftp[TFTP_CLIENT].node[2]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[3]&0xff, arptable_tftp[TFTP_CLIENT].node[4]&0xff,
		   arptable_tftp[TFTP_CLIENT].node[5]&0xff,
		   nic.packet[ETH_ALEN]&0xff, nic.packet[ETH_ALEN+1]&0xff,
		   nic.packet[ETH_ALEN+2]&0xff, nic.packet[ETH_ALEN+3]&0xff,
		   nic.packet[ETH_ALEN+4]&0xff, nic.packet[ETH_ALEN+5]&0xff
		   );
		   }
		 */
		memcpy(arptable_tftp[TFTP_CLIENT].node,
		       (Int8 *) & (nic.packet[ETH_ALEN]), ETH_ALEN);

		prom_printf("\n**TFTP GET File %s,Size %X Byte\n", filename,
			    file_length_to_client);
		/*Initialziation of RRQ file */
		//image_address=FILESTART; //sc_yang
		/*now we can use fiile_length_to_client, if we have meet WRQ */
		one_tftp_lock = 1;
		/*we should send a data block numbered 1, waiting for number 1 ACK. */
		tftpd_send_data(filename, 0x0001);
		block_expected = 1;
//prom_printf("line=%d:         block_expected=%d\n", __LINE__,  block_expected); // for debug

		/*now change state to RRQ */
		bootState = BOOT_STATE1_TFTP_CLIENT_RRQ;
	}
}

 /*DEBUG*/
//int upload_start=0;
     /*DEBUG*/
//----------------------------------------------------------------------------------------
    static void
setTFTP_WRQ(void)
{
	struct udphdr *udpheader;

	struct tftp_t *tftppacket;
	Int16 tftpopcode;

	if (!tftpd_is_ready)
		return;

//dprintf("Receive TFTP WRQ\n");

	udpheader =
	    (struct udphdr *)&nic.packet[ETH_HLEN + sizeof (struct iphdr)];
	if (udpheader->dest == htons(TFTP_PORT)) {
		/*memorize CLIENT port */
		CLIENT_port = ntohs(udpheader->src);
		tftppacket = (struct tftp_t *)&nic.packet[ETH_HLEN];

		/*memorize CLIENT mac address */
		memcpy(arptable_tftp[TFTP_CLIENT].node,
		       (Int8 *) & (nic.packet[ETH_ALEN]), ETH_ALEN);
		/*memorize CLIENT IP address */
		memcpy(&(arptable_tftp[TFTP_CLIENT].ipaddr.s_addr),
		       (Int8 *) & nic.packet[ETH_HLEN + 12], 4);
		/*here we can parse the file name if required. */
		prom_printf("\n**TFTP Client Upload, File Name: %s\n",
			    tftppacket->u.wrq);

		/*initializaiton of writing file. */

//#if 0
//    if(!strcmp(tftppacket->u.wrq,TEST_FILENAME))
		if (strstr(tftppacket->u.wrq, TEST_FILENAME)) {
			jump_to_test = 1;
			//image_address=TESTSTART;
		} else if (!strcmp(tftppacket->u.wrq, BOOT_FILENAME)) {
			jump_to_test = 1;
			image_address = BOOTSTART;
		}
#ifdef SUPPORT_TFTP_CLIENT
		else
			jump_to_test = 0;
#endif
//#endif  
		address_to_store = image_address;
		file_length_to_server = 0;
		/*now send one ACK out, to identify this. */
		tftpd_send_ack(0x0000);	/*Block number 0 */
		block_expected = 1;	/*later client will send an Data number 1 */
		//prom_printf("line=%d:             block_expected=%d\n", __LINE__,  block_expected); // for debug

		//now lock the tftp..till upload finished
		one_tftp_lock = 1;
		/*Change state to WRQ state. */
		bootState = BOOT_STATE2_TFTP_CLIENT_WRQ;
	}
}

//----------------------------------------------------------------------------------------
SIGN_T sign_tbl[] = {		//  sigature, name, skip_header, max_len, reboot
	{FW_SIGNATURE, "Linux kernel", SIG_LEN, 0, 0x2C0000, 1},
	{FW_SIGNATURE_WITH_ROOT, "Linux kernel (root-fs)", SIG_LEN, 0, 0x2C0000,
	 1},
	{WEB_SIGNATURE, "Webpages", 3, 0, 0x20000, 0},
	{WEB_JFFS2_SIGNATURE, "JFFS2 web", 4, 1, 0x50000, 0},
	{ROOT_SIGNATURE, "Root filesystem", SIG_LEN, 1, 0x100000, 0},
	{BOOT_SIGNATURE, "Boot code", SIG_LEN, 1, 0x10000, 1},
//BUILDIN_SECURITY_FILE
	{CWMP_SIGNATURE, "cwmp file", SIG_LEN, 0, 0x4000, 0},
	{KSAP_SIGNATURE, "ksap file", SIG_LEN, 0, 0x4000, 0},
//BUILDIN_SECURITY_FILE
	{ALL1_SIGNATURE, "Total Image", SIG_LEN, 1, 0x200000, 1},
	{ALL2_SIGNATURE, "Total Image (no check)", SIG_LEN, 1, 0x200000, 1}
};

#define MAX_SIG_TBL (sizeof(sign_tbl)/sizeof(SIGN_T))
int autoBurn = 1;

//----------------------------------------------------------------------------------------
#if defined(HTTP_SERVER)
int
imageFileValid(unsigned long startAddr, int len)
{
	int i = 0;
	unsigned long head_offset = 0;
	unsigned short sum = 0;
	unsigned char sum1 = 0;
	IMG_HEADER_T Header;	//avoid unalign problem
	int skip_check_signature = 0;

	int found = 0;

	while ((head_offset + sizeof (IMG_HEADER_T)) < len) {
		/*as soon as we found a correct header. we thinks the file is valid */
		memcpy(&Header, ((char *)startAddr + head_offset),
		       sizeof (IMG_HEADER_T));
		if (!skip_check_signature) {
			for (i = 0; i < MAX_SIG_TBL; i++) {

				if (!memcmp
				    (Header.signature,
				     (char *)sign_tbl[i].signature,
				     sign_tbl[i].sig_len)) {
					found++;
					break;
				}

			}
			if (i == MAX_SIG_TBL) {
				head_offset +=
				    Header.len + sizeof (IMG_HEADER_T);
				continue;
			}
		} else {
			if (!memcmp(Header.signature, BOOT_SIGNATURE, SIG_LEN)) {
				found++;
			} else {
				unsigned char *pRoot =
				    ((unsigned char *)startAddr) + head_offset +
				    sizeof (IMG_HEADER_T);
				if (!memcmp(pRoot, SQSH_SIGNATURE, SIG_LEN)) {
					found++;
				}
			}
		}

		if (skip_check_signature ||
		    (memcmp(Header.signature, WEB_SIGNATURE, 3)
		     && memcmp(Header.signature, WEB_JFFS2_SIGNATURE, 4))) {
			//calculate checksum
			if (!memcmp(Header.signature, ALL1_SIGNATURE, SIG_LEN)
			    || !memcmp(Header.signature, ALL2_SIGNATURE,
				       SIG_LEN)) {
				for (i = 0;
				     i < Header.len + sizeof (IMG_HEADER_T);
				     i += 2) {
					sum +=
					    *((unsigned short *)(startAddr +
								 head_offset +
								 i));
				}
			} else {
				unsigned char x = 0, y = 0;
				unsigned short temp = 0;
				for (i = 0; i < Header.len; i += 2) {
					//sum +=*((unsigned short *)(startAddr+ head_offset + sizeof(IMG_HEADER_T) + i));
					memcpy(&temp, (startAddr + head_offset + sizeof (IMG_HEADER_T) + i), 2);	// for alignment issue
					sum += temp;
				}
			}
			if (sum) {
				for (i = 0; i < MAX_SIG_TBL; i++) {

					if (!memcmp
					    (Header.signature,
					     (char *)sign_tbl[i].signature,
					     sign_tbl[i].sig_len)) {
						break;
					}
				}
				SprintF(message,
					"%s imgage checksum error at %X!\n",
					sign_tbl[i].comment,
					startAddr + head_offset);
				return -1;
			}

			if (!memcmp(Header.signature, ALL1_SIGNATURE, SIG_LEN)) {
				found++;
				head_offset += sizeof (IMG_HEADER_T);
				continue;
			}
			if (!memcmp(Header.signature, ALL2_SIGNATURE, SIG_LEN)) {
				found++;
				skip_check_signature = 1;
				head_offset += sizeof (IMG_HEADER_T);
				continue;
			}
		} else {
			//web page use different checksum algorimth
			for (i = 0; i < Header.len; i++)
				sum1 +=
				    *((unsigned char *)(startAddr +
							head_offset +
							sizeof (IMG_HEADER_T) +
							i));
			if (sum1) {
				for (i = 0; i < MAX_SIG_TBL; i++) {
					if (!memcmp
					    (Header.signature,
					     (char *)sign_tbl[i].signature,
					     sign_tbl[i].sig_len)) {
						break;
					}
				}
				SprintF(message,
					"%s imgage checksum error at %X!\n",
					sign_tbl[i].comment,
					startAddr + head_offset);
				return -1;
			}
		}
		head_offset += Header.len + sizeof (IMG_HEADER_T);
	}			//while       
	return found;
}
#endif

void
autoreboot()
{
	jumpF = (void *)(0xbfc00000);
	outl(0, GIMR0);		// mask all interrupt        
	cli();

	flush_cache();
	prom_printf("Rebooting...\n");
	/* this is to enable 865xc watch dog reset */
	*(volatile unsigned long *)(0xB800311c) = 0;
	for (;;) ;		//Disable for NAND flash booting
	/* reboot */
	jumpF();
}

void
checkAutoFlashing(unsigned long startAddr, int len)
{
	int i = 0;
	unsigned long head_offset = 0, srcAddr, burnLen;
	unsigned short sum = 0;
	unsigned char sum1 = 0;
	int skip_header = 0;
	int reboot = 0;
	IMG_HEADER_T Header;	//avoid unalign problem
	int skip_check_signature = 0;
	unsigned long burn_offset = 0;	//mark_dual

	int trueorfaulse = 0;

	while ((head_offset + sizeof (IMG_HEADER_T)) < len) {
		sum = 0;
		sum1 = 0;
		memcpy(&Header, ((char *)startAddr + head_offset),
		       sizeof (IMG_HEADER_T));

		if (!skip_check_signature) {
			for (i = 0; i < MAX_SIG_TBL; i++) {

				if (!memcmp
				    (Header.signature,
				     (char *)sign_tbl[i].signature,
				     sign_tbl[i].sig_len))
					break;

			}
			if (i == MAX_SIG_TBL) {
				head_offset +=
				    Header.len + sizeof (IMG_HEADER_T);
				continue;
			}
			skip_header = sign_tbl[i].skip;
			if (skip_header) {
				srcAddr =
				    startAddr + head_offset +
				    sizeof (IMG_HEADER_T);
				burnLen = Header.len;	// +checksum
			} else {
				srcAddr = startAddr + head_offset;
				burnLen = Header.len + sizeof (IMG_HEADER_T);
			}
			reboot |= sign_tbl[i].reboot;
			prom_printf("\n%s upgrade.\n", sign_tbl[i].comment);
		} else {
			if (!memcmp(Header.signature, BOOT_SIGNATURE, SIG_LEN))
				skip_header = 1;
			else {
				unsigned char *pRoot =
				    ((unsigned char *)startAddr) + head_offset +
				    sizeof (IMG_HEADER_T);
				if (!memcmp(pRoot, SQSH_SIGNATURE, SIG_LEN))
					skip_header = 1;
				else
					skip_header = 0;
			}
			if (skip_header) {
				srcAddr =
				    startAddr + head_offset +
				    sizeof (IMG_HEADER_T);
				burnLen = Header.len;	// +checksum

			} else {
				srcAddr = startAddr + head_offset;
				burnLen = Header.len + sizeof (IMG_HEADER_T);
			}
		}

		if (skip_check_signature ||
		    (memcmp(Header.signature, WEB_SIGNATURE, 3) &&
		     memcmp(Header.signature, WEB_JFFS2_SIGNATURE, 4) &&
//BUILDIN_SECURITY_FILE
		     memcmp(Header.signature, CWMP_SIGNATURE, 4) &&
		     memcmp(Header.signature, KSAP_SIGNATURE, 4))) {
//BUILDIN_SECURITY_FILE
			//calculate checksum
			if (!memcmp(Header.signature, ALL1_SIGNATURE, SIG_LEN)
			    || !memcmp(Header.signature, ALL2_SIGNATURE,
				       SIG_LEN)) {
				for (i = 0;
				     i < Header.len + sizeof (IMG_HEADER_T);
				     i += 2) {
					sum +=
					    *((unsigned short *)(startAddr +
								 head_offset +
								 i));
				}
			} else {
				unsigned char x = 0, y = 0;
				unsigned short temp = 0;

				for (i = 0; i < Header.len; i += 2) {

					//sum +=*((unsigned short *)(startAddr+ head_offset + sizeof(IMG_HEADER_T) + i));
					memcpy(&temp, (void *)(startAddr + head_offset + sizeof (IMG_HEADER_T) + i), 2);	// for alignment issue
					sum += temp;
				}
			}
			if (sum) {
				prom_printf("%s imgage checksum error at %X!\n",
					    Header.signature,
					    startAddr + head_offset);
				return;
			}
			if (!memcmp(Header.signature, ALL1_SIGNATURE, SIG_LEN)) {
				head_offset += sizeof (IMG_HEADER_T);
				continue;
			}
			if (!memcmp(Header.signature, ALL2_SIGNATURE, SIG_LEN)) {
				skip_check_signature = 1;
				head_offset += sizeof (IMG_HEADER_T);
				continue;
			}
		}
		else
		{		//web page use different checksum algorimth

			for (i = 0; i < Header.len; i++)
				sum1 +=
				    *((unsigned char *)(startAddr +
							head_offset +
							sizeof (IMG_HEADER_T) +
							i));
			if (sum1) {
				prom_printf("%s imgage checksum error at %X!\n",
					    Header.signature,
					    startAddr + head_offset);
				return;
			}
		}
		prom_printf("Checksum OK\n");

		if ((burnLen % 0x1000) == 0)	//mean 4k alignemnt
		{
			if ((*((unsigned int *)(startAddr + burnLen))) == 0xdeadc0de)	//wrt jffs2 endof mark
			{
				prom_printf
				    ("it's special wrt image need add 4 byte to burnlen =%8x!\n",
				     burnLen);
				burnLen += 4;
			}
		}

		prom_printf("Writing 0x%x bytes to flash @ 0x%x\n",
			    burnLen, Header.burnAddr);

//int trueorfaulse = 0;
#ifdef SUPPORT_SPI_MIO_8198_8196C
		if (Header.burnAddr + burn_offset + burnLen >
		    spi_flash_info[0].chip_size) {
			if (spi_flw_image_mio_8198
			    (0, Header.burnAddr + burn_offset,
			     (unsigned char *)srcAddr,
			     spi_flash_info[0].chip_size - (Header.burnAddr +
							    burn_offset))
			    && spi_flw_image_mio_8198(1, 0,
						      (unsigned char *)(srcAddr +
						      spi_flash_info[0].
						      chip_size -
						      (Header.burnAddr +
						       burn_offset)),
						      Header.burnAddr +
						      burn_offset + burnLen -
						      spi_flash_info[0].
						      chip_size))
				trueorfaulse = 1;
		} else
		    if (spi_flw_image_mio_8198
			(0, Header.burnAddr + burn_offset,
			 (unsigned char *)srcAddr, burnLen))
			trueorfaulse = 1;
#else
		if (spi_flw_image
		    (0, Header.burnAddr + burn_offset, srcAddr, burnLen))
#endif

		if (trueorfaulse)
			prom_printf("Flash write OK\n");
		else {
			prom_printf("Flash write FAILED\n");
			return;
		}

		head_offset += Header.len + sizeof (IMG_HEADER_T);
	}			//while
	if (reboot) {
		autoreboot();
	}

}

/*Cyrus Tsai*/
extern int flashwrite(unsigned long FLASH, unsigned long SDRAM,
		      unsigned long length);
extern void ddump(unsigned char *pData, int len);
//----------------------------------------------------------------------------------------
/*Cyrus Tsai*/
/*Why we prepare ACK, because we have data uploaded by client*/

static void
prepareACK(void)
{
	struct udphdr *udpheader;
	struct tftp_t *tftppacket;
	Int16 tftpopcode;
	Int32 tftpdata_length;
	volatile Int16 block_received = 0;

	if (!tftpd_is_ready)
		return;

	//dprintf("Receive TFTP Data\n");

	udpheader =
	    (struct udphdr *)&nic.packet[ETH_HLEN + sizeof (struct iphdr)];
#ifdef SUPPORT_TFTP_CLIENT
	if ((tftp_client_enabled && (udpheader->dest == htons(CLIENT_port))) ||
	    (!tftp_client_enabled && (udpheader->dest == htons(SERVER_port))))
#else
	if (udpheader->dest == htons(SERVER_port))
#endif
	{
#ifdef SUPPORT_TFTP_CLIENT
		if (tftp_client_enabled)
			SERVER_port = ntohs(udpheader->src);
		else
#endif
			/*memorize CLIENT port */
			CLIENT_port = ntohs(udpheader->src);
		tftppacket = (struct tftp_t *)&nic.packet[ETH_HLEN];
		/*no need to check opcode, this is a Data packet */
		/*parse the TFTP block number */
		block_received = tftppacket->u.data.block;
//prom_printf("line=%d:         block_received=%d\n", __LINE__,  block_received); // for debug

		if (block_received != (block_expected)) {
			//prom_printf("line=%d: block_received=%d, block_expected=%d\n", __LINE__,  block_received,  block_expected); // for debug
			prom_printf("TFTP #\n");
			/*restore the block number */
			tftpd_send_ack(block_expected - 1);
		} else {

			tftpdata_length =
			    ntohs(udpheader->len) - 4 - sizeof (struct udphdr);
			/*put the image into memory address */
			memcpy((void *)address_to_store,
			       tftppacket->u.data.download, tftpdata_length);

			// ddump(address_to_store, tftpdata_length);
			//prom_printf("a %x. l %x\n",address_to_store,tftpdata_length);

			address_to_store = address_to_store + tftpdata_length;
			/*use this to count the image bytes */
			file_length_to_server =
			    file_length_to_server + tftpdata_length;
			/*this is for receiving one packet */
			//prom_printf("%x.\n",address_to_store);
			twiddle();
			//prom_printf(" <- ");
			//prom_printf("%x. %x. %x\n",block_expected,address_to_store,tftpdata_length);

			tftpd_send_ack(block_expected);
			block_expected = block_expected + 1;
//prom_printf("line=%d:         block_expected=%d\n", __LINE__,  block_expected); // for debug

			/*remember to check if it is the last packet */
			if (tftpdata_length < TFTP_DEFAULTSIZE_PACKET) {
				prom_printf
				    ("\n**TFTP Client Upload File Size = %X Bytes at %X\n",
				     file_length_to_server, image_address);
				/*change the boot state back to orignal, and some variables also */
				nic.packet = eth_packet;
				nic.packetlen = 0;
				block_expected = 0;
//prom_printf("line=%d:         block_expected=%d\n", __LINE__,  block_expected); // for debug

				/*reset the file position */
				//image_address=FILESTART;
				address_to_store = image_address;
				file_length_to_client = file_length_to_server;
				/*file_length_to_server can not be reset,only when another WRQ */
				/*and export to file_length_to_client for our SDRAM direct RRQ */
				it_is_EOF = 0;
#if defined(SUPPORT_TFTP_CLIENT)
				if (tftp_from_command)
					tftp_client_recvdone = 1;
#endif
				bootState = BOOT_STATE0_INIT_ARP;
				/*Cyrus Tsai */
				one_tftp_lock = 0;
				SERVER_port++;

#if defined(SUPPORT_TFTP_CLIENT)
				if (tftp_from_command)
					prom_printf("\nSuccess!\n");
				else
#endif
					prom_printf("\nSuccess!\n%s",
						    "<RealTek>");

				if (jump_to_test == 1) {
					REG32(0xb8000010) =
					    REG32(0xb8000010) & (~(1 << 11));
					jump_to_test = 0;
					/*we should clear all irq mask. */
					//jumpF = (void *)(TESTSTART); //sc_yang
					jumpF = (void *)(image_address);
					/*we should clear all irq mask. */
					outl(0, GIMR0);	// mask all interrupt            
					cli();

					dprintf("Jump to 0x%x\n",
						image_address);
					flush_cache();
					jumpF();
				}
				else if (autoBurn) {
					checkAutoFlashing(image_address,
							  file_length_to_server);
				}
			}
#ifdef SUPPORT_TFTP_CLIENT
			else if (tftp_client_enabled && block_expected == 2) {	/* first block */
				bootState = BOOT_STATE4_TFTP_SERVER_DATA;
				if (!tftp_from_command)
					jump_to_test = 1;
			}
			last_sent_time = get_timer_jiffies();
#endif
		}
	}
//else 
//   prom_printf("\n**TFTP port number error");   

}

//----------------------------------------------------------------------------------------
/*Why we prepare DATA, because we receive the ACK*/
static void
prepareDATA(void)
{
	/*support CLIENT RRQ now. */
	struct udphdr *udpheader;
	struct tftp_t *tftppacket;
	Int16 tftpopcode;
	Int32 tftpdata_length;
	Int16 block_received = 0;

	if (!tftpd_is_ready)
		return;
	udpheader =
	    (struct udphdr *)&nic.packet[ETH_HLEN + sizeof (struct iphdr)];
	if (udpheader->dest == htons(SERVER_port)) {
		/*memorize CLIENT port */
		CLIENT_port = ntohs(udpheader->src);
		tftppacket = (struct tftp_t *)&nic.packet[ETH_HLEN];
		/*no need to check opcode, this is a ACK packet */
		/*parse the TFTP ACK number */
		block_received = tftppacket->u.ack.block;
		if (block_received != (block_expected)) {
			//prom_printf("line=%d: block_received=%d, block_expected=%d\n", __LINE__,  block_received,  block_expected); // for debug
			prom_printf("\n**TFTP #\n");
			tftpd_send_data(filename, block_expected);
		} else {
			block_expected = block_expected + 1;
			if (!(it_is_EOF))
				tftpd_send_data(filename, block_expected);
			else {
				/*After we receive the last ACK then we can go on. */
				bootState = BOOT_STATE0_INIT_ARP;
				one_tftp_lock = 0;
				//prom_printf("\n**TFTP Client Upload Success! File Size = %X Bytes\n",file_length_to_server);                        
				prom_printf
				    ("\n*TFTP Client Download Success! File Size = %X Bytes\n",
				     file_length_to_client);
				prom_printf(".Success!\n%s", "<RealTek>");
				nic.packet = eth_packet;
				nic.packetlen = 0;
				block_expected = 0;
//prom_printf("line=%d:         block_expected=%d\n", __LINE__,  block_expected); // for debug

				it_is_EOF = 0;
			}
		}
	}
//else 
//   prom_printf("\n**TFTP port number error\n");   

}

//----------------------------------------------------------------------------------------
//char eth0_mac[6]={0x56, 0xaa, 0xa5, 0x5a, 0x7d, 0xe8};
extern char eth0_mac[6];
void
tftpd_entry(int is_client_mode)
{
	int i, j;

	arptable_tftp[TFTP_SERVER].ipaddr.s_addr = IPTOUL(192, 168, 1, 6);

	arptable_tftp[TFTP_CLIENT].ipaddr.s_addr = IPTOUL(192, 162, 1, 116);
	/*This is ETH0. we treat ETH0 as the TFTP server */
	/*char eth0_mac[6]={0x56, 0xaa, 0xa5, 0x5a, 0x7d, 0xe8}; */
	arptable_tftp[TFTP_SERVER].node[5] = eth0_mac[5];
	arptable_tftp[TFTP_SERVER].node[4] = eth0_mac[4];
	arptable_tftp[TFTP_SERVER].node[3] = eth0_mac[3];
	arptable_tftp[TFTP_SERVER].node[2] = eth0_mac[2];
	arptable_tftp[TFTP_SERVER].node[1] = eth0_mac[1];
	arptable_tftp[TFTP_SERVER].node[0] = eth0_mac[0];

//arptable_tftp[TFTP_SERVER].node[5]=0xe8;
//arptable_tftp[TFTP_SERVER].node[4]=0x7d;
//arptable_tftp[TFTP_SERVER].node[3]=0x5a;
//arptable_tftp[TFTP_SERVER].node[2]=0xa5;
//arptable_tftp[TFTP_SERVER].node[1]=0xaa;
//arptable_tftp[TFTP_SERVER].node[0]=0x56;

	/*intialize boot state */
	bootState = BOOT_STATE0_INIT_ARP;
	/*this nic is the expected data structure to be processed. */
	nic.packet = eth_packet;
	nic.packetlen = 0;

	block_expected = 0;
	one_tftp_lock = 0;
	it_is_EOF = 0;

//prom_printf("line=%d:         block_expected=%d\n", __LINE__,  block_expected); // for debug

	//image_address=FILESTART; //sc_yang
	address_to_store = image_address;

	file_length_to_server = 0;
	file_length_to_client = 0;

#ifdef SUPPORT_TFTP_CLIENT
	if (is_client_mode)
		SERVER_port = 69;
	else
#endif
		SERVER_port = 2098;

	tftpd_is_ready = 1;

#ifdef SUPPORT_TFTP_CLIENT
	if (is_client_mode) {
		int start_time;
		arptable_tftp[TFTP_SERVER].ipaddr.s_addr =
		    IPTOUL(192, 168, 1, 97);
		arptable_tftp[TFTP_CLIENT].ipaddr.s_addr =
		    IPTOUL(192, 168, 1, 116);
		memset(arptable_tftp[TFTP_SERVER].node, '\xff', ETH_ALEN);
		arptable_tftp[TFTP_CLIENT].node[5] = eth0_mac[5];
		arptable_tftp[TFTP_CLIENT].node[4] = eth0_mac[4];
		arptable_tftp[TFTP_CLIENT].node[3] = eth0_mac[3];
		arptable_tftp[TFTP_CLIENT].node[2] = eth0_mac[2];
		arptable_tftp[TFTP_CLIENT].node[1] = eth0_mac[1];
		arptable_tftp[TFTP_CLIENT].node[0] = eth0_mac[0];
		CLIENT_port = 1010;
		tftp_client_enabled = 1;
	} else
		tftp_client_enabled = 0;
#endif
}

//----------------------------------------------------------------------------------------
void
tftpd_send_ack(Int16 number)
{
	/*UDP source port: SERVER_port */
	/*UDP target port: CLIENT_port */
	struct iphdr *ip;
	struct udphdr *udp;
	struct tftp_t tftp_tx;
	/*generate the TFTP body */
	tftp_tx.opcode = htons(TFTP_ACK);
	tftp_tx.u.ack.block = htons(number);

	ip = (struct iphdr *)&tftp_tx;
	udp = (struct udphdr *)((Int8 *) & tftp_tx + sizeof (struct iphdr));

	/*IP header */
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(32);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = 60;
	ip->protocol = IP_UDP;
	ip->chksum = 0;
#ifdef SUPPORT_TFTP_CLIENT
	if (tftp_client_enabled) {
		ip->src.s_addr = arptable_tftp[TFTP_CLIENT].ipaddr.s_addr;
		ip->dest.s_addr = arptable_tftp[TFTP_SERVER].ipaddr.s_addr;
	} else
#endif
	{
		ip->src.s_addr = arptable_tftp[TFTP_SERVER].ipaddr.s_addr;
		ip->dest.s_addr = arptable_tftp[TFTP_CLIENT].ipaddr.s_addr;
	}
	ip->chksum =
	    ipheader_chksum((Int16 *) & tftp_tx, sizeof (struct iphdr));
	/*generate the UDP header */

#ifdef SUPPORT_TFTP_CLIENT
	if (tftp_client_enabled) {
		udp->src = htons(CLIENT_port);
		udp->dest = htons(SERVER_port);
	} else
#endif
	{
		udp->src = htons(SERVER_port);
		udp->dest = htons(CLIENT_port);
	}
	udp->len = htons(32 - sizeof (struct iphdr));	/*TFTP IP packet is 32 bytes. */
	udp->chksum = 0;

#ifdef SUPPORT_TFTP_CLIENT
	if (tftp_client_enabled)
		prepare_txpkt(0, FRAME_IP, arptable_tftp[TFTP_SERVER].node,
			      (Int8 *) & tftp_tx,
			      (Int16) sizeof (struct iphdr) +
			      sizeof (struct udphdr) + 4);
	else
#endif
		prepare_txpkt(0, FRAME_IP, arptable_tftp[TFTP_CLIENT].node,
			      (Int8 *) & tftp_tx,
			      (Int16) sizeof (struct iphdr) +
			      sizeof (struct udphdr) + 4);
}

//----------------------------------------------------------------------------------------
void
tftpd_send_data(char *filename, Int16 block_number)
{
	/*because we only have 1 image supported */
	/*do nothing to char* file is of no use. */
	/*UDP source port: SERVER_port */
	/*UDP target port: CLIENT_port */

	struct iphdr *ip;
	struct udphdr *udp;
	struct tftp_t tftp_tx;
	Int32 *data;
	int length;

 /********************************************/
	data = (Int32 *) (image_address + 512 * (block_number - 1));
	//prom_printf("send data start at %x\n",data);
	if (512 * block_number == (file_length_to_client + 512)) {
		/*it is over that means a length=0 data is required */
		length = 0;
		//prom_printf("TFTP RRQ last NULL data to send\n");
		it_is_EOF = 1;
	} else if (512 * block_number > file_length_to_client) {
		length = file_length_to_client - 512 * (block_number - 1);
		//prom_printf("TFTP RRQ last data to send\n");
		it_is_EOF = 1;
	} else
		length = 512;

 /********************************************/
	/*generate the TFTP body */
	tftp_tx.opcode = htons(TFTP_DATA);
	memcpy(tftp_tx.u.data.download, (Int8 *) data, length);
	tftp_tx.u.data.block = htons(block_number);

	ip = (struct iphdr *)&tftp_tx;
	udp = (struct udphdr *)((Int8 *) & tftp_tx + sizeof (struct iphdr));

	/*generate the IP header */
	ip->verhdrlen = 0x45;
	ip->service = 0;
	ip->len = htons(32 + length);
	ip->ident = 0;
	ip->frags = 0;
	ip->ttl = 60;
	ip->protocol = IP_UDP;
	ip->chksum = 0;
	ip->src.s_addr = arptable_tftp[TFTP_SERVER].ipaddr.s_addr;
	ip->dest.s_addr = arptable_tftp[TFTP_CLIENT].ipaddr.s_addr;
	ip->chksum =
	    ipheader_chksum((Int16 *) & tftp_tx, sizeof (struct iphdr));
	/*generate the UDP header */
	udp->src = htons(SERVER_port);
	udp->dest = htons(CLIENT_port);
	udp->len = htons(length + 4 + 8);
	udp->chksum = 0;

	/*use twiddle here */
	twiddle();
	//prom_printf(" -> ");

	prepare_txpkt(0, FRAME_IP, arptable_tftp[TFTP_CLIENT].node,
		      (Int8 *) & tftp_tx,
		      (Int16) sizeof (struct iphdr) + sizeof (struct udphdr) +
		      length + 4);
}

//----------------------------------------------------------------------------------------
void
kick_tftpd(void)
{
	/*We always have the global nic structure, never change it directly */
	int i;
	/*First of all parse the packet type */
	/*that is the first 13 and 14 byte that is IP:(UDP) 0800 ARP 0806 */
	Int16 pkttype = 0;
	struct arprequest *arppacket;
	Int16 arpopcode;
	struct tftp_t *tftppacket;
	Int16 tftpopcode;

	struct iphdr *ipheader;

	struct udphdr *udpheader;
	//Cyrus Dick
	in_addr ip_addr;
	// in_addr source_ip_addr;

	void (*jump)(void);
	BootEvent_t kick_event = NUM_OF_BOOT_EVENTS;

	Int32 UDPIPETHheader =
	    ETH_HLEN + sizeof (struct iphdr) + sizeof (struct udphdr);

	if (nic.packetlen >= ETH_HLEN + sizeof (struct arprequest)) {
		pkttype = ((Int16) (nic.packet[12] << 8) | (Int16) (nic.packet[13]));	/*This BIG byte shifts right 8 */
	}

	switch (pkttype) {
		//--------------------------------------------------------------------------
	case htons(FRAME_ARP):
		// for debug                    
		//              dprintf("rx arp packet\n");     //wei add

		/*keep parsing, check the opcode is request or reply */
		arppacket = (struct arprequest *)&nic.packet[ETH_HLEN];
		/*Parse the opcode, 01->req, 02 ->reply */
		arpopcode = arppacket->opcode;

		switch (arpopcode) {
		case htons(ARP_REQUEST):
			// check dst ip, david+2007-12-26                                                                                   
			if (!memcmp
			    (arppacket->tipaddr,
			     &arptable_tftp[TFTP_SERVER].ipaddr, 4)
#ifdef SUPPORT_TFTP_CLIENT
			    || (tftp_client_enabled
				&& !memcmp(arppacket->tipaddr,
					   &arptable_tftp[TFTP_CLIENT].ipaddr,
					   4))
#endif
#if defined(HTTP_SERVER)	/*for httpd */
			    || !memcmp(arppacket->tipaddr,
				       &arptable_tftp[HTTPD_ARPENTRY].ipaddr, 4)
#endif
			    )
				kick_event = BOOT_EVENT0_ARP_REQ;

			//doARPReply();     //wei add
			//jump = (void *)(*BootStateEvent[bootState][BOOT_EVENT0_ARP_REQ]);
			//jump();
			break;
		case htons(ARP_REPLY):
			kick_event = BOOT_EVENT1_ARP_REPLY;
			//jump =(*BootStateEvent[bootState][BOOT_EVENT1_ARP_REPLY]);
			//jump();
			break;
		}
		//wei del
		if (kick_event != NUM_OF_BOOT_EVENTS) {
			jump = (void *)(*BootStateEvent[bootState][kick_event]);
			jump();
		}
		break;		/*ptype=ARP */

		//--------------------------------------------------------------------------    
	case htons(FRAME_IP):
		//dprintf("rx ip packet\n");        //wei add
		ipheader = (struct iphdr *)&nic.packet[ETH_HLEN];
		// word alignment
		//Cyrus Dick
		ip_addr.ip[0] = ipheader->dest.ip[0];
		ip_addr.ip[1] = ipheader->dest.ip[1];
		ip_addr.ip[2] = ipheader->dest.ip[2];
		ip_addr.ip[3] = ipheader->dest.ip[3];
		//source_ip_addr.ip[0] = ipheader->src.ip[0];
		//source_ip_addr.ip[1] = ipheader->src.ip[1];
		//source_ip_addr.ip[2] = ipheader->src.ip[2];
		//source_ip_addr.ip[3] = ipheader->src.ip[3];
		//Cyrus Dick
		/*Even type is IP, but the total payload must at least UDPH+IPH */
		if (nic.packetlen > UDPIPETHheader) {
			/*keep parsing, check the TCP/UDP, here is meaningful */
			if (ipheader->verhdrlen == 0x45) {
				//Cyrus Dick
				/*check the destination ip addr */
#ifdef SUPPORT_TFTP_CLIENT
				if ((tftp_client_enabled
				     && ip_addr.s_addr ==
				     arptable_tftp[TFTP_CLIENT].ipaddr.s_addr)
				    || (!tftp_client_enabled
					&& ip_addr.s_addr ==
					arptable_tftp[TFTP_SERVER].ipaddr.
					s_addr)
#if defined(DHCP_SERVER)	/*for DHCP dst ip  broadcast */
				    || ip_addr.s_addr == 0xFFFFFFFF
#endif
#if defined(HTTP_SERVER)	/*for httpd */
				    || ip_addr.s_addr ==
				    arptable_tftp[HTTPD_ARPENTRY].ipaddr.s_addr
#endif
				    ) {

#else
				if (ip_addr.s_addr ==
				    arptable_tftp[TFTP_SERVER].ipaddr.s_addr
#if defined(DHCP_SERVER)	/*for DHCP dst ip  broadcast */
				    || ip_addr.s_addr == 0xFFFFFFFF
#endif
#if defined(HTTP_SERVER)	/*for httpd */
				    || ip_addr.s_addr ==
				    arptable_tftp[HTTPD_ARPENTRY].ipaddr.s_addr
#endif
				    ) {
#endif
					//if(source_ip_addr.s_addr==arptable_tftp[TFTP_CLIENT].ipaddr.s_addr)
					//Cyrus Dick
					if (!ipheader_chksum
					    ((Int16 *) ipheader,
					     sizeof (struct iphdr))) {
						if (ipheader->protocol ==
						    IP_UDP) {
							/*udpheader = (struct udphdr *)&nic.packet[ETH_HLEN+ sizeof(struct iphdr)]; */
#ifdef DHCP_SERVER
							udpheader =
							    (struct udphdr *)
							    &nic.
							    packet[ETH_HLEN +
								   sizeof
								   (struct
								    iphdr)];
							if ( /*DHCP server port */ 67 == ntohs(udpheader->dest)) {
								dhcps_input();
								return;
							}
#endif
							/*All we care is TFTP protocol, no other  protocol */
							tftppacket =
							    (struct tftp_t *)
							    &nic.
							    packet[ETH_HLEN];
							tftpopcode =
							    tftppacket->opcode;
							switch (tftpopcode) {
							case htons(TFTP_RRQ):
								if (one_tftp_lock == 0)
									kick_event
									    =
									    BOOT_EVENT2_TFTP_RRQ;
								break;
							case htons(TFTP_WRQ):
								if (one_tftp_lock == 0) {
									kick_event
									    =
									    BOOT_EVENT3_TFTP_WRQ;
									rx_kickofftime = get_timer_jiffies();	//wei add
								} else {
									// prom_printf("TFTP_WRQ: one_tftp_lock=%d block_expected=%d\n",one_tftp_lock, block_expected);
									//fix TFTP WRQ retransmit issue and add timout mechanism for second TFTP WRQ coming issue
									if ((block_expected == 1) || ((get_timer_jiffies() - rx_kickofftime) > 2000)) {	//wait 20sec, unit is 10ms
										kick_event
										    =
										    BOOT_EVENT3_TFTP_WRQ;
										rx_kickofftime
										    =
										    get_timer_jiffies
										    ();
									}
								}
								//setTFTP_WRQ();
								break;
							case htons(TFTP_DATA):
								// for debug
								kick_event =
								    BOOT_EVENT4_TFTP_DATA;
								rx_kickofftime =
								    get_timer_jiffies
								    ();
								//      prepareACK();
								break;
							case htons(TFTP_ACK):
								kick_event =
								    BOOT_EVENT5_TFTP_ACK;
								break;
							case htons(TFTP_ERROR):
#if defined(SUPPORT_TFTP_CLIENT)
								if (bootState ==
								    BOOT_STATE3_TFTP_SERVER_DATA
								    || bootState
								    ==
								    BOOT_STATE4_TFTP_SERVER_DATA)
								{
									memset
									    (errmsg,
									     0,
									     512);
									errcode
									    = 0;
									strcpy
									    (errmsg,
									     tftppacket->
									     u.
									     err.
									     errmsg);
									errcode
									    =
									    tftppacket->
									    u.
									    err.
									    errcode;
								}
#endif
								kick_event =
								    BOOT_EVENT6_TFTP_ERROR;
								break;
							case htons(TFTP_OACK):
								kick_event =
								    BOOT_EVENT7_TFTP_OACK;
								break;
							}

							if (kick_event !=
							    NUM_OF_BOOT_EVENTS)
							{
								jump =
								    (void
								     *)
								    (*BootStateEvent
								     [bootState]
								     [kick_event]);
								jump();
							}
						}	/*UDP packet,all TFTP case. */
#ifdef HTTP_SERVER
						else if (IP_TCP ==
							 ipheader->protocol) {
							tcpinput();
						}
#endif
					}
				}
			}
		}
		break;		/*ptype=IP */
	}
}

//----------------------------------------------------------------------------------------
Int16
ipheader_chksum(Int16 * ip, int len)
{
	Int32 sum = 0;
	len >>= 1;
	while (len--) {
		sum += *(ip++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}			/*Correct return 0 */
	return ((~sum) & 0x0000FFFF);	/*only 2 bytes */
}

//----------------------------------------------------------------------------------------
