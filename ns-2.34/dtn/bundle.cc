/*
Copyright (c) 2005 Henrik Eriksson and Patrik J�nsson.
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

/** \file bundle.cc  Bundle. 
*/

#include "bundle.h"
#include "debug.h"
#include "bmflags.h"
#include "common/node.h"
#include <time.h>
#include <stdlib.h>
u_int64_t getTimestamp();

/** Bundle constructor. 
 */
Bundle :: Bundle() : bdl_prim_(NULL), 
		     bdl_dict_(NULL), 
		     next_(NULL), 
		     next_frag_(NULL), 
		     report_(NULL), 
		     custreport_(NULL), 
		     token_(0), 
		     prevhop_(-1), 
		     transtime_(0.0), 
		     recvtime_(0.0), 
		     fid_(BDL_FID_NORM)
{
  static int token = 1;
  token_ = token++;

}

/** Bundle destructor. 
 */
Bundle :: ~Bundle()
{

  DPRINT(DEB_INFO, "Deleting bundle with token %d.",token_);
  if(next_){
    DPRINT(DEB_WARN,"Removing bundle with next_ not NULL.");
  }
  delete report_;
  delete custreport_;
  delete bdl_prim_;
  int curhdr = BDL_DICT;
  void* curhdr_p = bdl_dict_;
  void* nextp;
  while(curhdr_p){
    switch(curhdr){
    case BDL_DICT:
      nextp = ((hdr_bdldict*) curhdr_p) -> next_hdr_p;
      curhdr = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      delete ((hdr_bdldict*) curhdr_p);
      curhdr_p = nextp;
      break;
    case BDL_PYLD:
      delete ((hdr_bdlpyld*) curhdr_p);
      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    case BDL_AUTH:
      nextp = ((hdr_bdlauth*) curhdr_p) -> next_hdr_p;
      curhdr = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      delete ((hdr_bdlauth*) curhdr_p);
      curhdr_p = nextp;
      break;
    case BDL_PSEC:
      nextp = ((hdr_bdlpsec*) curhdr_p) -> next_hdr_p;
      curhdr = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      delete ((hdr_bdlpsec*) curhdr_p);
      curhdr_p = nextp;
      break;
    case BDL_FRAG:
      nextp = ((hdr_bdlfrag*) curhdr_p) -> next_hdr_p;
      curhdr = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      delete ((hdr_bdlfrag*) curhdr_p);
      curhdr_p = nextp;
      break;
    default:
      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    }
  }
}


/** Calculates the total data size of all headers located after
 *  the primary header in the bundle. 
 *
 * \returns The sum of all header lengths. 
 */
int Bundle :: getDataSize(int frag)
{
  int curhdr = BDL_DICT;
  void* curhdr_p = (void*) bdl_dict_;
  int bcount = 0;
  int count;

  while(curhdr_p){
    count = 0;
    switch(curhdr) {
    case BDL_DICT:
      if(frag == 0 || frag == 1){
	bcount += 2;
	while(count < ((hdr_bdldict*) curhdr_p)->strcount){
	  StrRec* rec = ((hdr_bdldict*) curhdr_p)->record->getRecord(count);
	  bcount += 1+rec->len_;
	  count++;
	}
      }
      curhdr = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdldict*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_PYLD:
      if(frag==0) bcount += 1+sizeof(u_int32_t)+((hdr_bdlpyld*) curhdr_p)->len;
      if(frag==1) bcount += 1+sizeof(u_int32_t);
      if(frag==2) bcount +=                     ((hdr_bdlpyld*) curhdr_p)->len;
      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    case BDL_AUTH:
      if(frag == 0 || frag == 1) bcount += 2+((hdr_bdlauth*) curhdr_p)->len;
      curhdr = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlauth*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_PSEC:
      if(frag == 0 || frag == 1) bcount += 2+((hdr_bdlpsec*) curhdr_p)->len;
      curhdr = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlpsec*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_FRAG:
      if(frag == 0) bcount += 1+2*sizeof(u_int32_t);
      curhdr = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlfrag*) curhdr_p) -> next_hdr_p;
      break;
    default:
      DPRINT(DEB_ERR, "Unknown header type. %d@%08x.", curhdr, curhdr_p);
      abort();
      break;
    }
  }
  return bcount;
}

