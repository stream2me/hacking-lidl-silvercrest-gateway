// SPDX-License-Identifier: GPL-2.0
/*
 * RTL8196E minimal Ethernet driver - core net_device glue.
 *
 * This file wires the platform device to netdev, NAPI, IRQs, and TX/RX
 * scheduling. Hardware register programming is isolated in rtl8196e_hw.c.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <asm/cacheflush.h>
#include "rtl8196e_dt.h"
#include "rtl8196e_hw.h"
#include "rtl8196e_ring.h"
#include "rtl8196e_pool.h"
#include "rtl8196e_regs.h"

#define RTL8196E_DRV_NAME "rtl8196e-eth"

#define RTL8196E_TX_DESC      600
#define RTL8196E_RX_DESC      500
#define RTL8196E_RX_MBUF_DESC 500
#define RTL8196E_RX_POOL      1100
#define RTL8196E_POOL_BUF_SIZE 2048
#define RTL8196E_CLUSTER_SIZE 1700

#define RTL8196E_TX_STOP_THRESH 32
#define RTL8196E_TX_WAKE_THRESH 128
#define RTL8196E_TX_TIMER_MS    2

static unsigned int link_poll_ms;
module_param(link_poll_ms, uint, 0644);
MODULE_PARM_DESC(link_poll_ms, "Link poll interval in ms (0=disabled)");

static unsigned int rtl8196e_debug;
module_param(rtl8196e_debug, uint, 0644);
MODULE_PARM_DESC(rtl8196e_debug, "Enable extra debug logging (default=0)");

static unsigned int rtl8196e_force_trap;
module_param(rtl8196e_force_trap, uint, 0644);
MODULE_PARM_DESC(rtl8196e_force_trap, "Force all unknown traffic to CPU (debug)");

static unsigned int rtl8196e_cpu_port_mask = RTL8196E_CPU_PORT_MASK;
module_param(rtl8196e_cpu_port_mask, uint, 0644);
MODULE_PARM_DESC(rtl8196e_cpu_port_mask, "CPU port mask for VLAN/L2 (default=0x100)");

struct rtl8196e_priv {
	struct net_device *ndev;
	struct napi_struct napi;
	struct rtl8196e_hw hw;
	struct rtl8196e_ring *ring;
	struct rtl8196e_pool *pool;
	struct rtl8196e_dt_iface iface;
	struct timer_list tx_timer;
	struct timer_list link_timer;
	atomic_t tx_pending;
	u16 vlan_id;
	u16 portmask;
	int phy_port;
	int phy_id;
	unsigned int link_poll_ms;
	u32 tx_debug_once;
};

/**
 * rtl8196e_port_from_mask() - Return the first port index from a mask.
 * @mask: Port bitmask.
 *
 * The driver uses a single physical port. This helper extracts the first
 * set bit so the PHY/link logic can be configured deterministically.
 *
 * Return: Port index (0..5) or -EINVAL if no bit is set.
 */
static int rtl8196e_port_from_mask(u16 mask)
{
	int port;

	for (port = 0; port < 6; port++) {
		if (mask & (1 << port))
			return port;
	}

	return -EINVAL;
}

/**
 * rtl8196e_tx_timer_fn() - Periodic TX reclaim for TX-only traffic.
 * @t: Timer handle.
 *
 * Frees completed TX descriptors and wakes the queue if space is available.
 */
static void rtl8196e_tx_timer_fn(struct timer_list *t)
{
	struct rtl8196e_priv *priv = from_timer(priv, t, tx_timer);
	struct netdev_queue *txq;
	unsigned int pkts = 0, bytes = 0;
	int free_count;

	if (!priv->ring)
		return;

	rtl8196e_ring_tx_reclaim(priv->ring, &pkts, &bytes);
	if (pkts) {
		txq = netdev_get_tx_queue(priv->ndev, 0);
		netdev_tx_completed_queue(txq, pkts, bytes);
	}

	free_count = rtl8196e_ring_tx_free_count(priv->ring);
	if (free_count >= RTL8196E_TX_WAKE_THRESH && netif_queue_stopped(priv->ndev))
		netif_wake_queue(priv->ndev);

	if (atomic_read(&priv->tx_pending) && free_count < RTL8196E_TX_WAKE_THRESH) {
		mod_timer(&priv->tx_timer, jiffies + msecs_to_jiffies(RTL8196E_TX_TIMER_MS));
	} else {
		atomic_set(&priv->tx_pending, 0);
	}
}

