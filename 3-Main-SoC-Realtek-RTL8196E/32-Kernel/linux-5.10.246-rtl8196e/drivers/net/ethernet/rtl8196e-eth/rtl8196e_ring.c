// SPDX-License-Identifier: GPL-2.0
/*
 * RTL8196E minimal Ethernet driver - descriptor rings.
 *
 * Owns TX/RX ring allocation, descriptor setup, and TX/RX path operations.
 * Cache management and ownership transitions are critical here.
 */
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include "rtl8196e_ring.h"
#include "rtl8196e_pool.h"
#include "rtl8196e_regs.h"

struct rtl8196e_ring {
	u32 *tx_ring;
	u32 *rx_pkthdr_ring;
	u32 *rx_mbuf_ring;
	void *tx_ring_alloc;
	void *rx_pkthdr_ring_alloc;
	void *rx_mbuf_ring_alloc;
	struct rtl_pktHdr *pkthdr_pool;
	struct rtl_mBuf *mbuf_pool;
	void *pkthdr_alloc;
	void *mbuf_alloc;
	struct rtl_mBuf *rx_mbuf_base;
	unsigned int tx_cnt;
	unsigned int rx_cnt;
	unsigned int rx_mbuf_cnt;
	unsigned int tx_prod;
	unsigned int tx_cons;
	unsigned int rx_idx;
	unsigned int last_tx_submit;
	unsigned int rx_debug_once;
	unsigned int rx_debug_bad;
	size_t buf_size;
	struct rtl8196e_pool *pool;
	spinlock_t tx_lock;
};

/**
 * rtl8196e_alloc_uncached() - Allocate memory and return KSEG1 alias.
 * @size: Allocation size in bytes.
 * @orig_out: Optional pointer to receive original cached pointer.
 *
 * The hardware expects uncached KSEG1 addresses, so we allocate normally and
 * return the KSEG1 alias. The original pointer is kept for freeing.
 *
 * Return: Uncached pointer or NULL on failure.
 */
static void *rtl8196e_alloc_uncached(size_t size, void **orig_out)
{
	void *p = kmalloc(size, GFP_ATOMIC);
	if (!p)
		return NULL;
	if (orig_out)
		*orig_out = p;
	return rtl8196e_uncached_addr(p);
}

/**
 * rtl8196e_desc_ptr() - Extract descriptor pointer from ring entry.
 * @entry: Ring entry with OWN/WRAP bits.
 *
 * Return: Descriptor pointer without ownership bits.
 */
static struct rtl_pktHdr *rtl8196e_desc_ptr(u32 entry)
{
	return (struct rtl_pktHdr *)(entry & ~(RTL8196E_DESC_OWNED_BIT | RTL8196E_DESC_WRAP));
}

/**
 * rtl8196e_ring_create() - Allocate and initialize TX/RX rings.
 * @pool: Private buffer pool.
 * @tx_cnt: TX descriptor count.
 * @rx_cnt: RX descriptor count.
 * @rx_mbuf_cnt: RX mbuf descriptor count.
 * @buf_size: RX buffer size.
 *
 * Return: Ring handle or NULL on failure.
 */
