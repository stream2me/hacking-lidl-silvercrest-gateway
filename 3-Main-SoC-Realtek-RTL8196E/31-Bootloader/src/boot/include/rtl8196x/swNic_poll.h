/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
*
* Abstract: Switch core polling mode NIC header file.
*
* ---------------------------------------------------------------
*/


#ifndef _SWNIC_H
#define _SWNIC_H
#define CONFIG_RTL865XC 1

#define RTL865X_SWNIC_RXRING_MAX_PKTDESC 6
#define RTL865X_SWNIC_TXRING_MAX_PKTDESC 4



/* --------------------------------------------------------------------
 * ROUTINE NAME - swNic_intHandler
 * --------------------------------------------------------------------
 * FUNCTION: This function is the NIC interrupt handler.
 * INPUT   :
		intPending: Pending interrupts.
 * OUTPUT  : None.
 * RETURN  : None.
 * NOTE    : None.
 * -------------------------------------------------------------------*/
void swNic_intHandler(uint32 intPending);

int32 swNic_init(uint32 userNeedRxPkthdrRingCnt[RTL865X_SWNIC_RXRING_MAX_PKTDESC],
		 uint32 userNeedRxMbufRingCnt,
		 uint32 userNeedTxPkthdrRingCnt[RTL865X_SWNIC_TXRING_MAX_PKTDESC],
		 uint32 clusterSize);
int32 swNic_receive(void** input, uint32* pLen);
int32 swNic_send(void * output, uint32 len);
void swNic_txDone(void); 

#endif /* _SWNIC_H */
