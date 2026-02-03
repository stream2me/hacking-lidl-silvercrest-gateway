// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "rtl8196e_hw.h"

static inline void rtl8196e_writel(u32 val, u32 reg)
{
	*(volatile u32 *)(reg) = val;
}

static inline u32 rtl8196e_readl(u32 reg)
{
	return *(volatile u32 *)(reg);
}

static int rtl8196e_mdio_wait_ready(void)
{
	int i;

	for (i = 0; i < 1000; i++) {
		if ((rtl8196e_readl(MDCIOSR) & MDC_STATUS) == 0)
			return 0;
		udelay(10);
	}

	return -ETIMEDOUT;
}

static int rtl8196e_mdio_read(int phy, int reg, u16 *val)
{
	int ret;

	rtl8196e_writel(COMMAND_READ | (phy << PHYADD_OFFSET) | (reg << REGADD_OFFSET), MDCIOCR);
	ret = rtl8196e_mdio_wait_ready();
	if (ret)
		return ret;

	*val = rtl8196e_readl(MDCIOSR) & 0xffff;
	return 0;
}

static int rtl8196e_mdio_write(int phy, int reg, u16 val)
{
	rtl8196e_writel(COMMAND_WRITE | (phy << PHYADD_OFFSET) | (reg << REGADD_OFFSET) | val, MDCIOCR);
	return rtl8196e_mdio_wait_ready();
}

static int rtl8196e_table_wait_ready(void)
{
	int i;

	for (i = 0; i < 1000; i++) {
		if ((rtl8196e_readl(TBL_ACCESS_CTRL) & TBL_ACCESS_BUSY) == 0)
			return 0;
		udelay(10);
	}

	return -ETIMEDOUT;
}

static int rtl8196e_l2_write_entry(u32 index, u32 word0, u32 word1)
{
	u32 addr = ASIC_TABLE_BASE + (index << 5);
	u32 swtcr;
	int ret;
	int i;

	ret = rtl8196e_table_wait_ready();
	if (ret)
		return ret;

	/* TLU access handshake (mirrors vendor flow) */
	swtcr = rtl8196e_readl(SWTCR0);
	rtl8196e_writel(swtcr | SWTCR0_TLU_START, SWTCR0);
	for (i = 0; i < 1000; i++) {
		if (rtl8196e_readl(SWTCR0) & SWTCR0_TLU_BUSY)
			break;
		udelay(10);
	}

	rtl8196e_writel(word0, TBL_ACCESS_DATA);
	rtl8196e_writel(word1, TBL_ACCESS_DATA + 0x04);
	rtl8196e_writel(addr, TBL_ACCESS_ADDR);
	rtl8196e_writel(TBL_ACCESS_CMD_WRITE, TBL_ACCESS_CTRL);

	ret = rtl8196e_table_wait_ready();
	rtl8196e_writel(swtcr & ~(SWTCR0_TLU_START | SWTCR0_TLU_BUSY), SWTCR0);
	if (ret)
		return ret;

	if (rtl8196e_readl(TBL_ACCESS_STAT) & 0x1)
		return -EIO;

	/* Direct mirror write (some silicon revisions only latch from table RAM) */
	rtl8196e_writel(word0, addr + 0x00);
	rtl8196e_writel(word1, addr + 0x04);
	rtl8196e_writel(0, addr + 0x08);
	rtl8196e_writel(0, addr + 0x0c);
	rtl8196e_writel(0, addr + 0x10);
	rtl8196e_writel(0, addr + 0x14);
	rtl8196e_writel(0, addr + 0x18);
	rtl8196e_writel(0, addr + 0x1c);

	return 0;
}

static int rtl8196e_l2_read_entry(u32 index, u32 *word0, u32 *word1)
{
	u32 addr = ASIC_TABLE_BASE + (index << 5);
	u32 a[8];
	u32 b[8];
	int i;
	int j;
	int ret;

	if (!word0 || !word1)
		return -EINVAL;

	ret = rtl8196e_table_wait_ready();
	if (ret)
		return ret;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 8; j++)
			a[j] = rtl8196e_readl(addr + (j * 4));
		for (j = 0; j < 8; j++)
			b[j] = rtl8196e_readl(addr + (j * 4));
		if (memcmp(a, b, sizeof(a)) == 0) {
			*word0 = a[0];
			*word1 = a[1];
			return 0;
		}
	}

	return -EIO;
}