struct rtl8196e_ring *rtl8196e_ring_create(struct rtl8196e_pool *pool,
					   unsigned int tx_cnt,
					   unsigned int rx_cnt,
					   unsigned int rx_mbuf_cnt,
					   size_t buf_size)
{
	struct rtl8196e_ring *ring;
	unsigned int i;
	unsigned int pkthdr_cnt;
	unsigned int mbuf_cnt;
	size_t alloc_size;

	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	if (!ring)
		return NULL;

	if (rx_mbuf_cnt < rx_cnt)
		goto err;

	ring->tx_cnt = tx_cnt;
	ring->rx_cnt = rx_cnt;
	ring->rx_mbuf_cnt = rx_mbuf_cnt;
	ring->buf_size = buf_size;
	ring->pool = pool;
	spin_lock_init(&ring->tx_lock);

	ring->tx_ring = rtl8196e_alloc_uncached(tx_cnt * sizeof(u32), &ring->tx_ring_alloc);
	ring->rx_pkthdr_ring = rtl8196e_alloc_uncached(rx_cnt * sizeof(u32), &ring->rx_pkthdr_ring_alloc);
	ring->rx_mbuf_ring = rtl8196e_alloc_uncached(rx_mbuf_cnt * sizeof(u32), &ring->rx_mbuf_ring_alloc);

	if (!ring->tx_ring || !ring->rx_pkthdr_ring || !ring->rx_mbuf_ring)
		goto err;

	pkthdr_cnt = tx_cnt + rx_cnt;
	mbuf_cnt = tx_cnt + rx_mbuf_cnt;

	alloc_size = pkthdr_cnt * sizeof(struct rtl_pktHdr) + L1_CACHE_BYTES;
	ring->pkthdr_alloc = kmalloc(alloc_size, GFP_ATOMIC);
	if (!ring->pkthdr_alloc)
		goto err;
	ring->pkthdr_pool = (struct rtl_pktHdr *)ALIGN((unsigned long)ring->pkthdr_alloc, L1_CACHE_BYTES);

	alloc_size = mbuf_cnt * sizeof(struct rtl_mBuf) + L1_CACHE_BYTES;
	ring->mbuf_alloc = kmalloc(alloc_size, GFP_ATOMIC);
	if (!ring->mbuf_alloc)
		goto err;
	ring->mbuf_pool = (struct rtl_mBuf *)ALIGN((unsigned long)ring->mbuf_alloc, L1_CACHE_BYTES);
	ring->rx_mbuf_base = ring->mbuf_pool + tx_cnt;

	/* Init TX descriptors. */
	for (i = 0; i < tx_cnt; i++) {
		struct rtl_pktHdr *ph = &ring->pkthdr_pool[i];
		struct rtl_mBuf *mb = &ring->mbuf_pool[i];

		memset(ph, 0, sizeof(*ph));
		memset(mb, 0, sizeof(*mb));

		ph->ph_mbuf = mb;
		ph->ph_flags = PKTHDR_USED | PKT_OUTGOING;
		ph->ph_type = PKTHDR_ETHERNET;
		ph->ph_portlist = 0;

		mb->m_pkthdr = ph;
		mb->m_flags = MBUF_USED | MBUF_EXT | MBUF_PKTHDR | MBUF_EOR;
		mb->m_data = NULL;
		mb->m_extbuf = NULL;
		mb->m_extsize = 0;
		mb->skb = NULL;

		/* Descriptor ring stores KSEG1 pointers, OWN bit set for CPU. */
		ring->tx_ring[i] = (u32)ph | RTL8196E_DESC_RISC_OWNED;

		dma_cache_wback_inv((unsigned long)ph, sizeof(*ph));
		dma_cache_wback_inv((unsigned long)mb, sizeof(*mb));
	}
	if (tx_cnt)
		ring->tx_ring[tx_cnt - 1] |= RTL8196E_DESC_WRAP;

	/* Init RX descriptors. */
	for (i = 0; i < rx_cnt; i++) {
		struct rtl_pktHdr *ph = &ring->pkthdr_pool[tx_cnt + i];
		struct rtl_mBuf *mb = &ring->mbuf_pool[tx_cnt + i];
		struct sk_buff *skb;

		memset(ph, 0, sizeof(*ph));
		memset(mb, 0, sizeof(*mb));

		ph->ph_mbuf = mb;
		ph->ph_flags = PKTHDR_USED | PKT_INCOMING;
		ph->ph_type = PKTHDR_ETHERNET;
		ph->ph_portlist = 0;

		mb->m_pkthdr = ph;
		mb->m_flags = MBUF_USED | MBUF_EXT | MBUF_PKTHDR | MBUF_EOR;
		mb->m_len = 0;
		mb->m_extsize = buf_size;

		skb = rtl8196e_pool_alloc_skb(pool, buf_size);
		if (!skb)
			goto err;

		mb->m_data = skb->data;
		mb->m_extbuf = skb->data;
		mb->skb = skb;

		ring->rx_pkthdr_ring[i] = (u32)ph | RTL8196E_DESC_SWCORE_OWNED;
		ring->rx_mbuf_ring[i] = (u32)mb | RTL8196E_DESC_SWCORE_OWNED;

		dma_cache_wback_inv((unsigned long)mb->m_extbuf, buf_size);
		dma_cache_wback_inv((unsigned long)ph, sizeof(*ph));
		dma_cache_wback_inv((unsigned long)mb, sizeof(*mb));
	}
	if (rx_cnt)
		ring->rx_pkthdr_ring[rx_cnt - 1] |= RTL8196E_DESC_WRAP;
	if (rx_mbuf_cnt)
		ring->rx_mbuf_ring[rx_mbuf_cnt - 1] |= RTL8196E_DESC_WRAP;

	/* Flush descriptor structures */
	dma_cache_wback_inv((unsigned long)ring->pkthdr_pool, pkthdr_cnt * sizeof(struct rtl_pktHdr));
	dma_cache_wback_inv((unsigned long)ring->mbuf_pool, mbuf_cnt * sizeof(struct rtl_mBuf));

	return ring;

err:
	rtl8196e_ring_destroy(ring);
	return NULL;
}