/** Extracts data from all headers in the bundle into a buffer. 
 * 
 * \param buf    The buffer into which to copy data.
 * \param buflen The size of the buffer.
 * 
 * \returns The size of all data copied into the buffer. Should match the
 *          argument buflen.
 * \retval -1 on failure. 
 */
int Bundle :: extractData(u_int8_t* buf, int buflen)
{
  if(!buf || !bdl_dict_){DPRINT(DEB_WARN, "buf (%08x) or bdl_dict_ (%08x) is null.",buf,bdl_dict_);return -1;}

  int curhdr = BDL_DICT;
  void* curhdr_p = (void*) bdl_dict_;
  int bcount = 0;
  int count;

  while(curhdr_p){
    count = 0;
    switch(curhdr) {
    case BDL_DICT:
      if(bcount+2 > buflen){DPRINT(DEB_WARN, "Not enough space in buf."); return -1;}
      buf[bcount++] = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      buf[bcount++] = ((hdr_bdldict*) curhdr_p)->strcount;
      while(count < ((hdr_bdldict*) curhdr_p)->strcount){
	StrRec* rec = ((hdr_bdldict*) curhdr_p)->record->getRecord(count);
	if(bcount+rec->len_+1 > buflen) {DPRINT(DEB_WARN, "Not enough space in buf."); return -1;}
	buf[bcount++] = rec->len_;
	memcpy(&buf[bcount], rec->text_, (size_t) rec->len_);
	bcount+=rec->len_;
	count++;
      }

      curhdr = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdldict*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_PYLD:
      if(bcount+5+((hdr_bdlpyld*) curhdr_p)->len > buflen) {
	DPRINT(DEB_WARN, "Not enough space in buf. %d > %d", bcount+5+((hdr_bdlpyld*) curhdr_p)->len , buflen); 
	return -1;
      }
      buf[bcount++] = ((hdr_bdlpyld*) curhdr_p) -> pldclass;
      memcpy(&buf[bcount], &((hdr_bdlpyld*) curhdr_p)->len, sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);
      memcpy(&buf[bcount], ((hdr_bdlpyld*) curhdr_p)->payload, ((hdr_bdlpyld*) curhdr_p)->len);
      bcount+=((hdr_bdlpyld*) curhdr_p)->len;

      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    case BDL_AUTH:
      if(bcount+2+((hdr_bdlauth*) curhdr_p)->len > buflen) {
	DPRINT(DEB_WARN, "Not enough space in buf."); 
	return -1;
      }
      buf[bcount++] = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      buf[bcount++] = ((hdr_bdlauth*) curhdr_p) -> len;
      memcpy(&buf[bcount], ((hdr_bdlauth*) curhdr_p)->info, ((hdr_bdlauth*) curhdr_p)->len);
      bcount+=((hdr_bdlauth*) curhdr_p)->len;

      curhdr = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlauth*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_PSEC:
      if(bcount+2+((hdr_bdlpsec*) curhdr_p)->len > buflen) {
	DPRINT(DEB_WARN, "Not enough space in buf."); 
	return -1;
      }
      buf[bcount++] = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      buf[bcount++] = ((hdr_bdlpsec*) curhdr_p) -> len;
      memcpy(&buf[bcount], ((hdr_bdlpsec*) curhdr_p)->info, ((hdr_bdlpsec*) curhdr_p)->len);
      bcount+=((hdr_bdlpsec*) curhdr_p)->len;

      curhdr = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlpsec*) curhdr_p) -> next_hdr_p;
      break;
    case BDL_FRAG:
      if(bcount+1+2*sizeof(u_int32_t) > buflen) {DPRINT(DEB_WARN, "Not enough space in buf."); return -1;}
      buf[bcount++] = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      memcpy(&buf[bcount], &((hdr_bdlfrag*) curhdr_p)->tot_len, sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);
      memcpy(&buf[bcount], &((hdr_bdlfrag*) curhdr_p)->offset, sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);

      curhdr = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlfrag*) curhdr_p) -> next_hdr_p;
      break;
    default:
      DPRINT(DEB_WARN, "Unknown header type.");
      abort();
      break;
    }
  }
  return bcount;
}

