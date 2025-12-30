/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
*
* Abstract: Switch core vlan table access driver source code.
*
*
* ---------------------------------------------------------------
*/
#include <rtl_types.h>
#include <rtl_errno.h>
#include <rtl8196x/asicregs.h>
#include <rtl8196x/swCore.h>
#include <rtl8196x/vlanTable.h>

#include <rtl8196x/swTable.h>
extern void tableAccessForeword(uint32, uint32, void *);
extern int32 swTable_addEntry(uint32 tableType, uint32 eidx,
			      void *entryContent_P);
extern int32 swTable_modifyEntry(uint32 tableType, uint32 eidx,
				 void *entryContent_P);
extern int32 swTable_forceAddEntry(uint32 tableType, uint32 eidx,
				   void *entryContent_P);
extern int32 swTable_readEntry(uint32 tableType, uint32 eidx,
			       void *entryContent_P);

#include <asm/mipsregs.h>
//wei add
int
lx4180_ReadStatus()
{
	volatile unsigned int reg;
	reg = read_32bit_cp0_register(CP0_STATUS);
	__asm__ volatile ("nop");	// david
	__asm__ volatile ("nop");
	return reg;

}

void
lx4180_WriteStatus(int s)
{
	volatile unsigned int reg = s;
	write_32bit_cp0_register(CP0_STATUS, reg);
	__asm__ volatile ("nop");	// david
	__asm__ volatile ("nop");
	return;

}

/* STATIC VARIABLE DECLARATIONS
 */

/* LOCAL SUBPROGRAM SPECIFICATIONS
 */

int32
swCore_netifCreate(uint32 idx, rtl_netif_param_t * param)
{
	netif_table_t entryContent;
	uint32 temp, temp2;

	ASSERT_CSP(param);

	// disable interrupt
	// I don't know the reason but if you want to use "-O" flag, must disalbe interrupt before swTable_readEntry();
	temp = lx4180_ReadStatus();
	if (0 != temp & 0x1) {
		temp2 = temp & 0xfffffffe;
		lx4180_WriteStatus(temp2);
	}

	swTable_readEntry(TYPE_NETINTERFACE_TABLE, idx, &entryContent);

	// restore status register
	if (0 != temp & 0x1) {
		lx4180_WriteStatus(temp);
	}

	if (entryContent.valid) {
		return EEXIST;
	}

	bzero((void *)&entryContent, sizeof (entryContent));
	entryContent.valid = param->valid;
	entryContent.vid = param->vid;

	entryContent.mac47_19 =
	    ((param->gMac.mac47_32 << 13) | (param->gMac.
					     mac31_16 >> 3)) & 0xFFFFFFF;
	entryContent.mac18_0 =
	    ((param->gMac.mac31_16 << 16) | param->gMac.mac15_0) & 0x7FFFF;

	entryContent.inACLStartH = (param->inAclStart >> 2) & 0x1f;
	entryContent.inACLStartL = param->inAclStart & 0x3;
	entryContent.inACLEnd = param->inAclEnd;
	entryContent.outACLStart = param->outAclStart;
	entryContent.outACLEnd = param->outAclEnd;
	entryContent.enHWRoute = param->enableRoute;

	entryContent.macMask = 8 - (param->macAddrNumber & 0x7);

	entryContent.mtuH = param->mtu >> 3;
	entryContent.mtuL = param->mtu & 0x7;

	/* Write into hardware */
	if (swTable_addEntry(TYPE_NETINTERFACE_TABLE, idx, &entryContent) == 0)
		return 0;
	else
		/* There might be something wrong */
		ASSERT_CSP(0);
}

int32
vlanTable_create(uint32 vid, rtl_vlan_param_t * param)
{
	vlan_table_t entryContent;
	uint32 temp, temp2;

	ASSERT_CSP(param);

	// disable interrupt
	// I don't know the reason but if you want to use "-O" flag, must disalbe interrupt before swTable_readEntry();
	temp = lx4180_ReadStatus();
	if (0 != temp & 0x1) {
		temp2 = temp & 0xfffffffe;
		lx4180_WriteStatus(temp2);
	}

	swTable_readEntry(TYPE_VLAN_TABLE, vid, &entryContent);

	// restore status register
	if (0 != temp & 0x1) {
		lx4180_WriteStatus(temp);
	}

	bzero((void *)&entryContent, sizeof (entryContent));
	entryContent.memberPort = param->memberPort & ALL_PORT_MASK;
	entryContent.egressUntag = param->egressUntag;
	entryContent.fid = param->fid;

	entryContent.vid = vid;

	/* Write into hardware */
	if (swTable_addEntry(TYPE_VLAN_TABLE, vid, &entryContent) == 0)
		return 0;
	else
		/* There might be something wrong */
		ASSERT_CSP(0);
}

int32
vlanTable_setStpStatusOfAllPorts(uint32 vid, uint32 STPStatus)
{

}