/**
 * rtl8196e_ring_destroy() - Tear down rings and free resources.
 * @ring: Ring handle to destroy.
 *
 * Frees any outstanding SKBs and releases all ring allocations.
 */
void rtl8196e_ring_destroy(struct rtl8196e_ring *ring)
{
	unsigned int i;

	if (!ring)
		return;

	if (ring->mbuf_pool) {
		for (i = 0; i < ring->tx_cnt + ring->rx_mbuf_cnt; i++) {
			if (ring->mbuf_pool[i].skb) {
				dev_kfree_skb_any((struct sk_buff *)ring->mbuf_pool[i].skb);
				ring->mbuf_pool[i].skb = NULL;
			}
		}
	}

	kfree(ring->tx_ring_alloc);
	kfree(ring->rx_pkthdr_ring_alloc);
	kfree(ring->rx_mbuf_ring_alloc);
	kfree(ring->pkthdr_alloc);
	kfree(ring->mbuf_alloc);
	kfree(ring);
}

/**
 * rtl8196e_ring_tx_desc_base() - Return TX ring base address.
 * @ring: Ring handle.
 *
 * Return: Uncached TX ring base or NULL.
 */
void *rtl8196e_ring_tx_desc_base(struct rtl8196e_ring *ring)
{
	return ring ? ring->tx_ring : NULL;
}

/**
 * rtl8196e_ring_rx_pkthdr_base() - Return RX pkthdr ring base address.
 * @ring: Ring handle.
 *
 * Return: Uncached RX pkthdr ring base or NULL.
 */
void *rtl8196e_ring_rx_pkthdr_base(struct rtl8196e_ring *ring)
{
	return ring ? ring->rx_pkthdr_ring : NULL;
}

/**
 * rtl8196e_ring_rx_mbuf_base() - Return RX mbuf ring base address.
 * @ring: Ring handle.
 *
 * Return: Uncached RX mbuf ring base or NULL.
 */
void *rtl8196e_ring_rx_mbuf_base(struct rtl8196e_ring *ring)
{
	return ring ? ring->rx_mbuf_ring : NULL;
}

/**
 * rtl8196e_ring_tx_submit() - Queue a TX packet to the hardware ring.
 * @ring: Ring handle.
 * @skb: SKB being transmitted (owned until reclaim).
 * @data: Data pointer for DMA (must be in KSEG1).
 * @len: Packet length.
 * @vid: VLAN ID.
 * @portlist: Destination port mask.
 * @flags: Packet header flags.
 * @was_empty: Optional pointer set if ring was empty before submit.
 *
 * Return: 0 on success, negative errno on failure.
 */