/** Retrieves data from a buffer into matching headers in the bundle.
 *  It assumes that the fist header in the buffer is a dictionary header
 *  and recreates string records. 
 *
 * \param buf The buffer from which to copy data.
 * \param buflen The size of the buffer.
 * 
 * \returns The size of all data copied from the buffer. Should match the
 * argument buflen.
 * \retval -1 on failure. 
 */
int Bundle :: retrieveData(u_int8_t* buf, int buflen)
{
  if(!buf || !bdl_dict_){DPRINT(DEB_WARN, "buf (%08x) or bdl_dict_ (%08x) is null.",buf,bdl_dict_);return -1;}
  int curhdr = BDL_DICT;
  void* curhdr_p = (void*) bdl_dict_;
  void** prev_nextp;
  int bcount = 0;
  int count;
  DTNStrRecords* rec;

  while(curhdr){
    count = 0;
    switch(curhdr) {
    case BDL_DICT:
      if(bcount+2 > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdldict*) curhdr_p)->next_hdr = buf[bcount++];
      ((hdr_bdldict*) curhdr_p)->strcount = buf[bcount++];
      rec = new DTNStrRecords();
      if(!rec){DPRINT(DEB_ERR, "Could not create String Records."); abort();}
      while(count < ((hdr_bdldict*) curhdr_p)->strcount){
	int len = buf[bcount++];
	if(rec->addRecord(&buf[bcount],len) == -1){DPRINT(DEB_ERR, "Could not add String Records."); abort();}
	bcount+=len;
	count++;
      }
      ((hdr_bdldict*) curhdr_p)->record = rec;
      curhdr = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      prev_nextp = &(((hdr_bdldict*) curhdr_p)->next_hdr_p);
      break;
    case BDL_PYLD:
      curhdr_p = (void*) new hdr_bdlpyld;
      if(!curhdr_p){DPRINT(DEB_ERR, "Could not create header."); abort();}
      *prev_nextp = curhdr_p;

      if(bcount+5 > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdlpyld*) curhdr_p) -> pldclass = buf[bcount++];
      memcpy(&((hdr_bdlpyld*) curhdr_p)->len, &buf[bcount], sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);
      
      if(bcount+((hdr_bdlpyld*) curhdr_p)->len > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdlpyld*) curhdr_p)->payload = new char[((hdr_bdlpyld*) curhdr_p)->len];
      if(!((hdr_bdlpyld*) curhdr_p)->payload){DPRINT(DEB_ERR, "Could not allocate memory."); abort();}
      memcpy(((hdr_bdlpyld*) curhdr_p)->payload, &buf[bcount], ((hdr_bdlpyld*) curhdr_p)->len);
      bcount+=((hdr_bdlpyld*) curhdr_p)->len;

      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    case BDL_AUTH:
      curhdr_p = (void*) new hdr_bdlauth;
      if(!curhdr_p){DPRINT(DEB_ERR, "Could not create header."); abort();}
      *prev_nextp = curhdr_p;
      
      if(bcount+2 > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdlauth*) curhdr_p) -> next_hdr = buf[bcount++];
      ((hdr_bdlauth*) curhdr_p) -> len = buf[bcount++];
      
      if(bcount+((hdr_bdlauth*) curhdr_p)->len > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      memcpy(((hdr_bdlauth*) curhdr_p)->info, &buf[bcount], ((hdr_bdlauth*) curhdr_p)->len);
      bcount+=((hdr_bdlauth*) curhdr_p)->len;

      curhdr = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      prev_nextp = &(((hdr_bdlauth*) curhdr_p)->next_hdr_p);
      break;
    case BDL_PSEC:
      curhdr_p = (void*) new hdr_bdlpsec;
      if(!curhdr_p){DPRINT(DEB_ERR, "Could not create header."); abort();}
      *prev_nextp = curhdr_p;
      
      if(bcount+2 > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdlpsec*) curhdr_p) -> next_hdr = buf[bcount++];
      ((hdr_bdlpsec*) curhdr_p) -> len = buf[bcount++];

      if(bcount+((hdr_bdlpsec*) curhdr_p)->len > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      memcpy(((hdr_bdlpsec*) curhdr_p)->info, &buf[bcount], ((hdr_bdlpsec*) curhdr_p)->len);
      bcount+=((hdr_bdlpsec*) curhdr_p)->len;

      curhdr = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      prev_nextp = &(((hdr_bdlpsec*) curhdr_p)->next_hdr_p);
      break;
    case BDL_FRAG:
      curhdr_p = (void*) new hdr_bdlfrag;
      if(!curhdr_p){DPRINT(DEB_ERR, "Could not create header."); abort();}
      *prev_nextp = curhdr_p;
      
      if(bcount+1+2*sizeof(u_int32_t) > buflen) {DPRINT(DEB_WARN, "buf is too short."); return -1;}
      ((hdr_bdlfrag*) curhdr_p) -> next_hdr = buf[bcount++];
      memcpy(&((hdr_bdlfrag*) curhdr_p)->tot_len, &buf[bcount], sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);
      memcpy(&((hdr_bdlfrag*) curhdr_p)->offset, &buf[bcount], sizeof(u_int32_t));
      bcount+=sizeof(u_int32_t);

      curhdr = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      prev_nextp = &(((hdr_bdlfrag*) curhdr_p)->next_hdr_p);
      break;
    default:
      DPRINT(DEB_WARN, "Unknown header type.");
      abort();
      break;
    }
  }
  return bcount;
}

