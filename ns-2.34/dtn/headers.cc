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
 DAMAGES (ICLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.

*/

/** \file headers.cc  DTN bundle headers. 
*/

#include "headers.h"
#include "debug.h"

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdlprim* hdr_bdlprim :: clone()
{
  hdr_bdlprim* hdr = new hdr_bdlprim();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->version   = version;
  hdr->next_hdr  = next_hdr;
  hdr->cos       = cos;
  hdr->pld_sec   = pld_sec;
  hdr->dest      = dest;
  hdr->src       = src;
  hdr->rpt_to    = rpt_to;
  hdr->cust      = cust;
  hdr->timestamp = timestamp;
  hdr->exp_time  = exp_time;
  hdr->elapsed_time=elapsed_time;
  hdr->me=me;
  strcpy(hdr->region,region);
  hdr->hopcount=hopcount;
  hdr->u_id=u_id;
  return hdr;
}

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdldict* hdr_bdldict :: clone()
{
  hdr_bdldict* hdr = new hdr_bdldict();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->next_hdr = next_hdr;
  hdr->next_hdr_p = next_hdr_p;
  hdr->strcount = strcount;
  hdr->record = record->clone();
  return hdr;
}

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdlfrag* hdr_bdlfrag :: clone()
{
  hdr_bdlfrag* hdr = new hdr_bdlfrag();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->next_hdr = next_hdr;
  hdr->next_hdr_p = next_hdr_p;
  hdr->tot_len = tot_len;
  hdr->offset = offset;
  return hdr;
}

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdlpyld* hdr_bdlpyld :: clone()
{
  hdr_bdlpyld* hdr = new hdr_bdlpyld();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->pldclass = pldclass;
  hdr->len = len;
  hdr->payload = new char[len];
  if(!hdr->payload){DPRINT(DEB_ERR, "Could not allocate memory."); abort();}
  memcpy(hdr->payload,payload,len);
  return hdr;
}

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdlauth* hdr_bdlauth :: clone()
{
  hdr_bdlauth* hdr = new hdr_bdlauth();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->next_hdr = next_hdr;
  hdr->next_hdr_p = next_hdr_p;
  hdr->len = len;
  hdr->info = new char[len];
  if(!hdr->info){DPRINT(DEB_ERR, "Could not allocate memory."); abort();}
  memcpy(hdr->info,info,len);
  return hdr;
}

/** Clone header.
 *
 * \returns Cloned header
 */
hdr_bdlpsec* hdr_bdlpsec :: clone()
{
  hdr_bdlpsec* hdr = new hdr_bdlpsec();
  if(!hdr){DPRINT(DEB_ERR, "Could not allocate memory for header."); abort();}
  hdr->next_hdr = next_hdr;
  hdr->next_hdr_p = next_hdr_p;
  hdr->len = len;
  hdr->info = new char[len];
  if(!hdr->info){DPRINT(DEB_ERR, "Could not allocate memory."); abort();}
  memcpy(hdr->info,info,len);
  return hdr;
}