int rtl8196e_hw_init(struct rtl8196e_hw *hw)
{
	(void)hw;
	/* MEMCR init is mandatory */
	rtl8196e_writel(0, MEMCR);
	rtl8196e_writel(0x7f, MEMCR);

	/* Full reset of switch core */
	rtl8196e_writel(FULL_RST, SIRR);
	mdelay(300);

	/* Start TX/RX */
	rtl8196e_writel(TRXRDY, SIRR);
	mdelay(1);

	/* Clear pending interrupts */
	rtl8196e_writel(rtl8196e_readl(CPUIISR), CPUIISR);

	return 0;
}

int rtl8196e_hw_init_phy(struct rtl8196e_hw *hw, int port, int phy_id)
{
	u32 pcr;
	u16 bmcr;
	int ret;

	(void)hw;
	if (port < 0 || phy_id < 0)
		return -EINVAL;

	pcr = rtl8196e_readl(PCRP0 + (port << 2));
	pcr |= EnablePHYIf | MacSwReset;
	rtl8196e_writel(pcr, PCRP0 + (port << 2));
	udelay(10);
	pcr &= ~MacSwReset;
	rtl8196e_writel(pcr, PCRP0 + (port << 2));

	ret = rtl8196e_mdio_read(phy_id, 0, &bmcr);
	if (ret)
		return ret;

	bmcr |= (1 << 12) | (1 << 9); /* AN enable + restart */
	return rtl8196e_mdio_write(phy_id, 0, bmcr);
}

bool rtl8196e_hw_link_up(struct rtl8196e_hw *hw, int port)
{
	u32 status;

	(void)hw;
	if (port < 0)
		return false;

	status = rtl8196e_readl(PSRP0 + (port << 2));
	return (status & PortStatusLinkUp) != 0;
}

void rtl8196e_hw_l2_setup(struct rtl8196e_hw *hw)
{
	u32 swtcr;
	u32 ffcr;
	u32 cscr;
	u32 mscr;
	u32 teacr;

	(void)hw;
	mscr = rtl8196e_readl(MSCR);
	mscr |= EN_L2;
	mscr &= ~(EN_L3 | EN_L4);
	rtl8196e_writel(mscr, MSCR);

	teacr = rtl8196e_readl(TEACR);
	teacr &= ~0x3; /* enable L2 aging, disable L4 aging */
	rtl8196e_writel(teacr, TEACR);

	swtcr = rtl8196e_readl(SWTCR0);
	swtcr &= ~LIMDBC_MASK;
	swtcr |= LIMDBC_VLAN;
	swtcr &= ~NAPTF2CPU;
	rtl8196e_writel(swtcr, SWTCR0);

	ffcr = rtl8196e_readl(FFCR);
	ffcr &= ~(EN_UNUNICAST_TOCPU | EN_UNMCAST_TOCPU);
	rtl8196e_writel(ffcr, FFCR);

	cscr = rtl8196e_readl(CSCR);
	cscr &= ~(ALLOW_L2_CHKSUM_ERR | ALLOW_L3_CHKSUM_ERR | ALLOW_L4_CHKSUM_ERR);
	rtl8196e_writel(cscr, CSCR);
}

void rtl8196e_hw_l2_trap_enable(struct rtl8196e_hw *hw)
{
	u32 swtcr;
	u32 ffcr;
	u32 cscr;

	(void)hw;
	swtcr = rtl8196e_readl(SWTCR0);
	swtcr &= ~LIMDBC_MASK;
	swtcr |= LIMDBC_VLAN | NAPTF2CPU;
	rtl8196e_writel(swtcr, SWTCR0);

	ffcr = rtl8196e_readl(FFCR);
	ffcr |= EN_UNUNICAST_TOCPU | EN_UNMCAST_TOCPU;
	rtl8196e_writel(ffcr, FFCR);

	cscr = rtl8196e_readl(CSCR);
	cscr &= ~(ALLOW_L2_CHKSUM_ERR | ALLOW_L3_CHKSUM_ERR | ALLOW_L4_CHKSUM_ERR);
	rtl8196e_writel(cscr, CSCR);
}