int rtl8196e_ring_tx_submit(struct rtl8196e_ring *ring, void *skb,
			    void *data, unsigned int len,
			    u16 vid, u16 portlist, u16 flags,
			    bool *was_empty)
{
	unsigned long irq_flags;
	unsigned int next;
	struct rtl_pktHdr *ph;
	struct rtl_mBuf *mb;

	if (!ring || !skb || !data || len == 0)
		return -EINVAL;

	if (len < ETH_ZLEN)
		len = ETH_ZLEN;
	if (len > 1518)
		return -EINVAL;

	spin_lock_irqsave(&ring->tx_lock, irq_flags);

	next = ring->tx_prod + 1;
	if (next >= ring->tx_cnt)
		next = 0;

	if (next == ring->tx_cons) {
		spin_unlock_irqrestore(&ring->tx_lock, irq_flags);
		return -ENOSPC;
	}

	if (was_empty)
		*was_empty = (ring->tx_prod == ring->tx_cons);

	ph = rtl8196e_desc_ptr(ring->tx_ring[ring->tx_prod]);
	mb = ph->ph_mbuf;
	ring->last_tx_submit = ring->tx_prod;

	mb->m_len = len;
	mb->m_extsize = len;
	mb->m_data = data;
	mb->m_extbuf = data;
	mb->skb = skb;

	ph->ph_len = len;
	ph->ph_vlanId = vid;
	ph->ph_portlist = portlist & 0x3f;
	ph->ph_srcExtPortNum = 0;
	ph->ph_flags = flags;

	/* Flush packet data and descriptors before handing to hardware. */
	dma_cache_wback_inv((unsigned long)data, len);
	dma_cache_wback_inv((unsigned long)ph, sizeof(*ph));
	dma_cache_wback_inv((unsigned long)mb, sizeof(*mb));

	/* Hand over to hardware: OWN bit + barriers. */
	wmb();
	ring->tx_ring[ring->tx_prod] |= RTL8196E_DESC_SWCORE_OWNED;
	wmb();

	ring->tx_prod = next;
	spin_unlock_irqrestore(&ring->tx_lock, irq_flags);

	return 0;
}

/**
 * rtl8196e_ring_tx_reclaim() - Reclaim completed TX descriptors.
 * @ring: Ring handle.
 * @pkts: Optional output count of packets reclaimed.
 * @bytes: Optional output count of bytes reclaimed.
 *
 * Return: Number of packets reclaimed.
 */
int rtl8196e_ring_tx_reclaim(struct rtl8196e_ring *ring,
			     unsigned int *pkts,
			     unsigned int *bytes)
{
	unsigned int done_pkts = 0;
	unsigned int done_bytes = 0;

	if (!ring)
		return 0;

	while (ring->tx_cons != ring->tx_prod) {
		u32 entry;
		struct rtl_pktHdr *ph;
		struct rtl_mBuf *mb;
		struct sk_buff *skb;

		/* Descriptor ownership must be read uncached. */
		dma_cache_inv((unsigned long)&ring->tx_ring[ring->tx_cons], sizeof(u32));
		rmb();
		entry = ring->tx_ring[ring->tx_cons];
		if (entry & RTL8196E_DESC_OWNED_BIT)
			break;

		ph = rtl8196e_desc_ptr(entry);
		dma_cache_inv((unsigned long)ph, sizeof(*ph));
		mb = ph->ph_mbuf;
		dma_cache_inv((unsigned long)mb, sizeof(*mb));

		skb = (struct sk_buff *)mb->skb;
		if (skb) {
			done_pkts++;
			done_bytes += skb->len;
			dev_kfree_skb_any(skb);
			mb->skb = NULL;
		}

		ring->tx_cons++;
		if (ring->tx_cons >= ring->tx_cnt)
			ring->tx_cons = 0;
	}

	if (pkts)
		*pkts = done_pkts;
	if (bytes)
		*bytes = done_bytes;

	return done_pkts;
}

/**
 * rtl8196e_ring_rx_poll() - Poll RX ring and push packets to the stack.
 * @ring: Ring handle.
 * @budget: NAPI budget.
 * @napi: NAPI context.
 * @dev: Net device.
 *
 * Return: Number of packets processed.
 */
int rtl8196e_ring_rx_poll(struct rtl8196e_ring *ring, int budget,
			  struct napi_struct *napi,
			  struct net_device *dev)
{
	int work_done = 0;

	if (!ring)
		return 0;