/**
 * rtl8196e_link_timer_fn() - Link poll timer.
 * @t: Timer handle.
 *
 * Polls PHY link status when link polling is enabled.
 */
static void rtl8196e_link_timer_fn(struct timer_list *t)
{
	struct rtl8196e_priv *priv = from_timer(priv, t, link_timer);
	bool link;

	if (!netif_running(priv->ndev))
		return;

	link = rtl8196e_hw_link_up(&priv->hw, priv->phy_port);
	if (link)
		netif_carrier_on(priv->ndev);
	else
		netif_carrier_off(priv->ndev);

	if (priv->link_poll_ms)
		mod_timer(&priv->link_timer, jiffies + msecs_to_jiffies(priv->link_poll_ms));
}

/**
 * rtl8196e_open() - net_device open hook.
 * @ndev: net_device instance.
 *
 * Programs hardware tables, initializes rings, and enables IRQs.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int rtl8196e_open(struct net_device *ndev)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	int ret;
	bool link;

	napi_enable(&priv->napi);

	/* Hardware must be initialized before ring programming. */
	rtl8196e_hw_init(&priv->hw);
	rtl8196e_hw_set_rx_rings(&priv->hw,
				   rtl8196e_ring_rx_pkthdr_base(priv->ring),
				   rtl8196e_ring_rx_mbuf_base(priv->ring));
	rtl8196e_hw_set_tx_ring(&priv->hw, rtl8196e_ring_tx_desc_base(priv->ring));
	ret = rtl8196e_hw_init_phy(&priv->hw, priv->phy_port, priv->phy_id);
	if (ret) {
		napi_disable(&priv->napi);
		return ret;
	}
	ret = rtl8196e_hw_vlan_setup(&priv->hw, priv->vlan_id, 0,
				     priv->portmask | rtl8196e_cpu_port_mask,
				     priv->iface.untag_ports);
	if (ret)
		netdev_warn(ndev, "VLAN setup failed (%d)\n", ret);
	ret = rtl8196e_hw_netif_setup(&priv->hw, ndev->dev_addr,
				      priv->vlan_id, ndev->mtu,
				      priv->portmask | rtl8196e_cpu_port_mask);
	if (ret)
		netdev_warn(ndev, "NETIF setup failed (%d)\n", ret);
	rtl8196e_hw_l2_setup(&priv->hw);
	if (rtl8196e_force_trap) {
		netdev_warn(ndev, "L2 trap-all debug enabled\n");
		rtl8196e_hw_l2_trap_enable(&priv->hw);
	}
	ret = rtl8196e_hw_l2_add_cpu_entry(&priv->hw, ndev->dev_addr, 0, 0);
	if (ret) {
		netdev_warn(ndev, "L2 toCPU entry failed (%d), enabling trap fallback\n", ret);
		rtl8196e_hw_l2_trap_enable(&priv->hw);
	} else {
		ret = rtl8196e_hw_l2_add_bcast_entry(&priv->hw, 0,
						     priv->portmask | rtl8196e_cpu_port_mask);
		if (ret)
			netdev_warn(ndev, "L2 broadcast entry failed (%d)\n", ret);
		if (rtl8196e_debug) {
			ret = rtl8196e_hw_l2_check_cpu_entry(&priv->hw, ndev->dev_addr, 0);
			if (ret) {
				netdev_warn(ndev, "L2 toCPU entry verify failed (%d), enabling trap fallback\n", ret);
				rtl8196e_hw_l2_trap_enable(&priv->hw);
			} else {
				netdev_dbg(ndev, "L2 toCPU entry verified\n");
			}
		}
	}
	rtl8196e_hw_start(&priv->hw);
	rtl8196e_hw_enable_irqs(&priv->hw);

	netif_start_queue(ndev);
	link = rtl8196e_hw_link_up(&priv->hw, priv->phy_port);
	if (link)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);
	if (priv->link_poll_ms)
		mod_timer(&priv->link_timer, jiffies + msecs_to_jiffies(priv->link_poll_ms));

	return 0;
}

/**
 * rtl8196e_stop() - net_device stop hook.
 * @ndev: net_device instance.
 *
 * Disables IRQs, stops HW, and stops timers.
 *
 * Return: 0 on success.
 */
