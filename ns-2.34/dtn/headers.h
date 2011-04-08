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
/** \file headers.h  DTN bundle headers. 
*/

#ifndef DTN_HEADERS_H
#define DTN_HEADERS_H

#include <sys/types.h>
#include "common/packet.h"
#include "strrecords.h"
#include "common/simulator.h"
/** Define 64-bit type if not defined. */
#ifndef u_int64_t
//typedef unsigned long long u_int64_t;
#endif

/** Primary Bundle Header */
struct hdr_bdlprim {
  u_int8_t  version;         /**< Bundling protocol version */
  u_int8_t  next_hdr;        /**< Next Header Type */
  u_int8_t  cos;             /**< Class of Service */
  u_int8_t  pld_sec;         /**< Payload Security */
  u_int8_t  dest;            /**< Destination Tuple */
  u_int8_t  src;             /**< Source Tuple */
  u_int8_t  rpt_to;          /**< Report-To Tuple */
  u_int8_t  cust;            /**< Current Custodian Tuple */
  u_int64_t timestamp;       /**< Creation Timestamp */
  double exp_time;        /**< Expiration Time */
  double elapsed_time;    /**< Elapsed Time */

  nsaddr_t  me;
  char      region[8];
  u_int8_t  hopcount;
  int 	    u_id; 
  int  conversation;
  u_int8_t source_rule;
  u_int8_t send_type;
  
  hdr_bdlprim* clone();
  // Header access methods
  static int offset_; /**< required by PacketHeaderManager */
  inline static int& offset() { return offset_; }
  inline static hdr_bdlprim* access(const Packet* p) {
    return (hdr_bdlprim*) p->access(offset_);
  }
};

/** Dictionary Header */
class hdr_bdldict {
 public:
  hdr_bdldict() : next_hdr(0),next_hdr_p(NULL),strcount(0),record(NULL) {}
  ~hdr_bdldict() {delete record;}
  hdr_bdldict* clone();
  u_int8_t       next_hdr;    /**< Next Header Type */
  void*          next_hdr_p;  /**< Next Header */
  int            strcount;    /**< String Record Count */
  DTNStrRecords* record;      /**< String Records */
};

/** Bundle Fragment Header */
class hdr_bdlfrag {
 public:
  hdr_bdlfrag() : next_hdr(0),next_hdr_p(NULL),tot_len(0),offset(0) {}
  ~hdr_bdlfrag() {}
  hdr_bdlfrag* clone();
  u_int8_t  next_hdr;         /**< Next Header Type */
  void*     next_hdr_p;       /**< Next Header */
  u_int32_t tot_len;          /**< Length of Original Bundle Payload */
  u_int32_t offset;           /**< Fragment Offset */
};

/** Bundle Authentication Header */
class hdr_bdlauth {
 public:
  hdr_bdlauth() : next_hdr(0),next_hdr_p(NULL),len(0),info(NULL) {}
  ~hdr_bdlauth() {delete info;}
  hdr_bdlauth* clone();
  u_int8_t  next_hdr;         /**< Next Header Type */
  void*     next_hdr_p;       /**< Next Header */
  u_int8_t  len;              /**< Length */
  char*     info;             /**< Authentication Information */
};

/** Payload Security Header */
class hdr_bdlpsec {
 public:
  hdr_bdlpsec() : next_hdr(0),next_hdr_p(NULL),len(0),info(NULL) {}
  ~hdr_bdlpsec() {delete info;}
  hdr_bdlpsec* clone();
  u_int8_t  next_hdr;         /**< Next Header Type */
  void*     next_hdr_p;       /**< Next Header */
  u_int8_t  len;              /**< Length */
  char*     info;             /**< Security Information */
};

/** Bundle Payload Header */
class hdr_bdlpyld {
 public:
  hdr_bdlpyld() : pldclass(0),len(0),payload(NULL) {}
  ~hdr_bdlpyld() {delete payload;}
  hdr_bdlpyld* clone();
  u_int8_t  pldclass;         /**< Payload Class */
  u_int32_t len;              /**< Length of Payload */
  char*     payload;          /**< Bundle Payload */
};

#endif // DTN_HEADERS_H
