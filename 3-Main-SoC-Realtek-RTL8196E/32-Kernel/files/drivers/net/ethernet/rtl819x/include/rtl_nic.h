/*
 * RTL8196E NIC Driver Header
 * Copyright (c) 2003-2011 Realtek Semiconductor Corporation
 * Adapted for Linux 5.10 & RTL8196E: Jacques Nilo (2025)
 *
 * Public definitions for RTL8196E Ethernet driver.
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef RTL_NIC_H
#define RTL_NIC_H

#include "rtl865x_netif.h"
/************************************
 *	feature enable/disable
 *************************************/
#define RX_TASKLET 1
#define TX_TASKLET 1
#define LINK_TASKLET 1
#define RTL819X_PRIV_IOCTL_ENABLE 1 /* mark_add */
#define CONFIG_RTL_PHY_PATCH 1
#define RTK_QUE 1
/*
 * Total SKB pool size for multiple parallel TCP streams:
 *
 * Configuration V3 (after discovering kernel buffer retention):
 * - RX descriptors: NUM_RX_PKTHDR_DESC (500)
 * - Pre-alloc queue: MAX_PRE_ALLOC_RX_SKB (300)
 * - Extra margin: 690 (increased to account for kernel buffering)
 *
 * Total: 500 + 300 + 690 = 1490 → ~1500 SKBs
 * Memory: 1500 × 2KB = 3 MB (9.4% of 32MB RAM)
 *
 * Rationale for increase from 1100 to 1500:
 * Testing revealed pool exhausted to 0 free before traffic tests even started.
 * Analysis showed ~290 buffers retained by kernel subsystems:
 *   - TCP socket buffers (SSH, iperf, services): ~50-100
 *   - Qdisc layers (txqueuelen=1000): ~100-200
 *   - Network stack (routing, netfilter): ~50
 *   - TCP retransmit queues (high retrans rate): ~50-100
 *
 * With 1500 buffers:
 * - 510 in RX descriptor rings (hardware DMA)
 * - 300 in refill queue (ready for immediate use)
 * - 690 available for kernel → ~400 free under normal load
 *
 * This handles:
 * - 8 parallel TCP connections (iperf -P 8) with heavy kernel buffering
 * - 95+ Mbps UDP stress tests
 * - High TCP retransmission scenarios (>20%)
 * - mDNS/SSDP broadcast storms
 * - Zigbee2MQTT + Home Assistant traffic
 */
#define MAX_ETH_SKB_NUM ( \
	NUM_RX_PKTHDR_DESC + NUM_RX_PKTHDR_DESC1 + NUM_RX_PKTHDR_DESC2 + NUM_RX_PKTHDR_DESC3 + NUM_RX_PKTHDR_DESC4 + NUM_RX_PKTHDR_DESC5 + MAX_PRE_ALLOC_RX_SKB + 690)
/* Kernel 5.4: NET_SKB_PAD uses max() which is braced-group, can't use in array declaration
 * On MIPS, L1_CACHE_BYTES=32, so NET_SKB_PAD=32. Use constant instead. */
#define ETH_SKB_BUF_SIZE 2048  /* Was: SKB_DATA_ALIGN(CROSS_LAN_MBUF_LEN + sizeof(struct skb_shared_info) + 160 + 32) */
#define ETH_MAGIC_CODE "819X"
#define ETH_MAGIC_LEN 4

struct re865x_priv
{
	u16 ready;
	u16 addIF;
	u16 devnum;
	u32 sec_count;
	u32 sec;
	struct net_device *dev[ETH_INTF_NUM];
	// spinlock_t		lock;
	void *regs;
	struct tasklet_struct rx_tasklet;
	struct timer_list timer; /* Media monitoring timer. */
	unsigned long linkchg;
};

struct dev_priv
{
	u32 id;		  /* VLAN id, not vlan index */
	u32 portmask; /* member port mask */
	u32 portnum;  /* number of member ports */
	u32 netinit;
	struct net_device *dev;
	struct net_device *dev_prev;
	struct net_device *dev_next;

	/* Phase 2: NAPI migration (v3.5.0) */
	struct napi_struct napi;		/* Combined RX+TX NAPI polling */
	struct tasklet_struct link_dsr_tasklet;	/* Link changes (not NAPI) */

	spinlock_t lock;
	u32 msg_enable;
	u32 opened;
	u32 irq_owner; // record which dev request IRQ
	struct net_device_stats net_stats;
	struct timer_list expire_timer;