static int rtl8196e_stop(struct net_device *ndev)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	rtl8196e_hw_disable_irqs(&priv->hw);
	rtl8196e_hw_stop(&priv->hw);
	napi_disable(&priv->napi);
	del_timer_sync(&priv->tx_timer);
	del_timer_sync(&priv->link_timer);
	netif_carrier_off(ndev);

	return 0;
}

/**
 * rtl8196e_set_mac_address() - Runtime MAC update.
 * @ndev: net_device instance.
 * @addr: New MAC address (sockaddr).
 *
 * Updates NETIF/L2 tables when the interface is running.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int rtl8196e_set_mac_address(struct net_device *ndev, void *addr)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	struct sockaddr *sa = addr;
	int ret;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(ndev->dev_addr, sa->sa_data);

	if (!netif_running(ndev))
		return 0;

	netif_tx_disable(ndev);
	rtl8196e_hw_disable_irqs(&priv->hw);

	ret = rtl8196e_hw_netif_setup(&priv->hw, ndev->dev_addr,
				      priv->vlan_id, ndev->mtu,
				      priv->portmask | rtl8196e_cpu_port_mask);
	if (ret)
		netdev_warn(ndev, "NETIF setup failed (%d)\n", ret);

	ret = rtl8196e_hw_l2_add_cpu_entry(&priv->hw, ndev->dev_addr, 0, 0);
	if (ret) {
		netdev_warn(ndev, "L2 toCPU entry failed (%d), enabling trap fallback\n", ret);
		rtl8196e_hw_l2_trap_enable(&priv->hw);
	} else if (rtl8196e_debug) {
		ret = rtl8196e_hw_l2_check_cpu_entry(&priv->hw, ndev->dev_addr, 0);
		if (ret) {
			netdev_warn(ndev, "L2 toCPU entry verify failed (%d), enabling trap fallback\n", ret);
			rtl8196e_hw_l2_trap_enable(&priv->hw);
		}
	}

	rtl8196e_hw_enable_irqs(&priv->hw);
	netif_wake_queue(ndev);

	return 0;
}

/**
 * rtl8196e_start_xmit() - Transmit a packet.
 * @skb: Packet buffer to transmit.
 * @ndev: net_device instance.
 *
 * Linearizes non-linear SKBs, fills a TX descriptor, and kicks the switch core.
 *
 * Return: NETDEV_TX_OK or NETDEV_TX_BUSY.
 */
static netdev_tx_t rtl8196e_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	struct netdev_queue *txq;
	bool was_empty = false;
	int ret;
	int free_count;
	u32 debug_val;

	if (!priv->ring || !priv->portmask) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (unlikely(skb_is_nonlinear(skb))) {
		if (skb_linearize(skb)) {
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
	}

	ret = rtl8196e_ring_tx_submit(priv->ring, skb, skb->data, skb->len,
					     priv->vlan_id, priv->portmask,
					     PKTHDR_USED | PKT_OUTGOING,
					     &was_empty);
	debug_val = READ_ONCE(priv->tx_debug_once);
	if (debug_val == 0) {
		WRITE_ONCE(priv->tx_debug_once, 1);
		netdev_info(ndev, "xmit first packet len=%u portmask=0x%x vid=%u\n",
			    skb->len, priv->portmask, priv->vlan_id);
	}
	if (ret < 0) {
		unsigned int pkts = 0, bytes = 0;

		netdev_warn(ndev, "xmit submit failed (%d), reclaiming\n", ret);
		rtl8196e_ring_tx_reclaim(priv->ring, &pkts, &bytes);
		ret = rtl8196e_ring_tx_submit(priv->ring, skb, skb->data, skb->len,
					     priv->vlan_id, priv->portmask,
					     PKTHDR_USED | PKT_OUTGOING,
					     &was_empty);
		if (ret < 0) {
			netdev_warn(ndev, "xmit submit still failed (%d)\n", ret);
			atomic_set(&priv->tx_pending, 1);
			mod_timer(&priv->tx_timer, jiffies + msecs_to_jiffies(RTL8196E_TX_TIMER_MS));
			netif_stop_queue(ndev);
			return NETDEV_TX_BUSY;
		}
	}

	rtl8196e_ring_kick_tx(was_empty);

	txq = netdev_get_tx_queue(ndev, 0);
	netdev_tx_sent_queue(txq, skb->len);
	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += skb->len;

	free_count = rtl8196e_ring_tx_free_count(priv->ring);
	if (free_count < RTL8196E_TX_STOP_THRESH) {
		netif_stop_queue(ndev);
		atomic_set(&priv->tx_pending, 1);
		mod_timer(&priv->tx_timer, jiffies + msecs_to_jiffies(RTL8196E_TX_TIMER_MS));
	}
	return NETDEV_TX_OK;
}