	while (work_done < budget) {
		u32 entry = ring->rx_pkthdr_ring[ring->rx_idx];
		struct rtl_pktHdr *ph;
		struct rtl_mBuf *mb;
		struct sk_buff *skb, *new_skb;
		unsigned int len;
		unsigned int mbuf_index;

		if (entry & RTL8196E_DESC_OWNED_BIT)
			break;

		/* Descriptor completed by hardware; read packet header. */
		ph = rtl8196e_desc_ptr(entry);
		dma_cache_inv((unsigned long)ph, sizeof(*ph));
		mb = ph->ph_mbuf;
		dma_cache_inv((unsigned long)mb, sizeof(*mb));

		skb = (struct sk_buff *)mb->skb;
		if (!skb)
			goto rearm;

		/* Allocate a replacement buffer before returning the old one. */
		new_skb = rtl8196e_pool_alloc_skb(ring->pool, ring->buf_size);
		if (!new_skb)
			goto rearm;

		len = ph->ph_len;
		if (len < ETH_ZLEN || len > ring->buf_size)
			goto rearm_bad;
		skb->tail = skb->data;
		skb->len = 0;
		skb_put(skb, len);
		skb->dev = dev;
		if (dev) {
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;
		}
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (dev && ring->rx_debug_once == 0) {
			ring->rx_debug_once = 1;
			netdev_info(dev, "rx first len=%u flags=0x%04x port=0x%02x vid=%u\n",
				    len, ph->ph_flags, ph->ph_portlist, ph->ph_vlanId);
		}

		/* Install new buffer and reset header for RX ownership. */
		mb->m_data = new_skb->data;
		mb->m_extbuf = new_skb->data;
		mb->m_extsize = ring->buf_size;
		mb->m_len = 0;
		mb->skb = new_skb;
		ph->ph_len = 0;
		ph->ph_flags = PKTHDR_USED | PKT_INCOMING;

		napi_gro_receive(napi, skb);
		work_done++;
		goto rearm;

rearm_bad:
		if (dev && ring->rx_debug_bad < 3) {
			ring->rx_debug_bad++;
			netdev_warn(dev, "rx bad len=%u flags=0x%04x port=0x%02x vid=%u\n",
				    len, ph->ph_flags, ph->ph_portlist, ph->ph_vlanId);
		}

rearm:
		/* Return both descriptor rings to hardware ownership. */
		mbuf_index = (unsigned int)(mb - ring->rx_mbuf_base);
		if (mbuf_index < ring->rx_mbuf_cnt)
			ring->rx_mbuf_ring[mbuf_index] |= RTL8196E_DESC_SWCORE_OWNED;

		ring->rx_pkthdr_ring[ring->rx_idx] =
			(u32)ph | (ring->rx_pkthdr_ring[ring->rx_idx] & RTL8196E_DESC_WRAP) | RTL8196E_DESC_SWCORE_OWNED;

		if (mb->m_extbuf)
			dma_cache_wback_inv((unsigned long)mb->m_extbuf, ring->buf_size);
		dma_cache_wback_inv((unsigned long)ph, sizeof(*ph));
		dma_cache_wback_inv((unsigned long)mb, sizeof(*mb));

		ring->rx_idx++;
		if (ring->rx_idx >= ring->rx_cnt)
			ring->rx_idx = 0;
	}

	return work_done;
}

/**
 * rtl8196e_ring_tx_free_count() - Return free TX descriptor slots.
 * @ring: Ring handle.
 *
 * Return: Available TX descriptors (excluding one slot for full/empty).
 */
int rtl8196e_ring_tx_free_count(struct rtl8196e_ring *ring)
{
	int used;

	if (!ring || ring->tx_cnt == 0)
		return 0;

	if (ring->tx_prod >= ring->tx_cons)
		used = ring->tx_prod - ring->tx_cons;
	else
		used = ring->tx_cnt - ring->tx_cons + ring->tx_prod;

	return (int)ring->tx_cnt - 1 - used;
}

/**
 * rtl8196e_ring_kick_tx() - Trigger TX DMA fetch.
 * @was_empty: Whether the ring was empty before submit (unused).
 */
void rtl8196e_ring_kick_tx(bool was_empty)
{
	u32 icr;

	(void)was_empty;

	icr = *(volatile u32 *)CPUICR;
	*(volatile u32 *)CPUICR = icr | TXFD;
	wmb();
	(void)*(volatile u32 *)CPUICR;
	*(volatile u32 *)CPUICR = icr;
	mb();
	(void)*(volatile u32 *)CPUICR;
}