	/* Phase 2: Descriptor error statistics */
	unsigned long rx_desc_null_errors;      /* NULL descriptor pointers in RX */
	unsigned long rx_mbuf_null_errors;      /* NULL mbuf pointers in RX */
	unsigned long rx_skb_null_errors;       /* NULL skb pointers in RX */
	unsigned long rx_desc_index_errors;     /* RX descriptor index out of bounds */
	unsigned long rx_mbuf_index_errors;     /* RX mbuf index out of bounds */
	unsigned long rx_length_errors;         /* Invalid packet length */
	unsigned long tx_desc_null_errors;      /* NULL descriptor pointers in TX */
	unsigned long tx_mbuf_null_errors;      /* NULL mbuf pointers in TX */
	unsigned long tx_desc_index_errors;     /* TX descriptor index out of bounds */
	unsigned long tx_ring_full_errors;      /* TX ring full events */
	unsigned long ring_recovery_count;      /* Number of ring recoveries */
	unsigned long last_recovery_jiffies;    /* Last recovery timestamp */

	/* Phase 6: Buffer pool monitoring (freeze debugging) */
	unsigned long rx_refill_failures;       /* refill_rx_skb() allocation failures */
	unsigned long rx_pool_empty_events;     /* Times eth_skb_free_num == 0 */
	int last_eth_skb_free_num;             /* Snapshot of buffer pool size */
};

typedef struct __rtlInterruptRxData
{
} rtlInterruptRxData;

/*	define return value		*/
#define RTL_RX_PROCESS_RETURN_SUCCESS 0
#define RTL_RX_PROCESS_RETURN_CONTINUE -1
#define RTL_RX_PROCESS_RETURN_BREAK -2

#define RTL819X_IOCTL_READ_PORT_STATUS (SIOCDEVPRIVATE + 0x01)
#define RTL819X_IOCTL_READ_PORT_STATS (SIOCDEVPRIVATE + 0x02)

struct lan_port_status
{
	unsigned char link;
	unsigned char speed;
	unsigned char duplex;
	unsigned char nway;
};

struct port_statistics
{
	unsigned int rx_bytes;
	unsigned int rx_unipkts;
	unsigned int rx_mulpkts;
	unsigned int rx_bropkts;
	unsigned int rx_discard;
	unsigned int rx_error;
	unsigned int tx_bytes;
	unsigned int tx_unipkts;
	unsigned int tx_mulpkts;
	unsigned int tx_bropkts;
	unsigned int tx_discard;
	unsigned int tx_error;
};
#define RTL_PPTPL2TP_VLANID 999

// flowing name in protocol stack DO NOT duplicate
#define RTL_PS_BR0_DEV_NAME RTL_BR_NAME
#define RTL_PS_ETH_NAME "eth"
#define RTL_PS_WLAN_NAME RTL_WLAN_NAME
#define RTL_PS_PPP_NAME "ppp"
#define RTL_PS_LAN_P0_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_ETH_NAME, 0)
#define RTL_PS_WAN0_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_ETH_NAME, 1)
#define RTL_PS_PPP0_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_PPP_NAME, 0)
#define RTL_PS_PPP1_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_PPP_NAME, 1)
#define RTL_PS_WLAN0_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_WLAN_NAME, 0)
#define RTL_PS_WLAN1_DEV_NAME RTL_DEV_NAME_NUM(RTL_PS_WLAN_NAME, 1)
// Used by fastpath mac-based qos under IMPROVE_QOS
#define QOS_LAN_DEV_NAME RTL_PS_BR0_DEV_NAME

struct rtl865x_vlanConfig
{
	uint8 ifname[IFNAMSIZ];
	uint8 isWan;
	uint16 if_type;
	uint16 vid;
	uint16 fid;
	uint32 memPort;
	uint32 untagSet;
	uint32 mtu;
	ether_addr_t mac;
	uint8 is_slave;
};
#define RTL865X_CONFIG_END {"", 0, 0, 0, 0, 0, 0, 0, {{0}}, 0}
#define GATEWAY_MODE 0
#define BRIDGE_MODE 1
#define WISP_MODE 2
#define CONFIG_CHECK(expr)                                                        \
	do                                                                            \
	{                                                                             \
		if (((int32)expr) != SUCCESS)                                             \
		{                                                                         \
			rtlglue_printf("Error >>> %s:%d failed !\n", __FUNCTION__, __LINE__); \
			return FAILED;                                                        \
		}                                                                         \
	} while (0)

#define INIT_CHECK(expr)                                                          \
	do                                                                            \
	{                                                                             \
		if (((int32)expr) != SUCCESS)                                             \
		{                                                                         \
			rtlglue_printf("Error >>> %s:%d failed !\n", __FUNCTION__, __LINE__); \
			return FAILED;                                                        \
		}                                                                         \
	} while (0)

typedef struct _ps_drv_netif_mapping_s
{
	uint32 valid : 1,			  // entry enable?
		flags;					  // reserverd
	struct net_device *ps_netif;  // linux ps network interface
	char drvName[MAX_IFNAMESIZE]; // netif name in driver

} ps_drv_netif_mapping_t;

/* ========================================
 * Function Declarations (compiler-driven)
 * ======================================== */

#define CONFIG_RTL_NIC_HWSTATS
#define CONFIG_RTL_CUSTOM_PASSTHRU

#endif