/**
 * rtl8196e_tx_timeout() - TX watchdog recovery.
 * @ndev: net_device instance.
 * @txqueue: TX queue index.
 *
 * Resets the TX ring and restarts the hardware TX engine.
 */
static void rtl8196e_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	unsigned int pkts = 0, bytes = 0;

	netdev_warn(ndev, "TX timeout\n");

	if (!priv->ring)
		return;

	netif_stop_queue(ndev);
	rtl8196e_hw_disable_irqs(&priv->hw);
	rtl8196e_hw_stop(&priv->hw);

	rtl8196e_ring_tx_reclaim(priv->ring, &pkts, &bytes);
	rtl8196e_ring_tx_reset(priv->ring);
	rtl8196e_hw_set_tx_ring(&priv->hw, rtl8196e_ring_tx_desc_base(priv->ring));

	rtl8196e_hw_start(&priv->hw);
	rtl8196e_hw_enable_irqs(&priv->hw);

	atomic_set(&priv->tx_pending, 0);
	netif_wake_queue(ndev);
}

/**
 * rtl8196e_poll() - NAPI poll handler.
 * @napi: NAPI instance.
 * @budget: Work budget.
 *
 * Handles RX packets and reclaims TX descriptors.
 *
 * Return: Number of packets processed.
 */
static int rtl8196e_poll(struct napi_struct *napi, int budget)
{
	struct rtl8196e_priv *priv = container_of(napi, struct rtl8196e_priv, napi);
	struct netdev_queue *txq;
	unsigned int pkts = 0, bytes = 0;
	int work_done;

	work_done = rtl8196e_ring_rx_poll(priv->ring, budget, napi, priv->ndev);

	rtl8196e_ring_tx_reclaim(priv->ring, &pkts, &bytes);
	if (pkts) {
		txq = netdev_get_tx_queue(priv->ndev, 0);
		netdev_tx_completed_queue(txq, pkts, bytes);
	}

	if (work_done < budget) {
		if (napi_complete_done(napi, work_done)) {
			*(volatile u32 *)CPUIISR = PKTHDR_DESC_RUNOUT_IP_ALL | MBUF_DESC_RUNOUT_IP_ALL;
			rtl8196e_hw_enable_irqs(&priv->hw);
		}
	}

	return work_done;
}

/**
 * rtl8196e_isr() - Interrupt handler.
 * @irq: IRQ number.
 * @dev_id: net_device pointer.
 *
 * Acks interrupts, updates link, and schedules NAPI if needed.
 *
 * Return: IRQ_HANDLED.
 */
static irqreturn_t rtl8196e_isr(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	u32 status;
	bool link;

	status = *(volatile u32 *)CPUIISR;
	*(volatile u32 *)CPUIISR = status;
	status &= *(volatile u32 *)CPUIIMR;

	if (status & LINK_CHANGE_IP) {
		link = rtl8196e_hw_link_up(&priv->hw, priv->phy_port);
		if (link)
			netif_carrier_on(ndev);
		else
			netif_carrier_off(ndev);
	}

	if (status & (RX_DONE_IP_ALL | TX_ALL_DONE_IP_ALL | PKTHDR_DESC_RUNOUT_IP_ALL)) {
		if (napi_schedule_prep(&priv->napi)) {
			rtl8196e_hw_disable_irqs(&priv->hw);
			__napi_schedule(&priv->napi);
		}
	}

	return IRQ_HANDLED;
}

static const struct net_device_ops rtl8196e_netdev_ops = {
	.ndo_open = rtl8196e_open,
	.ndo_stop = rtl8196e_stop,
	.ndo_start_xmit = rtl8196e_start_xmit,
	.ndo_tx_timeout = rtl8196e_tx_timeout,
	.ndo_set_mac_address = rtl8196e_set_mac_address,
};

