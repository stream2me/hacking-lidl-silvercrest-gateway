/*
 * RTL865x FDB API
 * Copyright (c) 2011 Realtek Semiconductor Corporation
 * Adapted for Linux 5.10 & RTL8196E: Jacques Nilo (2025)
 *
 * L2 forwarding database API definitions.
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef RTL865X_FDB_API_H
#define RTL865X_FDB_API_H
#define RTL_LAN_FID 0
#define RTL_WAN_FID 0
#define FDB_STATIC 0x01	 /* flag for FDB: process static entry only */
#define FDB_DYNAMIC 0x02 /* flag for FDB: process dynamic entry only */
#define RTL865x_FDB_NUMBER 4
#define RTL865x_L2_TYPEI 0x0001	  /* Referenced by ARP/PPPoE */
#define RTL865x_L2_TYPEII 0x0002  /* Referenced by Protocol */
#define RTL865x_L2_TYPEIII 0x0004 /* Referenced by PCI/Extension Port */
#define CONFIG_RTL865X_SYNC_L2 1
#define RTL865X_FDBENTRY_TIMEOUT 0x1001 /*fdb entry time out*/
#define RTL865X_FDBENTRY_450SEC 0x1002	/*fdb entry 450s timing*/
#define RTL865X_FDBENTRY_300SEC 0x1004	/*fdb entry 300s timing*/
#define RTL865X_FDBENTRY_150SEC 0x1008	/*fdb entry 150s timing*/
#endif
