/*****************************************
 * File Name : define.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 28-03-2012

 * Purpose : 

******************************************/

#ifndef _DEBUG_FLAG
#define _DEBUG_FLAG
#endif
/*
#ifndef _SL_DEBUG
#define _SL_DEBUG
#endif
*/
#ifndef _SL_DEBUG_GW
#define _SL_DEBUG_GW
#endif
#ifndef _SL_DEBUG_PS
#define _SL_DEBUG_PS
#endif

#ifndef _DEFINE_HH_
#define _DEFINE_HH_

#include<string.h>
#include<stdio.h>
#include<stdlib.h>

#define COMMON_HEADER_SIZE 16
#define OPAQUE_FIELD_SIZE 8
#define ROT_VERSION_SIZE 4
#define DEFAULT_ADDR_SIZE 20

/*Comments needed*/
#define MAX_FILE_LEN 1024
#define MAXLINELEN 20000
#define MASTER_KEY_LEN 16
#define IFID_SIZE 2
#define AID_SIZE 8

//SL: variable size host address
#define MAX_HOST_ADDR_SIZE 20
#define IPV4_SIZE	4
#define IPV6_SIZE	16
#define SCION_ADDR_SIZE	8
#define AIP_SIZE	20


#define CERT_SIZE 1024


#define	SCION_RF		0x8000		/*         reserved fragment flag    */
#define	SCION_DF		0x4000		/*         don't fragment flag	     */
#define	SCION_MF		0x2000		/*         more fragments flag	     */
#define	SCION_OFFMASK	0X1FFF		/*         mask for fragmenting bits */


#define SCION_HEADER_SIZE     40
#define PCB_MARKING_SIZE   32
#define PEER_MARKING_SIZE  24
#define PATH_HOP_SIZE 16 
//SL: PATH_HOP_SIZE needs to be removed (confusing)
#define SHA1_SIZE 20
#define OFG_KEY_SIZE 16
#define OFG_KEY_SIZE_BITS 128
#define TS_OFG_KEY_SIZE 20
#define MAX_PKT_LEN 65535
#define CERT_INFO_SIZE 16
#define CERT_REQ_SIZE 88 

#define DEFAULT_HD_ROOM 30
#define DEFAULT_TL_ROOM 30

#define BEACON_QUEUE_SIZE 100
#define PS_QUEUE_SIZE 100
#define NUM_REG 10
#define NUM_RUP 3
#define REG_TIME 5
#define PROP_TIME 5
#define LOG_LEVEL 300
#define RESET_TIME 600

#define MAX_TARGET_NUM 10

#define PATH_INFO_SIZE 24
#define KEY_TABLE_SIZE 86400

#define PCB_TYPE_CORE		2
#define PCB_TYPE_TRANSIT	1
#define PCB_TYPE_PEER	0
#define NON_PCB 	0

#define PORT_TO_SWITCH 0
#define MASTER_SECRET_KEY_SIZE 16 /*in bytes*/

//SL: Bit position in Opaque Field
#define NORMAL_OF	0x00
#define SPECIAL_OF	0x80
#define TDC_XOVR	0x80
#define NON_TDC_XOVR	0xc0
#define INPATH_XOVR		0xe0
#define INTRATD_PEER	0xf0
#define INTERTD_PEER	0xf8

#define MAC_TS_SIZE	3
#define EXP_SIZE	1

//SL: 
#define MAX_AD_HOPS 32
#define PATH_TYPE_TDC	0
#define PATH_TYPE_XOVR	1
#define PATH_TYPE_PEER	2

//SL:
#define MAX_UNVERIFIED_PCB	100

//SL:
#define MASK_MSB		0x80

#define STR_SCION	"SCION"
#define STR_IPV4	"IPV4"
#define STR_IPV6	"IPV6"
#define STR_AIP		"AIP"

#endif