/**
 * rtl8196e_probe() - Platform probe.
 * @pdev: Platform device.
 *
 * Allocates netdev, parses DT, initializes rings/pool, and registers the device.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int rtl8196e_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct rtl8196e_priv *priv;
	int ret;
	int irq;

	ndev = alloc_etherdev(sizeof(*priv));
	if (!ndev)
		return -ENOMEM;

	platform_set_drvdata(pdev, ndev);
	priv = netdev_priv(ndev);
	priv->ndev = ndev;

	ret = rtl8196e_dt_parse(&pdev->dev, &priv->iface);
	if (ret)
		goto err_free;

	if (priv->iface.mac_set)
		ether_addr_copy(ndev->dev_addr, priv->iface.mac);
	else
		eth_hw_addr_random(ndev);

	strscpy(ndev->name, priv->iface.ifname, IFNAMSIZ);
	priv->vlan_id = priv->iface.vlan_id;
	priv->portmask = priv->iface.member_ports;
	priv->phy_port = rtl8196e_port_from_mask(priv->portmask);
	priv->phy_id = priv->iface.phy_id_set ? priv->iface.phy_id : priv->phy_port;
	priv->link_poll_ms = priv->iface.link_poll_ms_set ? priv->iface.link_poll_ms : link_poll_ms;
	if (priv->phy_port < 0) {
		ret = -EINVAL;
		goto err_free;
	}

	priv->pool = rtl8196e_pool_create(RTL8196E_POOL_BUF_SIZE, RTL8196E_RX_POOL);
	if (!priv->pool) {
		ret = -ENOMEM;
		goto err_free;
	}

	priv->ring = rtl8196e_ring_create(priv->pool,
					 RTL8196E_TX_DESC,
					 RTL8196E_RX_DESC,
					 RTL8196E_RX_MBUF_DESC,
					 RTL8196E_CLUSTER_SIZE);
	if (!priv->ring) {
		ret = -ENOMEM;
		goto err_pool;
	}

	timer_setup(&priv->tx_timer, rtl8196e_tx_timer_fn, 0);
	timer_setup(&priv->link_timer, rtl8196e_link_timer_fn, 0);
	atomic_set(&priv->tx_pending, 0);

	netif_napi_add(ndev, &priv->napi, rtl8196e_poll, 64);
	ndev->netdev_ops = &rtl8196e_netdev_ops;
	ndev->watchdog_timeo = 10 * HZ;
	ndev->min_mtu = 68;
	ndev->max_mtu = priv->iface.mtu;
	ndev->mtu = priv->iface.mtu;
	ndev->priv_flags |= IFF_LIVE_ADDR_CHANGE;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err_ring;
	}

	ret = request_irq(irq, rtl8196e_isr, 0, RTL8196E_DRV_NAME, ndev);
	if (ret)
		goto err_ring;

	ret = register_netdev(ndev);
	if (ret)
		goto err_irq;

	dev_info(&pdev->dev, "rtl8196e-eth registered (experimental)\n");
	return 0;

err_irq:
	free_irq(irq, ndev);
err_ring:
	rtl8196e_ring_destroy(priv->ring);
err_pool:
	rtl8196e_pool_destroy(priv->pool);
err_free:
	free_netdev(ndev);
	return ret;
}

/**
 * rtl8196e_remove() - Platform remove.
 * @pdev: Platform device.
 *
 * Unregisters netdev and frees resources.
 *
 * Return: 0 on success.
 */
static int rtl8196e_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct rtl8196e_priv *priv;
	int irq;

	if (!ndev)
		return 0;

	priv = netdev_priv(ndev);

	unregister_netdev(ndev);

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0)
		free_irq(irq, ndev);

	if (priv->ring)
		rtl8196e_ring_destroy(priv->ring);
	if (priv->pool)
		rtl8196e_pool_destroy(priv->pool);

	free_netdev(ndev);
	return 0;
}

static const struct of_device_id rtl8196e_of_match[] = {
	{ .compatible = "realtek,rtl8196e-mac" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtl8196e_of_match);

static struct platform_driver rtl8196e_driver = {
	.probe = rtl8196e_probe,
	.remove = rtl8196e_remove,
	.driver = {
		.name = RTL8196E_DRV_NAME,
		.of_match_table = rtl8196e_of_match,
	},
};

module_platform_driver(rtl8196e_driver);

MODULE_AUTHOR("Jacques Nilo");
MODULE_DESCRIPTION("RTL8196E minimal Ethernet driver");
MODULE_LICENSE("GPL");