int rtl8196e_hw_l2_add_cpu_entry(struct rtl8196e_hw *hw, const u8 *mac, u8 fid)
{
	static const u8 fid_hash[] = { 0x00, 0x0f, 0xf0, 0xff };
	u32 row;
	u32 index;
	u32 word0;
	u32 word1;

	(void)hw;
	if (!mac)
		return -EINVAL;

	fid &= 0x3;
	row = (mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5] ^ fid_hash[fid]) & 0xff;
	index = (row << 2);

	word0 = ((u32)mac[1] << 24) | ((u32)mac[2] << 16) | ((u32)mac[3] << 8) | mac[4];
	word1 = ((u32)mac[0] << 24) |
		(1 << 14) | /* toCPU */
		(1 << 13) | /* isStatic */
		(1 << 9) |  /* nhFlag */
		((u32)fid << 7);

	return rtl8196e_l2_write_entry(index, word0, word1);
}

int rtl8196e_hw_l2_check_cpu_entry(struct rtl8196e_hw *hw, const u8 *mac, u8 fid)
{
	static const u8 fid_hash[] = { 0x00, 0x0f, 0xf0, 0xff };
	u32 row;
	u32 index;
	u32 word0;
	u32 word1;
	u32 expected0;
	u32 expected1;
	u32 mask;
	int ret;
	int tries;

	(void)hw;
	if (!mac)
		return -EINVAL;

	fid &= 0x3;
	row = (mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5] ^ fid_hash[fid]) & 0xff;
	index = (row << 2);

	expected0 = ((u32)mac[1] << 24) | ((u32)mac[2] << 16) | ((u32)mac[3] << 8) | mac[4];
	expected1 = ((u32)mac[0] << 24) |
		(1 << 14) | /* toCPU */
		(1 << 13) | /* isStatic */
		(1 << 9) |  /* nhFlag */
		((u32)fid << 7);
	mask = 0xff000000 | (1 << 14) | (1 << 13) | (1 << 9) | (3 << 7);

	for (tries = 0; tries < 50; tries++) {
		ret = rtl8196e_l2_read_entry(index, &word0, &word1);
		if (ret)
			return ret;
		if (word0 == expected0 && (word1 & mask) == expected1)
			return 0;
		udelay(10);
	}

	pr_warn("rtl8196e-eth: L2 verify mismatch row=%u idx=%u exp0=0x%08x exp1=0x%08x got0=0x%08x got1=0x%08x\n",
		row, index, expected0, expected1, word0, word1);
	return -EIO;
}

void rtl8196e_hw_start(struct rtl8196e_hw *hw)
{
	u32 icr = TXCMD | RXCMD | BUSBURST_32WORDS | MBUF_2048BYTES | EXCLUDE_CRC;
	(void)hw;
	rtl8196e_writel(icr, CPUICR);
}

void rtl8196e_hw_stop(struct rtl8196e_hw *hw)
{
	u32 icr = rtl8196e_readl(CPUICR);
	(void)hw;
	icr &= ~(TXCMD | RXCMD);
	rtl8196e_writel(icr, CPUICR);
}

void rtl8196e_hw_set_rx_rings(struct rtl8196e_hw *hw, void *pkthdr, void *mbuf)
{
	(void)hw;
	rtl8196e_writel((u32)rtl8196e_uncached_addr(pkthdr), CPURPDCR0);
	rtl8196e_writel((u32)rtl8196e_uncached_addr(mbuf), CPURMDCR0);
}

void rtl8196e_hw_set_tx_ring(struct rtl8196e_hw *hw, void *pkthdr)
{
	(void)hw;
	rtl8196e_writel((u32)rtl8196e_uncached_addr(pkthdr), CPUTPDCR0);
}

void rtl8196e_hw_enable_irqs(struct rtl8196e_hw *hw)
{
	u32 mask = RX_DONE_IE_ALL | TX_ALL_DONE_IE_ALL | LINK_CHANGE_IE | PKTHDR_DESC_RUNOUT_IE_ALL;
	(void)hw;
	rtl8196e_writel(mask, CPUIIMR);
}

void rtl8196e_hw_disable_irqs(struct rtl8196e_hw *hw)
{
	(void)hw;
	rtl8196e_writel(0, CPUIIMR);
}
