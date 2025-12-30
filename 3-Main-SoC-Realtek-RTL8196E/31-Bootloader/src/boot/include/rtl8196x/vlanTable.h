/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
*
* Abstract: Switch core vlan table access header file.
*
* ---------------------------------------------------------------
*/

#ifndef _VLANTABLE_H_
#define _VLANTABLE_H_

#include <boot_config.h>
#include <rtl_types.h>

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint32          memberPort;
    /* word 1 */
    uint32          egressUntag;
    /* word 2 */
    uint32          fid:2;
    uint32          vid:12;
    uint32          reserv0:18;
    /* word 3 */
    uint32          reserv1;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
#else /*LITTLE_ENDIAN*/
    /* word 0 */
    uint32          memberPort;
    /* word 1 */
    uint32          egressUntag;
    /* word 2 */
    uint32          reserv0:18;
    uint32          vid:12;
    uint32          fid:2;
    /* word 3 */
    uint32          reserv1;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
#endif /*LITTLE_ENDIAN*/
} vlan_table_t;

typedef struct {
    /* word 0 */
    uint32          mac18_0:19;
    uint32          vid		 : 12;
    uint32          valid       : 1;	
    /* word 1 */
    uint32         inACLStartL:2;	
    uint32         enHWRoute : 1;	
    uint32         mac47_19:29;

    /* word 2 */
    uint32         mtuL       : 3;
    uint32         macMask :3;	
    uint32         outACLEnd : 7;	
    uint32         outACLStart : 7;	
    uint32         inACLEnd : 7;	
    uint32         inACLStartH: 5;	
    /* word 3 */
    uint32          reserv10   : 20;
    uint32          mtuH       : 12;

    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} netif_table_t;


/* VLAN table access routines 
*/

/* Create vlan 
Return: EEXIST- Speicified vlan already exists.
        ENFILE- Destined slot occupied by another vlan.*/
int32 vlanTable_create(uint32 vid, rtl_vlan_param_t * param);

/* Destroy vlan 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_destroy(uint32 vid);

/* Add a member port
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_addMemberPort(uint32 vid, uint32 portNum);

/* Remove a member port 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_removeMemberPort(uint32 vid, uint32 portNum);

/* Set a member port list 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setMemberPort(uint32 vid, uint32 portList);

/* Set ACL rule 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setAclRule(uint32 vid, uint32 inACLStart, uint32 inACLEnd,
                                uint32 outACLStart, uint32 outACLEnd);

/* Get ACL rule 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_getAclRule(uint32 vid, uint32 *inACLStart_P, uint32 *inACLEnd_P,
                                uint32 *outACLStart_P, uint32 *outACLEnd_P);

/* Set vlan as internal interface 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setInternal(uint32 vid);

/* Set vlan as external interface 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setExternal(uint32 vid);

/* Enable hardware routing for this vlan 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_enableHardwareRouting(uint32 vid);

/* Disable hardware routing for this vlan 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_disableHardwareRouting(uint32 vid);

/* Set spanning tree status 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setPortStpStatus(uint32 vid, uint32 portNum, uint32 STPStatus);

/* Get spanning tree status 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_getPortStpStatus(uint32 vid, uint32 portNum, uint32 *STPStatus_P);

/* Set spanning tree status 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_setStpStatus(uint32 vid, uint32 STPStatus);

/* Get information 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_getInformation(uint32 vid, rtl_vlan_param_t * param_P);

/* Get hardware information 
Return: ENOENT- Specified vlan id does not exist.*/
int32 vlanTable_getHwInformation(uint32 vid, rtl_vlan_param_t * param_P);

/* Get vlan id 
Return: ENOENT- Specified slot does not exist.*/
int32 vlanTable_getVidByIndex(uint32 eidx, uint32 * vid_P);

#define CONFIG_RTL865XC 1


#endif /*_VLANTABLE_H_*/
