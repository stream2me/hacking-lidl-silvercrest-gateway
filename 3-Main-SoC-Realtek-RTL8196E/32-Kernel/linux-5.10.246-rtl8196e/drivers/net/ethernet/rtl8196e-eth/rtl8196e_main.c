// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
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
	u32 l2_check_ok;
	u32 l2_check_fail;
	int l2_check_last;
};

static int rtl8196e_port_from_mask(u16 mask)
{
	int port;

	for (port = 0; port < 6; port++) {
		if (mask & (1 << port))
			return port;
	}

	return -EINVAL;
}

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

static int rtl8196e_open(struct net_device *ndev)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	int ret;
	bool link;

	napi_enable(&priv->napi);

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
	rtl8196e_hw_l2_setup(&priv->hw);
	ret = rtl8196e_hw_l2_add_cpu_entry(&priv->hw, ndev->dev_addr, 0);
	if (ret) {
		priv->l2_check_last = ret;
		priv->l2_check_fail++;
		netdev_warn(ndev, "L2 toCPU entry failed (%d), enabling trap fallback\n", ret);
		rtl8196e_hw_l2_trap_enable(&priv->hw);
	} else {
		ret = rtl8196e_hw_l2_check_cpu_entry(&priv->hw, ndev->dev_addr, 0);
		if (ret) {
			priv->l2_check_last = ret;
			priv->l2_check_fail++;
			netdev_warn(ndev, "L2 toCPU entry verify failed (%d), enabling trap fallback\n", ret);
			rtl8196e_hw_l2_trap_enable(&priv->hw);
		} else {
			priv->l2_check_last = 0;
			priv->l2_check_ok++;
			netdev_dbg(ndev, "L2 toCPU entry verified\n");
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

static netdev_tx_t rtl8196e_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);
	struct netdev_queue *txq;
	bool was_empty = false;
	int ret;
	int free_count;

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
	if (ret < 0) {
		unsigned int pkts = 0, bytes = 0;

		rtl8196e_ring_tx_reclaim(priv->ring, &pkts, &bytes);
		ret = rtl8196e_ring_tx_submit(priv->ring, skb, skb->data, skb->len,
					     priv->vlan_id, priv->portmask,
					     PKTHDR_USED | PKT_OUTGOING,
					     &was_empty);
		if (ret < 0) {
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

static void rtl8196e_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	netdev_warn(ndev, "TX timeout\n");
}

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
};

static int rtl8196e_get_sset_count(struct net_device *ndev, int sset)
{
	(void)ndev;
	if (sset == ETH_SS_STATS)
		return 3;
	return -EOPNOTSUPP;
}

static void rtl8196e_get_strings(struct net_device *ndev, u32 sset, u8 *data)
{
	static const char stats[][ETH_GSTRING_LEN] = {
		"rtl8196e_l2_check_ok",
		"rtl8196e_l2_check_fail",
		"rtl8196e_l2_check_last_result",
	};

	(void)ndev;
	if (sset != ETH_SS_STATS)
		return;

	memcpy(data, stats, sizeof(stats));
}

static void rtl8196e_get_ethtool_stats(struct net_device *ndev,
				       struct ethtool_stats *stats, u64 *data)
{
	struct rtl8196e_priv *priv = netdev_priv(ndev);

	(void)stats;
	data[0] = priv->l2_check_ok;
	data[1] = priv->l2_check_fail;
	data[2] = priv->l2_check_last;
}

static const struct ethtool_ops rtl8196e_ethtool_ops = {
	.get_sset_count = rtl8196e_get_sset_count,
	.get_strings = rtl8196e_get_strings,
	.get_ethtool_stats = rtl8196e_get_ethtool_stats,
};

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
	ndev->ethtool_ops = &rtl8196e_ethtool_ops;
	ndev->watchdog_timeo = 10 * HZ;
	ndev->min_mtu = 68;
	ndev->max_mtu = priv->iface.mtu;
	ndev->mtu = priv->iface.mtu;

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