/** Stores information for a bundle status report based on status flags. 
 *
 * \param flags Status flags for desired status report.
 *
 * \retval 0 on success.
 */
int Bundle :: generateReport(u_int8_t rflags, u_int8_t cflags, u_int8_t reason)
{
  DPRINT(DEB_DEBUG, "Start in generateReport: report flags %02x, cust flags %02x, reason %02x",rflags,cflags,reason);
  
  if(!report_){
    report_ = new bdl_report;
    if(!report_){DPRINT(DEB_ERR, "Could not allocate memory for status report."); abort();}
    report_->flags = 0;
    report_->type = BDL_PCLASS_REPORT;
    report_->timestamp = bdl_prim_->timestamp;
    StrRec* tmp = bdl_dict_->record->getRecord(((bdl_prim_->src)&0xf0)/0x10);
    report_->reg_len = tmp->len_;
    report_->reg_id = (u_int8_t*)tmp->text_;
    tmp = bdl_dict_->record->getRecord((bdl_prim_->src) & 0x0f);
    report_->adm_len = tmp->len_;
    report_->adm_id = (u_int8_t*)tmp->text_;
    report_->dest = bdl_dict_->record->getRecords(bdl_prim_->rpt_to);
  }

  /* Request Report of Bundle Reception */
  if(rflags & BDL_RPT_RECV && bdl_prim_->cos & BDL_COS_BREC){
    fprintf(stderr,"------> Request Report of Bundle Reception\n");
    report_->flags |= BDL_RPT_RECV;
    report_->tor = getTimestamp();
  }

  /*Custody*/
  if(rflags & BDL_RPT_CUST && bdl_prim_->cos & BDL_COS_CTREP){
    report_->flags |= BDL_RPT_CUST;
    fprintf(stderr, "Preparing for Custody Transfer Reporting\n");
  }

  if(cflags & BDL_CUST_SUCC && bdl_prim_->cos & BDL_COS_CUST){
    fprintf(stderr, "Preparing for Custodial Acknowledgement\n");
    if(!custreport_){
      custreport_ = new bdl_report;
      if(!custreport_){DPRINT(DEB_ERR, "Could not allocate memory for status report."); abort();}
      custreport_->flags = 0;
      custreport_->type = BDL_PCLASS_CUST;
      custreport_->timestamp = bdl_prim_->timestamp;
      custreport_->tos = getTimestamp();
      StrRec* tmp = bdl_dict_->record->getRecord(((bdl_prim_->src)&0xf0)/0x10);
      custreport_->reg_len = tmp->len_;
      custreport_->reg_id = (u_int8_t*)tmp->text_;
      tmp = bdl_dict_->record->getRecord((bdl_prim_->src) & 0x0f);
      custreport_->adm_len = tmp->len_;
      custreport_->adm_id = (u_int8_t*)tmp->text_;
      custreport_->dest = bdl_dict_->record->getRecords(bdl_prim_->cust);
    }
    custreport_->flags |= BDL_CUST_SUCC;
    custreport_->reason = reason;
  }

  /*Bundle forwarding*/
  if(rflags & BDL_RPT_FWD && bdl_prim_->cos & BDL_COS_BFWD){
    report_->flags |= BDL_RPT_FWD;
    report_->tof = getTimestamp();
  }

  /*Bundle delivery*/
  if(rflags & BDL_RPT_DELIV && bdl_prim_->cos & BDL_COS_RET){
    report_->flags |= BDL_RPT_DELIV;
    report_->todeliv = getTimestamp();
  }

  /*Bundle deletion*/
  if(rflags & BDL_RPT_EXP){
    report_->flags |= BDL_RPT_EXP;
    report_->todelet = getTimestamp();
  }
 
  /*Fragmentation*/
  if(report_)     report_->flags &= ~BDL_RPT_FRAG;
  if(custreport_) custreport_->flags &= ~BDL_CUST_FRAG;
  int next=BDL_DICT;
  void* nextp = bdl_dict_;

  while(next != BDL_PYLD){
    next = ((hdr_bdldict*)nextp)->next_hdr;
    nextp = ((hdr_bdldict*)nextp)->next_hdr_p;
    if(next == BDL_FRAG){
      if(report_){
	report_->flags |= BDL_RPT_FRAG;
	report_->frag_off = ((hdr_bdlfrag*) nextp)->offset;
      }
      if(custreport_){
	custreport_->flags |= BDL_CUST_FRAG;
	custreport_->frag_off = ((hdr_bdlfrag*) nextp)->offset;
      }
    }
  }
  if(report_){
    report_->frag_len = ((hdr_bdlpyld*) nextp)->len;
  }
  if(custreport_){
    custreport_->frag_len = ((hdr_bdlpyld*) nextp)->len;
  }
  
  if(report_){
    if(report_->flags == BDL_RPT_FRAG) report_->flags=0;
    DPRINT(DEB_DEBUG, "generateReport set report flags %02x",report_->flags);
  }
  if(custreport_){
    if(custreport_->flags == BDL_CUST_FRAG) custreport_->flags=0;
    DPRINT(DEB_DEBUG, "generateReport set cust flags %02x and reason %02x",custreport_->flags,custreport_->reason);
  }
  return 0;
}


