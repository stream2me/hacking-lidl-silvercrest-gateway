/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RTL8196E device-tree parsing interface.
 */
#ifndef RTL8196E_DT_H
#define RTL8196E_DT_H

#include <linux/types.h>

struct device;

/**
 * struct rtl8196e_dt_iface - Parsed interface configuration.
 * @ifname: Interface name (eth0).
 * @mac: MAC address.
 * @mac_set: True when MAC is provided by DT.
 * @vlan_id: VLAN ID.
 * @member_ports: VLAN member port mask.
 * @untag_ports: VLAN untag port mask.
 * @mtu: Interface MTU.
 * @phy_id: PHY address.
 * @phy_id_set: True when PHY ID is provided by DT.
 * @link_poll_ms: Link polling period in milliseconds.
 * @link_poll_ms_set: True when link polling is provided by DT.
 */
struct rtl8196e_dt_iface {
	char ifname[16];
	u8 mac[6];
	bool mac_set;
	u32 vlan_id;
	u32 member_ports;
	u32 untag_ports;
	u32 mtu;
	u32 phy_id;
	bool phy_id_set;
	u32 link_poll_ms;
	bool link_poll_ms_set;
};

/**
 * rtl8196e_dt_parse() - Parse device-tree properties for the driver.
 * @dev: Device pointer.
 * @iface: Output interface configuration.
 *
 * Return: 0 on success, negative errno otherwise.
 */
int rtl8196e_dt_parse(struct device *dev, struct rtl8196e_dt_iface *iface);

#endif /* RTL8196E_DT_H */