/**
 * rtl8196e_ring_tx_reset() - Reset TX descriptors to a clean state.
 * @ring: Ring handle.
 */
void rtl8196e_ring_tx_reset(struct rtl8196e_ring *ring)
{
	unsigned int i;

	if (!ring)
		return;

	for (i = 0; i < ring->tx_cnt; i++) {
		struct rtl_pktHdr *ph = &ring->pkthdr_pool[i];
		struct rtl_mBuf *mb = &ring->mbuf_pool[i];

		if (mb->skb) {
			dev_kfree_skb_any((struct sk_buff *)mb->skb);
			mb->skb = NULL;
		}

		memset(ph, 0, sizeof(*ph));
		memset(mb, 0, sizeof(*mb));

		ph->ph_mbuf = mb;
		ph->ph_flags = PKTHDR_USED | PKT_OUTGOING;
		ph->ph_type = PKTHDR_ETHERNET;
		ph->ph_portlist = 0;

		mb->m_pkthdr = ph;
		mb->m_flags = MBUF_USED | MBUF_EXT | MBUF_PKTHDR | MBUF_EOR;
		mb->m_data = NULL;
		mb->m_extbuf = NULL;
		mb->m_extsize = 0;

		/* Return descriptor ownership to CPU. */
		ring->tx_ring[i] = (u32)ph | RTL8196E_DESC_RISC_OWNED;

		dma_cache_wback_inv((unsigned long)ph, sizeof(*ph));
		dma_cache_wback_inv((unsigned long)mb, sizeof(*mb));
	}

	if (ring->tx_cnt)
		ring->tx_ring[ring->tx_cnt - 1] |= RTL8196E_DESC_WRAP;

	ring->tx_prod = 0;
	ring->tx_cons = 0;
	ring->last_tx_submit = 0;
}

/**
 * rtl8196e_ring_last_tx_submit() - Return last TX descriptor index used.
 * @ring: Ring handle.
 *
 * Return: Last submitted TX index.
 */
unsigned int rtl8196e_ring_last_tx_submit(struct rtl8196e_ring *ring)
{
	if (!ring)
		return 0;
	return ring->last_tx_submit;
}

/**
 * rtl8196e_ring_tx_count() - Return TX ring size.
 * @ring: Ring handle.
 *
 * Return: TX ring descriptor count.
 */
unsigned int rtl8196e_ring_tx_count(struct rtl8196e_ring *ring)
{
	if (!ring)
		return 0;
	return ring->tx_cnt;
}

/**
 * rtl8196e_ring_tx_entry() - Return TX ring entry value.
 * @ring: Ring handle.
 * @idx: Descriptor index.
 *
 * Return: Raw descriptor entry or 0.
 */
u32 rtl8196e_ring_tx_entry(struct rtl8196e_ring *ring, unsigned int idx)
{
	if (!ring || idx >= ring->tx_cnt)
		return 0;
	return ring->tx_ring[idx];
}

/**
 * rtl8196e_ring_rx_index() - Return current RX index.
 * @ring: Ring handle.
 *
 * Return: RX ring index.
 */
unsigned int rtl8196e_ring_rx_index(struct rtl8196e_ring *ring)
{
	if (!ring)
		return 0;
	return ring->rx_idx;
}

/**
 * rtl8196e_ring_rx_pkthdr_entry() - Return RX pkthdr entry value.
 * @ring: Ring handle.
 * @idx: Descriptor index.
 *
 * Return: Raw descriptor entry or 0.
 */
u32 rtl8196e_ring_rx_pkthdr_entry(struct rtl8196e_ring *ring, unsigned int idx)
{
	if (!ring || idx >= ring->rx_cnt)
		return 0;
	return ring->rx_pkthdr_ring[idx];
}

/**
 * rtl8196e_ring_rx_mbuf_entry() - Return RX mbuf entry value.
 * @ring: Ring handle.
 * @idx: Descriptor index.
 *
 * Return: Raw descriptor entry or 0.
 */
u32 rtl8196e_ring_rx_mbuf_entry(struct rtl8196e_ring *ring, unsigned int idx)
{
	if (!ring || idx >= ring->rx_mbuf_cnt)
		return 0;
	return ring->rx_mbuf_ring[idx];
}