/** Splits a bundle into two fragments.
 *
 * \param mtu MTU of bundles..
 * \param exact Boolean specifying whether the fragment should be of MTU size exactly.


*/
void Bundle :: fragment(size_t mtu, int exact)
{
  size_t headsize = sizeof(hdr_ip) + sizeof(hdr_bdlprim) + getDataSize(1) + 1+2*sizeof(u_int32_t);
  if(headsize>=mtu) {DPRINT(DEB_ERR, "Scenario configuration error, header size is larger than mtu."); abort();}
  size_t pyldsize = getDataSize(2);
  
  size_t chunk = mtu-headsize;
  if(exact) chunk = mtu;
  if(chunk>pyldsize){
    DPRINT(DEB_WARN, "Fragment called, but no need to fragment.");
    return;
  }
  
  if(! exact){
    int chunks = (pyldsize+chunk-1)/chunk;
    if(chunks == 2) chunk = pyldsize/2;
  }
  DPRINT(DEB_DEBUG,"Fragment size: %d.",chunk);
  
  Bundle* fragrest = new Bundle();
  if(! fragrest) {DPRINT(DEB_ERR, "Could not allocate memory for bundle fragment."); abort();}

  fragrest->prevhop_=prevhop_;
  fragrest->transtime_=transtime_;
  fragrest->recvtime_=recvtime_;
  fragrest->fid_=fid_;

  fragrest->bdl_prim_ = bdl_prim_->clone();
  fragrest->bdl_dict_ = bdl_dict_->clone();

  int curhdr = BDL_DICT;
  void* curhdr_p  = (void*) bdl_dict_;
  void* currest   = (void*) fragrest->bdl_dict_;
  void** prevnext = NULL;
  hdr_bdlfrag* fragheader = NULL;
  hdr_bdlfrag* fragheader_rest = NULL;

  int offset=0;
  int plen=0;
  int tot_len=0;

  hdr_bdlpyld* pyld=NULL;
  hdr_bdlpyld* pyldrest = NULL;
  char* olddata = NULL;
  char* data = NULL;
  char* datarest = NULL;
 
  while(curhdr_p){
    switch(curhdr) {
    case BDL_DICT:
      curhdr = ((hdr_bdldict*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdldict*) curhdr_p) -> next_hdr_p;
      ((hdr_bdldict*) currest)->next_hdr = curhdr;
      prevnext = & (((hdr_bdldict*) currest)->next_hdr_p);
      break;
    case BDL_FRAG:
      currest= ((hdr_bdlfrag*) curhdr_p) -> clone();
      *prevnext=currest;      
      fragheader = ((hdr_bdlfrag*) curhdr_p);
      fragheader_rest = ((hdr_bdlfrag*)currest);
      curhdr = ((hdr_bdlfrag*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlfrag*) curhdr_p) -> next_hdr_p;
      ((hdr_bdlfrag*) currest)->next_hdr = curhdr;
      prevnext = & (((hdr_bdlfrag*) currest)->next_hdr_p);
      break;
    case BDL_PYLD:
      pyld=(hdr_bdlpyld*)curhdr_p;
      if(fragheader){
	offset=fragheader->offset;

	plen=pyld->len;
	tot_len=fragheader->tot_len;
      } else {
	fragheader = new hdr_bdlfrag();
	fragheader_rest = new hdr_bdlfrag();
	if(!fragheader || !fragheader_rest ){ DPRINT(DEB_ERR,"Could not allocate memory."); abort(); }

	fragheader->next_hdr = bdl_dict_->next_hdr;
	fragheader->next_hdr_p = bdl_dict_->next_hdr_p;
	fragheader_rest->next_hdr = fragrest->bdl_dict_->next_hdr;
	fragheader_rest->next_hdr_p = fragrest->bdl_dict_->next_hdr_p;

	bdl_dict_->next_hdr = BDL_FRAG;
	bdl_dict_->next_hdr_p = fragheader;
	fragrest->bdl_dict_->next_hdr = BDL_FRAG;
	fragrest->bdl_dict_->next_hdr_p = fragheader_rest;

	if(& fragrest->bdl_dict_->next_hdr_p == prevnext) prevnext=& fragheader_rest->next_hdr_p;

	offset=0;
	plen=pyld->len;
	tot_len=pyld->len;
      }

      fragheader->offset=offset;      
      fragheader->tot_len=tot_len;
      fragheader_rest->offset=offset+chunk;
      fragheader_rest->tot_len=tot_len;

      pyldrest = new hdr_bdlpyld();
      olddata = pyld->payload;
      data = new char[chunk];
      datarest = new char[plen-chunk];
      if(!pyldrest || !data || !datarest){ DPRINT(DEB_ERR,"Could not allocate memory."); abort(); }
      memcpy(data,olddata,chunk);
      memcpy(datarest,olddata,plen-chunk);
      
      delete olddata;
      olddata=NULL;
      pyld->payload=data;
      pyld->len=chunk;
      pyldrest->payload=datarest;
      pyldrest->len=plen-chunk;
      pyldrest->pldclass=pyld->pldclass;
      
      *prevnext=pyldrest;
      
      curhdr = BDL_NONE;
      curhdr_p = NULL;
      break;
    case BDL_AUTH:
      currest= ((hdr_bdlauth*) curhdr_p) -> clone();
      *prevnext=currest;
      curhdr = ((hdr_bdlauth*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlauth*) curhdr_p) -> next_hdr_p;
      ((hdr_bdlauth*) currest)->next_hdr = curhdr;
      prevnext = & (((hdr_bdlauth*) currest)->next_hdr_p);
      break;
    case BDL_PSEC:
      currest= ((hdr_bdlpsec*) curhdr_p) -> clone();
      *prevnext=currest;
      curhdr = ((hdr_bdlpsec*) curhdr_p) -> next_hdr;
      curhdr_p = ((hdr_bdlpsec*) curhdr_p) -> next_hdr_p;
      ((hdr_bdlpsec*) currest)->next_hdr = curhdr;
      prevnext = & (((hdr_bdlpsec*) currest)->next_hdr_p);
      break;
    default:
      DPRINT(DEB_ERR, "Unknown header type.");
      abort();
      break;
    }
  }  

  next_frag_=fragrest;
}

