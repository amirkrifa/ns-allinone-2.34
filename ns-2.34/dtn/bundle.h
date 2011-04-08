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

/** \file bundle.h  Bundle. 
*/

#ifndef DTN_BUNDLE_H
#define DTN_BUNDLE_H

#include <sys/types.h>
#include "headers.h"

/** Define 64-bit type if not defined. */
#ifndef u_int64_t
//typedef unsigned long long u_int64_t;
#endif

/** Status report information. 
 */
class bdl_report{
 public:
  bdl_report() : type(0),flags(0),reason(0),reg_id(NULL),adm_id(NULL),dest(NULL) {} /**< Constructor for a status report. */
  ~bdl_report() {delete dest;}                                    /**< Destructor for a status report.  */
  int       type;       /**< Report type. */
  u_int8_t  flags;      /**< Status flags. */
  u_int8_t  reason;     /**< Reason code (for custody report). */
  u_int32_t frag_off;   /**< Fragment offset (if present). */
  u_int32_t frag_len;   /**< Fragment length (if present). */
  u_int64_t timestamp;  /**< Copy of send timestamp. */
  u_int64_t tos;        /**< Time of signal (for custody report). */
  u_int64_t tor;        /**< Time of receipt of bundle (if present). */
  u_int64_t tof;        /**< Time of forwarding of bundle (if present). */
  u_int64_t todeliv;    /**< Time of delivery of bundle (if present). */
  u_int64_t todelet;    /**< Time of deletion of bundle (if present). */
  u_int8_t  reg_len;    /**< Region length. */
  u_int8_t* reg_id;     /**< Region ID of source endpoint. */
  u_int8_t  adm_len;    /**< Administrative length. */
  u_int8_t* adm_id;     /**< Administrative part of source endpoint. */
  char*     dest;       /**< Destination of status report. */
};

/** A Bundle. 
 */
class Bundle {
 public:
	Bundle();
	~Bundle();
	int getDataSize(int frag=0);
	int extractData(u_int8_t* buf, int buflen);
	int retrieveData(u_int8_t* data, int datalen);
	int generateReport(u_int8_t rflags, u_int8_t cflags=0, u_int8_t reason=0);
	void fragment(size_t mtu, int exact=0);
	Bundle* defragment(Bundle** frags);
	
	void ShowDetails()
	{
		fprintf(stdout,"	Source Tuple: %s Destination Tuple: %s Expiration Time: %f Elapsed time: %f \n",bdl_dict_->record->getRecords(bdl_prim_->src), bdl_dict_->record->getRecords(bdl_prim_->dest), bdl_prim_->exp_time, bdl_prim_->elapsed_time);
	}
	
	hdr_bdlprim* bdl_prim_;   /**<  Primary header. */
	hdr_bdldict* bdl_dict_;   /**<  Dictionary header. */
	Bundle* next_;            /**<  Next bundle. */
	Bundle* next_frag_;       /**<  Next bundle fragment. */
	bdl_report* report_;      /**<  Optional Bundle Status Report. */
	bdl_report* custreport_;  /**<  Optional Custody Status Report. */
	int token_;               /**<  Send token. */
	nsaddr_t prevhop_;        /**<  Previous hop. */
	double transtime_;        /**<  Last transmission time. */
	double recvtime_;         /**<  Time of reception. */
	int fid_;                 /**<  Field id. (Used for colour coding.) */

};


#endif // DTN_BUNDLE_H
