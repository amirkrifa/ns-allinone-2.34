/*
Copyright (c) 2005 Henrik Eriksson and Patrik J�sson.
All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.

*/

/** Definitions for bundle management. 
*/

#include "tools/random.h"
#ifndef DTN_BMFLAGS_H
#define DTN_BMFLAGS_H

/** No bundle header type. */
#define BDL_NONE 0x00
/** Primary bundle header type. */
#define BDL_PRIM 0x01
/** Dictionary header type. */
#define BDL_DICT 0x02
/** Bundle payload header type. */
#define BDL_PYLD 0x05
/** Bundle authentication header type. */
#define BDL_AUTH 0x07
/** Payload security header type. */
#define BDL_PSEC 0x08
/** Bundle fragment header type. */
#define BDL_FRAG 0x09

/** Priority Expedited */
#define BDL_PRIO_EXP  0x10
/** Priority Normal */
#define BDL_PRIO_NORM 0x20
/** Priority Bulk */
#define BDL_PRIO_BULK 0x40
/** Custody switch. */
#define BDL_COS_CUST  0x80

/** Request custody transfer reporting. */
#define BDL_COS_CTREP 0x01
/** Request reporting of bundle reception. */
#define BDL_COS_BREC  0x02
/** Request reporting of bundle forwarding. */
#define BDL_COS_BFWD  0x04
/** Request end-to-end return receipt. */
#define BDL_COS_RET   0x08

/** Priority mask for CoS. */
#define BDL_COS_PRIMASK  0x70
/** Shift length of priority for CoS. */
#define BDL_COS_PRISHIFT 0x10

/** Payload is encrypted. */
#define BDL_PSEC_ENC  0x80
/** Bundle authentication is supported. */
#define BDL_PSEC_AUTH 0x40
/** Bundle integrity is supported. */
#define BDL_PSEC_INT  0x20

/** Payload class Normal. */
#define BDL_PCLASS_NORMAL 0x00
/** Payload class Bundle status report. */
#define BDL_PCLASS_REPORT 0x01
/** Payload class Custodial signal. */
#define BDL_PCLASS_CUST   0x02

/** Bundle status report: Reporting node correctly received bundle. */
#define BDL_RPT_RECV   0x01
/** Bundle status report: Reporting node took custody of bundle. */
#define BDL_RPT_CUST   0x02
/** Bundle status report: Reporting node has forwarded the bundle.  */
#define BDL_RPT_FWD    0x04
/** Bundle status report: Reporting node has delivered the bundle. */
#define BDL_RPT_DELIV  0x08
/** Bundle status report: Bundle's TTL expired at reporting node. */
#define BDL_RPT_EXP    0x10
/** Bundle status report: Bundle was found to be inauthentic. */
#define BDL_RPT_INAUTH 0x20
/** Bundle status report: Report is for a bundle fragment; fragment offset and length fields are present. */
#define BDL_RPT_FRAG   0x80

/** Custody report: Custody transfer succeeded. */
#define BDL_CUST_SUCC  0x01
/** Custody report: Report is for a bundle fragment; fragment offset and length fields are present. */
#define BDL_CUST_FRAG  0x80

/** Custodial signal reason code: Acceptance of custody. */
#define BDL_CUST_ACCEPT  0x01
/** Custodial signal reason code: Delivery to application by non-custodian. */
#define BDL_CUST_DELIV   0x02
/** Custodial signal reason code: Redundant transmission. */
#define BDL_CUST_RDTRANS 0x03
/** Custodial signal reason code: Depleted storage. */
#define BDL_CUST_DEPL    0x04
/** Custodial signal reason code: No known route to dest. */
#define BDL_CUST_ROUTE   0x05
/** Custodial signal reason code: No timely contact with next node. */
#define BDL_CUST_CONTACT 0x06

/** No options. */
#define BDL_OPT_NONE   "NONE"
/** Request custody transfer. */
#define BDL_OPT_CUST   "CUST"
/** Request end-to-end return receipt. */
#define BDL_OPT_EERCPT "EERCPT"
/** Request reporting of bundle reception. */
#define BDL_OPT_RCPT   "REPRCPT"
/** Request reporting of bundle forwarding. */
#define BDL_OPT_FWD    "REPFWD"
/** Request custody transfer reporting. */
#define BDL_OPT_CTREP  "REPCUST"
/** Activate bundle authentication. */
#define BDL_OPT_PSAUTH "PSAUTH"
/** Activate bundle integrity. */
#define BDL_OPT_PSINT  "PSINT"
/** Activate bundle encryption. */
#define BDL_OPT_PSENC  "PSENC"

/** Colour coding for bundles. */
#define BDL_FID_NORM 0
/** Colour coding for bundles asking for custody. */
#define BDL_FID_CUST 1
/** Colour coding for status report. */
#define BDL_FID_REPT 2
/** Colour coding for custody ack. */
#define BDL_FID_CACK 3
/** Colour coding for custody ack reply. */
#define BDL_FID_ACKR 4


#define JITTER ((Random::uniform)*0.5)
#define BUFFER_CLEANING_INTERVAL  0.1

#define TIME_IS (double)((Scheduler::instance().clock()))
//#define NEIGHBORS_CHECK_INTERVAL  121.0
#define MAX_HOPS 800
#define MIN_MESSAGE_LENGTH 5
#define NUMBER_OF_NODES 70
#define MEETING_TIME 100.0
//#define USE_NUMBER_OF_NODES_FROM_STAT 
#endif // DTN_BMFLAGS_H
