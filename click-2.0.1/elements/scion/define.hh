/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef CLICK_2_0_1_ELEMENTS_SCION_DEFINE_H_
#define CLICK_2_0_1_ELEMENTS_SCION_DEFINE_H_
#endif

/*
#ifndef _SL_DEBUG
#define _SL_DEBUG
#endif
#ifndef _SL_DEBUG_SW
#define _SL_DEBUG_SW
#endif
*/

#ifndef _DEBUG_BS
#define _DEBUG_BS
#endif

#ifndef _DEBUG_CS
#define _DEBUG_CS
#endif
#ifndef _SL_DEBUG_PS
#define _SL_DEBUG_PS
#endif

/*
#ifndef _SL_DEBUG_BS
#define _SL_DEBUG_BS
#endif
#ifndef _SL_ROT_TEST
#define _SL_ROT_TEST
#endif
#ifndef _SL_DEBUG_CS
#define _SL_DEBUG_CS
#endif
#ifndef _SL_DEBUG_RT
#define _SL_DEBUG_RT
#endif
#ifndef _SL_DEBUG_OF
#define _SL_DEBUG_OF
#endif
*/

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
#define MAX_COMMAND_LEN 1024
#define MAX_LOG_LEN 1024
#define MASTER_KEY_LEN 16
#define IFID_SIZE 2
#define AID_SIZE 8

// SL: variable size host address
#define MAX_HOST_ADDR_SIZE 40
#define IPV4_SIZE 4
#define IPV6_SIZE 16
#define SCION_ADDR_SIZE 8
#define AIP_SIZE 40


#define CERT_SIZE 1024


#define SCION_RF 0x8000  /* reserved fragment flag */
#define SCION_DF 0x4000  /* don't fragment flag */
#define SCION_MF 0x2000  /* more fragments flag */
#define SCION_OFFMASK 0X1FFF  /* mask for fragmenting bits */


#define SCION_HEADER_SIZE 40
#define PCB_MARKING_SIZE 32
#define PEER_MARKING_SIZE 24
#define PATH_HOP_SIZE 16
// SL: PATH_HOP_SIZE needs to be removed (confusing)
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

#define PCB_TYPE_CORE 2
#define PCB_TYPE_TRANSIT 1
#define PCB_TYPE_PEER 0
#define NON_PCB 0

#define PORT_TO_SWITCH 0
#define MASTER_SECRET_KEY_SIZE 16 /*in bytes*/

// SL: Bit position in Opaque Field
#define NORMAL_OF 0x00
#define SPECIAL_OF 0x80
#define TDC_XOVR 0x80
#define NON_TDC_XOVR 0xc0
#define INPATH_XOVR 0xe0
#define INTRATD_PEER 0xf0
#define INTERTD_PEER 0xf8
#define PEER_XOVR 0x10

#define MAC_TS_SIZE 3
#define EXP_SIZE 1

// SL:
#define MAX_AD_HOPS 32
#define PATH_TYPE_TDC 0
#define PATH_TYPE_XOVR 1
#define PATH_TYPE_PEER 2

// SL:
#define MAX_UNVERIFIED_PCB 100

// SL:
#define MASK_MSB 0x80
#define UP_PATH_FLAG 0x80  // should be |=;i.e., turn on the first bit
#define DOWN_PATH_FLAG ~(0x80)  // should be &=;i.e., turn off the first bit
#define MASK_EXP_TIME 0x03  // bit mask for taking expiration time from OF

#define STR_SCION "SCION"
#define STR_IPV4 "IPV4"
#define STR_IPV6 "IPV6"
#define STR_AIP "AIP"

#define IPHDR_LEN 20
#define UDPHDR_LEN 8
#define UDPIPHDR_LEN 28
#define SCION_PROTO_NUM 17  // experimental -> UDP
#define SCION_PORT_NUM 33333  // random pick

// SL: added for IP-aware forwarding
#define TO_SERVER 1
#define TO_ROUTER 2

// SL:
#define NO_OF 0
#define WITH_OF 1

#define ROT_REQ_NO 0
#define ROT_REQ_SELF 1
#define ROT_REQ_OTHER 2

#define IFID_REQ_INT 2 /** IFID request interval */

#define TDC_AD 0
#define NON_TDC_AD 1

#endif  // CLICK_2_0_1_ELEMENTS_SCION_DEFINE_H_