/** Defragments a fragmented bundle.
 *
 * \param frags List of fragments.
 *
 * \returns The defragmented bundle or NULL if fragments are missing.
*/
Bundle* Bundle :: defragment(Bundle** frags)
{
  Bundle* prev = NULL;
  Bundle* curr = (*frags);
  Bundle* currfrag = NULL;

  // Stage 1. Find the other fragments.
  while(curr){
    char* ceid = curr ->bdl_dict_->record->getRecords(curr  ->bdl_prim_->src); // curr   endpoint id.
    char* beid =        bdl_dict_->record->getRecords(bdl_prim_->src);         // bundle endpoint id.
    
    if(strcmp(ceid, beid) == 0 && curr->bdl_prim_->timestamp ==  bdl_prim_->timestamp){
      delete ceid;
      delete beid;
      ceid=beid=NULL;
      break;
    }
    delete ceid;
    delete beid;
    ceid=beid=NULL;
    prev=curr;
    curr=curr->next_;
  }

  if(!curr){
    if(!prev) (*frags)=this;
    else prev->next_=this;
    DPRINT(DEB_DEBUG, "New fragmented bundle recieved.");
    return NULL;
  }
  DPRINT(DEB_DEBUG, "Continuing on fragmented bundle.");
  currfrag=curr;

  // Stage 2. Insert bundle.
  hdr_bdlfrag* curr_fh=NULL;
  hdr_bdlpyld* curr_ph=NULL;
  hdr_bdlfrag* bundle_fh=NULL;
  hdr_bdlpyld* bundle_ph=NULL;
  void* curh=NULL;

  curh=bdl_dict_;
  while(curh){
    if(((hdr_bdldict*)curh)->next_hdr == BDL_FRAG)   bundle_fh=(hdr_bdlfrag*)((hdr_bdldict*)curh)->next_hdr_p;
    if(((hdr_bdldict*)curh)->next_hdr == BDL_PYLD) { bundle_ph=(hdr_bdlpyld*)((hdr_bdldict*)curh)->next_hdr_p; break;}
    curh = ((hdr_bdldict*)curh)->next_hdr_p;
  }
  
  Bundle* prevfrag = NULL;

  while(currfrag){
    curh = (void*) currfrag->bdl_dict_;
    curr_fh = NULL;
    curr_ph = NULL;

    while(curh){
      if(((hdr_bdldict*)curh)->next_hdr == BDL_FRAG)   curr_fh=(hdr_bdlfrag*)((hdr_bdldict*)curh)->next_hdr_p;
      if(((hdr_bdldict*)curh)->next_hdr == BDL_PYLD) { curr_ph=(hdr_bdlpyld*)((hdr_bdldict*)curh)->next_hdr_p; break;}
      curh = ((hdr_bdldict*)curh)->next_hdr_p;
    }
    if(! curr_fh || ! curr_ph) { 
      DPRINT(DEB_ERR, "Defragment used on nonfragmented bundle, or bundle with no payload.");
      abort(); 
    }

    if(curr_fh->offset == bundle_fh->offset && curr_ph->len == bundle_ph->len) {
      DPRINT(DEB_DEBUG, "Recieved same fragment twice.");
      delete this;
      return NULL;
    }
    if(bundle_fh->offset < curr_fh->offset){
      if(currfrag == curr) { // Insert first.
	if(prev) prev->next_=this;
	else (*frags)=this;
	next_=curr->next_;
	curr->next_=NULL;
	next_frag_=curr;
	curr=this;
      } else { // Insert in the middle.
	prevfrag->next_frag_=this;
	next_frag_=currfrag;
      }
      break;
    }
    prevfrag=currfrag;
    currfrag=currfrag->next_frag_;
  }
  if(!currfrag){ // Insert last.
    prevfrag->next_frag_=this;
  }
  
  // Stage 3. Check if we have all fragments.

  int co=0; // current offset
  
  currfrag=curr;
  while(currfrag){
    curh = (void*) currfrag->bdl_dict_;
    curr_fh = NULL;
    curr_ph = NULL;

    while(curh){
      if(((hdr_bdldict*)curh)->next_hdr == BDL_FRAG)   curr_fh=(hdr_bdlfrag*)((hdr_bdldict*)curh)->next_hdr_p;
      if(((hdr_bdldict*)curh)->next_hdr == BDL_PYLD) { curr_ph=(hdr_bdlpyld*)((hdr_bdldict*)curh)->next_hdr_p; break;}
      curh = ((hdr_bdldict*)curh)->next_hdr_p;
    }
    if(! curr_fh || ! curr_ph) {
      DPRINT(DEB_ERR, "Defragment used on nonfragmented bundle, or bundle with no payload."); 
      abort(); 
    }
    
    if(co  < curr_fh->offset) return NULL; // Still missing pieces.
    if(co == curr_fh->offset)  co += curr_ph->len;
    else if(curr_fh->offset + curr_ph->len > co) co = curr_fh->offset + curr_ph->len ;

    currfrag=currfrag->next_frag_;
  }
  DPRINT(DEB_DEBUG, "All fragments are adjacent.");
  if(co != curr_fh->tot_len) return NULL;
  
  // Stage 4. Put it together in one bundle.
  DPRINT(DEB_DEBUG, "All fragments present, rebuildning.");
  
  // Create new payload.
  int totlen = curr_fh->tot_len;
  char* data = new char[totlen];
  if(!data) {DPRINT(DEB_ERR,"Could not allocate memory."); abort(); }
    
  currfrag=curr;
  while(currfrag){
    curh = (void*) currfrag->bdl_dict_;
    curr_fh = NULL;
    curr_ph = NULL;

    while(curh){
      if(((hdr_bdldict*)curh)->next_hdr == BDL_FRAG)   curr_fh=(hdr_bdlfrag*)((hdr_bdldict*)curh)->next_hdr_p;
      if(((hdr_bdldict*)curh)->next_hdr == BDL_PYLD) { curr_ph=(hdr_bdlpyld*)((hdr_bdldict*)curh)->next_hdr_p; break;}
      curh = ((hdr_bdldict*)curh)->next_hdr_p;
    }
    if(! curr_fh || ! curr_ph) {
      DPRINT(DEB_ERR, "Defragment used on nonfragmented bundle, or bundle with no payload."); 
      abort();
    }
    
    memcpy(&data[curr_fh->offset],curr_ph->payload,curr_ph->len);

    currfrag=currfrag->next_frag_;
  }
  
  // Remove fragmentation header and update payloadheader.
  curh = (void*) curr->bdl_dict_;
  curr_fh = NULL;
  curr_ph = NULL;
  
  while(curh){
    if(((hdr_bdldict*)curh)->next_hdr == BDL_PYLD) { curr_ph=(hdr_bdlpyld*)((hdr_bdldict*)curh)->next_hdr_p; break;}
    
    if(((hdr_bdldict*)curh)->next_hdr == BDL_FRAG) {
	curr_fh=(hdr_bdlfrag*)((hdr_bdldict*)curh)->next_hdr_p;
	((hdr_bdldict*)curh)->next_hdr_p = curr_fh->next_hdr_p;
	((hdr_bdldict*)curh)->next_hdr   = curr_fh->next_hdr;
	delete curr_fh;
	curr_fh = NULL;
    } else curh = ((hdr_bdldict*)curh)->next_hdr_p;
  }
  
  if(! curr_ph) { DPRINT(DEB_ERR, "Defragment used on bundle with no payload."); abort(); }
  
  curr_ph->len = totlen;
  delete curr_ph->payload;
  curr_ph->payload=data;
  
  Bundle* tobd = NULL;
  currfrag = curr->next_frag_;
  while(currfrag){
    tobd=currfrag;
    currfrag=currfrag->next_frag_;
    tobd->next_frag_=NULL;
    delete tobd;
    tobd=NULL;
  }
  
  if(prev) prev->next_=curr->next_;
  else (*frags)=curr->next_;
  curr->next_=NULL;
  return curr;
}
