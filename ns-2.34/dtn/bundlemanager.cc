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

/** \file bundlemanager.cc  Bundle management. */

#include "common/scheduler.h"
#include "common/node.h"
#include <stdlib.h>
#include <stdio.h>
#include "bundlemanager.h"
#include "bmflags.h"
#include "dtn.h"
#include "headers.h"
#include "debug.h"
#include "timers.h"
#include "bundle.h"
#include "strrecords.h"
#include "common/node.h"
#include "epidemic-routing.h"

#include <time.h>
#include "neighbors.h"


/** Gets current time.
 *
 * \returns Current time in 64-bit fixed-point format.
*/

inline u_int64_t getTimestamp() 
{
  return (u_int64_t)((Scheduler::instance().clock())*0x100000000LL);
}

/** Allocates a free variablename.
 *
 * \param value The value to be set.
 * \param name  Name of the tcl object that is to get this variable.
 *
 * \returns The allocated variablename.
 *
 * \note The returned string must be deleted.
 * 
 */
char* allocRetVar(const char* value, const char* name)
{
  static unsigned int next=0;
  char* tempname=new char[11]; // RVxxxxxxxx
  if(!tempname){
    abort();
  }
  snprintf(tempname, 11, "RV%08x", next++);
  tempname[10]='\0';

  char* out = new char[strlen(name) + strlen(tempname) + strlen(value) + strlen("x set x \"x\"")];
  if(! out){
    abort();
  }
  sprintf(out, "%s set %s \"%s\"",name , tempname,value);
  Tcl& tcl = Tcl::instance();
  tcl.eval(out);
  return tempname;  
}


/** Bundle Manager.
 *
 * \param routes  Routing list for current node.
 * \param reg     Registration handler.
 * \param da      DTNAgent for current node. 
*/
BundleManager :: BundleManager(Routes* routes, Registration* reg, DTNAgent* da) : lastTimestamp_(0),sent_(NULL),local_(NULL),frags_(NULL),agentEndpoint_(NULL),bids_(NULL)
{
	srand(time(NULL));  
	routes_ = routes;
  	reg_ = reg;
  	da_ = da;
  	qt_ = new QueueTimer (this);
  	rt_ = new ResendTimer(this);
  	ht_ = new CollectTimer(this);
  	er_ = new EpidemicRouting(this);
  	nm_=new DtnNeighborsManager(this);
  	nm_->set_routes(routes_);
  	b_id=0;
  	da_->epidemic_buffered_bundles=0;
  

  	ht_->sched(0.5*da_->hello_interval+2*(0.5)*da_->hello_interval*Random::uniform());
  	da_->number_of_asked_bundles=0;
  	da_->number_recv_bundles=0;

 	if(! da_ || ! qt_ || ! rt_||!ht_ || !er_ || !nm_ ){
    		fprintf(stdout,"No dtnagent(%08x) given or could not allocate qt(%08x) or rt(%08x) or ht(%08x) or er(%08x) or nm(%08x).",da_,qt_,rt_,ht_,er_,nm_);
	    	abort();
  	}
	
}

BundleManager::~BundleManager()
{
delete qt_;
delete rt_;
delete ht_;
delete er_;
delete nm_;
}

/**  Send token indication callback format. */
#define SENDTOKEN_IND_FMT "%s indSendToken %s %d"


/** Log trace data for a bundle.
 *  A classless function for logging.
 *
 * \param bundle A bundle to retreive information from.
 * \param da The nodes DTNAgent.
 * \param event Which event occured.
 * \param fnode The node the bundle came from, if relevant.
 */
void tprint(Bundle* bundle, DTNAgent* da, char* event, int fnode=-1)
{
  char* src = bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->src);
  char* dest = bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->dest);
  char* rpt_to = bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->rpt_to);
  int len = 0;
  int offset = 0;
  int fraglen = 0;
  
  hdr_bdldict* cur = bundle->bdl_dict_;
  hdr_bdlfrag* frag = NULL;
  hdr_bdlpyld* pyld = NULL;

  while(cur){
    if     (cur->next_hdr == BDL_FRAG)  frag = (hdr_bdlfrag*)((hdr_bdldict*)cur)->next_hdr_p;
    else if(cur->next_hdr == BDL_PYLD){ pyld = (hdr_bdlpyld*)((hdr_bdldict*)cur)->next_hdr_p; break;}
    cur = ((hdr_bdldict*)cur->next_hdr_p);
  }
  if(pyld && frag){
    offset = frag->offset;
    fraglen = pyld->len;
    len = frag->tot_len;
  }else if(pyld) len = pyld->len;
  
  if(fnode == -1){
    if(tracefile) fprintf(tracefile, "%f %d %s %s %016llx %d %02x %s %s %d %d %d %d\n",
			  Scheduler::instance().clock(),
			  da->addr(),
			  event,
			  src,
			  bundle->bdl_prim_->timestamp,
			  ((bundle->bdl_prim_->cos)&BDL_COS_PRIMASK)/BDL_COS_PRISHIFT,
			  (bundle->bdl_prim_->cos)&~BDL_COS_PRIMASK,
			  dest,
			  rpt_to,
			  bundle->token_,
			  offset,
			  fraglen,
			  len);
  }else{
    if(tracefile) fprintf(tracefile, "%f %d %s %s %016llx %d %02x %s %s %d %d %d %d %d\n",
			  Scheduler::instance().clock(),
			  da->addr(),
			  event,
			  src,
			  bundle->bdl_prim_->timestamp,
			  ((bundle->bdl_prim_->cos)&BDL_COS_PRIMASK)/BDL_COS_PRISHIFT,
			  (bundle->bdl_prim_->cos)&~BDL_COS_PRIMASK,
			  dest,
			  rpt_to,
			  bundle->token_,
			  offset,
			  fraglen,
			  len,
			  fnode);
  }
  
  delete src;
  delete dest;
  delete rpt_to;
}

/** Creates a new Bundle.
 *
 * \param src       Source endpoint id.
 * \param dest      Destination endpoint id.
 * \param rpt_to    Report to endpoint id.
 * \param cos       Requested priority.
 * \param options   Delivery options.
 * \param lifespan  Lifespan of the bundle in seconds.
 * \param binding   Send token binding.
 * \param data      Payload.
 * \param datasize  Minimum payload size.
 *
 * \returns Status code indicating outcome.
 * \retval  0 on success.
 * \retval -1 on failure.
*/

int BundleManager :: newBundle(const char* src, 
			       const char* dest, 
			       const char* rpt_to, 
			       const char* cos, 
			       const char* options, 
			       const char* lifespan, 
			       const char* binding, 
			       const char* data, 
			       const char* datasize,
			       int conversation )
{

  Tcl& tcl = Tcl::instance();
  hdr_bdlprim* bdl_prim = new hdr_bdlprim;
  hdr_bdldict* bdl_dict = new hdr_bdldict;
  hdr_bdlpyld* bdl_pyld = new hdr_bdlpyld;

  DTNStrRecords* records = new DTNStrRecords();
  Bundle* temp = new Bundle();
  if(!bdl_prim || !bdl_dict || !bdl_pyld || !records || !temp){fprintf(stdout, "Could not allocate memory."); abort();}
  
  temp->fid_ = BDL_FID_NORM;

  bdl_prim->version = BDL_VERSION;
  bdl_prim->next_hdr = BDL_DICT;
  bdl_prim->cos = BDL_COS_PRIMASK;
  bdl_prim->pld_sec = 0x00;
  parseOptions(cos,options,bdl_prim);
 
  int tmpval = 0;
  if((tmpval = records->addRecord(dest)) == -1){fprintf(stdout, "Could not add String RecordaddRecord(dest)."); abort();}
  bdl_prim->dest = tmpval;
  
  if((tmpval = records->addRecord(src)) == -1){fprintf(stdout, "Could not add String RecordaddRecord(src)."); abort();}
  bdl_prim->src = tmpval;
  if((tmpval = records->addRecord(rpt_to)) == -1){fprintf(stdout, "Could not add String Record.ddRecord(rpt_to)"); abort();}
  bdl_prim->rpt_to = tmpval;
 
  if(bdl_prim->cos & BDL_COS_CUST){
    if((tmpval = records->addRecord(agentEndpoint())) == -1){fprintf(stdout, "Could not add String Record."); abort();}
    bdl_prim->cust = tmpval;
    temp->fid_ = BDL_FID_CUST;
    fprintf(stdout,"------> Custody transfer of the Bundle required !\n");
  }

  else bdl_prim->cust = 0x00;

  bdl_prim->timestamp = getTimestamp();
  if(bdl_prim->timestamp <= lastTimestamp_){
    bdl_prim->timestamp = ++lastTimestamp_;
  } else {
    lastTimestamp_ = bdl_prim->timestamp;
  }

  
  if(atoi(lifespan)==0)
    bdl_prim->exp_time = (u_int32_t)atoi(da_->getlifespan());
else  bdl_prim->exp_time =(u_int32_t) atoi(lifespan);
 
  bdl_prim->elapsed_time=0;
  bdl_prim->hopcount=0;

  if(conversation==1||conversation==2||conversation==3||conversation==-1||conversation==4||conversation==5||conversation==6||conversation==7||conversation==8||conversation==9)
	bdl_prim->conversation=conversation;
  else bdl_prim->conversation=0;
 
  bdl_prim->source_rule=da_->rule;
  if(bdl_prim->conversation==0)
  {
  	b_id++;
  	if(b_id==da_->max_ids_count) b_id=1;
  	bdl_prim->u_id =b_id ; 
  }else bdl_prim->u_id =0;

  bdl_dict->next_hdr = BDL_PYLD;
  bdl_dict->next_hdr_p = bdl_pyld;
  bdl_dict->strcount = records->strcount();
  bdl_dict->record = records;
  

  bdl_pyld->pldclass = BDL_PCLASS_NORMAL;
  bdl_pyld->len = atoi(datasize);
  
  temp->prevhop_  = da_->addr();
  temp->recvtime_ = Scheduler::instance().clock();
  
  
 
  
  temp->bdl_prim_ = bdl_prim;
 (temp->bdl_prim_)->me=da_->addr();
  strcpy((temp->bdl_prim_)->region, routes_->region());
temp->bdl_dict_ = bdl_dict;
  const char* rdata=NULL;
  
  if(data && bdl_pyld->len < strlen(data)) bdl_pyld->len=strlen(data);
   

  bdl_pyld->payload = new char[bdl_pyld->len];
   if(!bdl_pyld->payload){fprintf(stdout, "Could not allocate memory for payload."); abort();}
  bzero(bdl_pyld->payload, bdl_pyld->len);
  

	if(data) 
	{
		memcpy(bdl_pyld->payload,data,strlen(data));
	}
 
	
  int token = temp->token_;
  if(enqueue(temp)) {  
   return -1;}

/*
	switch(bdl_prim->conversation)
	{
		case 0: 
			da_->data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 1:	
			da_->epidimic_control_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 2:
			da_->epidimic_control_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 3:
			da_->epidimic_control_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 4:
			da_->epidimic_control_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 5:
			da_->epidimic_control_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 6:
			da_->stat_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		case 7:
			da_->stat_data_size += (sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) + sizeof(hdr_bdlpyld));
			break;
		default: break;
	};
 */
  if(token != -1){
	// TODO:
	if (binding != NULL)
	{    
		if(strcmp(binding,"") != 0)
		{
      			int len = strlen(SENDTOKEN_IND_FMT) + strlen(da_->name()) + strlen(binding) + 11 + 1;
      			char out [len];
			if(! out){abort();}
      			snprintf(out, len, SENDTOKEN_IND_FMT, da_->name(), binding, token);
      			tcl.eval(out);
    		}
    		return 0;
	}
  }
  return -1;
}

/** Creates a new Bundle from an incoming packet.
 * 
 * It retrieves the payload of the packet and places
 * it in a new bundle, which is either delivered locally
 * or reenqueued for later transmission. 
 *
 * \param pkt An incoming Packet.
 *
 * \retval 0 on success.
 * \retval -1 on failure. 
 *
 */
int BundleManager :: newBundle(Packet* pkt)
{
 
  if(!pkt){fprintf(stdout, "Packet is null."); return -1;}
  PacketData* body = (PacketData*) pkt->userdata();

  hdr_bdlprim* bdl_prim = new hdr_bdlprim;
  hdr_bdldict* bdl_dict = new hdr_bdldict;
  if(!bdl_prim || !bdl_dict){fprintf(stderr, "Could not create header bdl_prim (%08x) or bdl_dict (%08x).",bdl_prim,bdl_dict); abort();}

  hdr_ip* iph = HDR_IP(pkt);
  hdr_bdlprim* ph = hdr_bdlprim::access(pkt);
  if(!bdl_prim){fprintf(stderr, "Could not access packet."); return -1;}

  bdl_prim->version = ph->version;
  bdl_prim->next_hdr = ph->next_hdr;
  bdl_prim->cos = ph->cos;
  bdl_prim->pld_sec = ph->pld_sec;
  bdl_prim->dest = ph->dest;
  bdl_prim->src = ph->src;
  bdl_prim->rpt_to = ph->rpt_to;
  bdl_prim->cust = ph->cust;
  bdl_prim->timestamp =  ph->timestamp;
  bdl_prim->exp_time = ph->exp_time;
  bdl_prim->u_id=ph->u_id;
  bdl_prim->me=ph->me;
  bdl_prim->send_type=ph->send_type;
  strcpy(bdl_prim->region,ph->region);
  bdl_prim->elapsed_time=ph->elapsed_time;
  bdl_prim->hopcount=ph->hopcount;
  bdl_prim->conversation=ph->conversation;
  bdl_prim->source_rule=ph->source_rule;
  Bundle* temp = new Bundle();
  if(!temp){fprintf(stderr, "Could not create new bundle."); abort();}
  temp->prevhop_  = iph->saddr();
  temp->recvtime_ = Scheduler::instance().clock();
  temp->fid_      = iph->fid_;
  temp->bdl_prim_ = bdl_prim;
  temp->bdl_dict_ = bdl_dict;
  int size = temp->retrieveData(body->data(),body->size());
  if(size == -1 || size != body->size()){fprintf(stderr, "Could not get data from packet."); delete temp; return -1;}
  
  temp->generateReport(BDL_RPT_RECV|BDL_RPT_FRAG);

  // Custody.
  if((bdl_prim->cos & BDL_COS_CUST) && da_->custodian()){
    nsaddr_t prevcust = -1;

    char* tcust = NULL;
    StrRec* crec = bdl_dict->record->getRecord(bdl_prim->cust&0x0f);
    if(crec) tcust = crec->text_;
    if(tcust){
      char* end = strchr(tcust, ':');
      char* temp2 = new char[end-tcust+1];
      if(!temp2){fprintf(stderr, "Could not allocate memory."); abort();}
      strncpy(temp2,tcust, end-tcust);
      temp2[end-tcust]='\0';
      Node* cnode = (Node*) TclObject::lookup(temp2);
      delete temp2;
      if(!cnode){fprintf(stderr,"Could not lookup node for previous custodian."); delete temp; return -1;}
      prevcust = cnode->address();
    }

    // Rewrite dict header...
    
   
    temp->generateReport(BDL_RPT_CUST,BDL_CUST_SUCC,BDL_CUST_ACCEPT);
    DTNStrRecords* records = new DTNStrRecords();
    if(!records){fprintf(stderr,  "Could not create string records."); abort();}

    int tmpval = 0;
    if((tmpval   = records->addRecord(bdl_dict->record->getRecords(bdl_prim->dest  ))) == -1)
      {fprintf(stderr,  "Could not add String Record bdl_prim->dest .\n"); abort();}
    bdl_prim->dest = tmpval;
    if((tmpval    = records->addRecord(bdl_dict->record->getRecords(bdl_prim->src   ))) == -1)
      {fprintf(stderr, "Could not add String Record bdl_prim->src.\n"); abort();}
    bdl_prim->src = tmpval;
    if((tmpval = records->addRecord(bdl_dict->record->getRecords(bdl_prim->rpt_to))) == -1)
      {fprintf(stderr, "Could not add String Record.bdl_prim->rpt_to\n"); abort();}
    bdl_prim->rpt_to = tmpval;
    if((tmpval   = records->addRecord(agentEndpoint())) == -1)
      {fprintf(stderr,  "Could not add String Record.agentEndpoint()\n"); abort();}
    bdl_prim->cust = tmpval;
    
    delete(bdl_dict->record);
    bdl_dict->strcount = records->strcount();
    bdl_dict->record = records;
    fprintf(stdout, "Took custody of bundle.");
    
  }

  // Check whether we should reenqueue or deliver locally
 // Update arrival date of the neighbor 
 //Node* no = Node::get_node_by_address(temp->bdl_prim_->me); 
 //nm_->update_arrival_date((char *)no->name());

  Node* local=Node::get_node_by_address(da_->addr());
  if(! local) {fprintf(stderr,  "No local node?!"); abort();}
  
if(strcmp(bdl_dict->record->getRecord(((bdl_prim->dest)&0xf0)/0x10)->text_,"BROADCAST") == 0)
{
	bh_=temp;
	StrRec* srcr = temp->bdl_dict_->record->getRecord((temp->bdl_prim_->src&0x0f));
  	if(! srcr) {fprintf(stderr, "Could not find src record\n");abort();}
  	char * temp_src_1 =NULL;
  	if(srcr) temp_src_1=srcr->text_;
		return recvHello(temp,temp_src_1);
}


	/*
	Node* prev = Node::get_node_by_address(temp->bdl_prim_->me);
	char id[15];
	strcpy(id,"");
	strcat(id,prev->name());
	strcat(id,"\0");
	char region[8];
	strcpy(region, temp->bdl_prim_->region);	
	nm_->add_neighbor(id, region);
	*/
switch(temp->bdl_prim_->conversation)
{
case 1:{
  	return er_->recv_c1(temp);
	}
case 2:{
  	return er_->recv_c2(temp);
	}
case 3:{
  	return er_->recv_c3(temp);
	}
case 4:{
  	return er_->recv_block_ack(temp);
	}
case 5:{return er_->recv_anti_packet(temp);
	}
case 6:{ return er_->stat_matrix_recived_first(temp);
	}
case 7:{ return er_->stat_matrix_recived_last(temp);
	}

case 8:{ 	return er_->stat_matrix_recived_versions_based_reverse_schema_first(temp);
	}
case 9:{
		return er_->stat_matrix_recived_versions_based_reverse_schema_last(temp);
	}
default:{if(temp->bdl_prim_->conversation!=0)
			fprintf(stdout," Ukhnown conversation bundle recived %i \n",temp->bdl_prim_->conversation); 
		break;
		}
}


if(strcmp(routes_->region(), bdl_dict->record->getRecord(((bdl_prim->dest)&0xf0)/0x10)->text_) == 0)
{
    	char* destination = bdl_dict->record->getRecord((bdl_prim->dest)&0x0f)->text_;
    	if(strncmp(destination,local->name(),strlen(local->name())) == 0 && destination[strlen(local->name())] == ':')
	{
		string bundleUid;
		er_->getBundleIdUid(temp, bundleUid);

		//fprintf(stdout, "conversation: %i bundleUid: %s destination: %s local->name(): %s strlen(local->name()): %i\n",temp->bdl_prim_->conversation, (char*)bundleUid.c_str(), destination,local->name(), strlen(local->name()));
	
		if(bdl_prim->cos & BDL_COS_CUST)
		{
			char* cur_cust = bdl_dict->record->getRecords(bdl_prim->cust);
			if(strcmp(cur_cust,agentEndpoint())) {
			//                                                temp->generateReport(0,BDL_CUST_SUCC,BDL_CUST_DELIV);
			//                                                  fprintf(stdout,"------> generateReport(0,BDL_CUST_SUCC,BDL_CUST_DELIV)\n");
			}
			delete cur_cust;
			cur_cust=NULL;
      		}

       		er_->update_bta_list(temp);
		return localDelivery(temp);
    	}
}

return er_->add_bundle(temp);
}

/** Sends a bundle. It builds a packet from the information
 * stored in a bundle and calls the agent for transmission. 
 * 
 * \param bundle The bundle to send.
 *
 * \retval 0 on success. 
 * \retval -1 on failure. 
 * 
 */
int BundleManager :: sendBundle(Bundle* bundle,LinkInfo* nexthop )
{
 	if(!bundle){return -1;}
  	size_t bundlesize = bundle->getDataSize();
  	size_t pktsize    = sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize;

  	if(nexthop==NULL){fprintf(stdout,"ER:------> No next hop found.\n");return -1;}
  
  	if(pktsize > nexthop->mtu_)
	{
  		fprintf(stdout, "Fragmentation of the Bundle \n");
   		bundle->fragment(nexthop->mtu_);
   		bundlesize = bundle->getDataSize();
  	}
	
	nexthop->setBusy(sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize);

  	size_t realsize=0;

  	Packet* pkt = da_->newpacket(sizeof(hdr_bdlprim)+bundlesize);
  	if(!pkt){fprintf(stdout, "ER:Could not create new packet."); return -1;}
  	pkt->allocdata(bundlesize);
  	PacketData* body = (PacketData*) pkt->userdata();

  	if((realsize=bundle->extractData(body->data(),bundlesize)) != bundlesize){
    	Packet::free(pkt);
    	return -1;
  	}
  	hdr_cmn::access(pkt)->size() = sizeof(hdr_bdlprim)+bundlesize;
  
  	hdr_ip* iph = HDR_IP(pkt);
  	if(!iph){fprintf(stdout, "ER:Could not get IP header."); Packet::free(pkt); return -1;}
  	hdr_bdlprim* ph = hdr_bdlprim::access(pkt);
  	if(!ph){fprintf(stdout, "ER:Could not access primary header."); Packet::free(pkt); return -1;}

  	ph->version   = bundle->bdl_prim_->version;
  	ph->next_hdr  = bundle->bdl_prim_->next_hdr;
  	ph->cos       = bundle->bdl_prim_->cos;
  	ph->pld_sec   = bundle->bdl_prim_->pld_sec;
  	ph->dest      = bundle->bdl_prim_->dest;
  	ph->src       = bundle->bdl_prim_->src;
  	ph->rpt_to    = bundle->bdl_prim_->rpt_to;
  	ph->cust      = bundle->bdl_prim_->cust;
  	ph->timestamp = bundle->bdl_prim_->timestamp;
  	ph->exp_time  = bundle->bdl_prim_->exp_time;
  	ph->me        = bundle->bdl_prim_->me;
  	ph->source_rule=bundle->bdl_prim_->source_rule;
  	strcpy(ph->region,bundle->bdl_prim_->region);
  	ph->elapsed_time	= bundle->bdl_prim_->elapsed_time;
  	ph->hopcount=bundle->bdl_prim_->hopcount; 
  	ph->u_id=bundle->bdl_prim_->u_id;
 	ph->conversation=bundle->bdl_prim_->conversation;
  	ph->send_type=bundle->bdl_prim_->send_type;

	if(strcmp(bundle->bdl_dict_->record->getRecord(((bundle->bdl_prim_->dest)&0xf0)/0x10)->text_,"BROADCAST")!=0)
	{
  		iph->daddr() = nexthop->node_;
 	}else  iph->daddr() = IP_BROADCAST;
 
  	iph->dport() = iph->sport();

  	iph->fid_ = bundle->fid_;

  	if(iph->saddr() == iph->daddr()){
    	fprintf(stdout, "Won't send packets to myself. (Should never been queued.)");
    	Packet::free(pkt);
    	return -1;
  	}

	switch(bundle->bdl_prim_->conversation)
	{
		case 0: 
			da_->data_size +=  bundlesize;
			break;
		case 1:	
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 2:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 3:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 4:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 5:
			da_->epidimic_control_data_size +=  bundlesize;
			break;
		case 6:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 7:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 8:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 9:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		
		default: break;
	};


  	da_->sendPacket(pkt);
  	return 0;
}


int BundleManager :: sendBundle(Bundle* bundle)
{
 	if(!bundle){return -1;}

  	LinkInfo* nexthop;
  	size_t bundlesize = bundle->getDataSize();
  	size_t pktsize    = sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize;

  	if(strcmp(bundle->bdl_dict_->record->getRecord(((bundle->bdl_prim_->dest)&0xf0)/0x10)->text_,"BROADCAST") !=0)
	{


   		nexthop = routes_->getNextHop(bundle->bdl_dict_->record->getRecord(((bundle->bdl_prim_->dest)&0xf0)/0x10)->text_, 
					  bundle->bdl_dict_->record->getRecord(((bundle->bdl_prim_->dest)&0x0f)/0x01)->text_, 
					  bundle->prevhop_, bundle->recvtime_,
					  pktsize,
					  bundle->bdl_prim_->cos & BDL_COS_CUST );
  		if(!nexthop){fprintf(stdout,"ER:No next hop found."); return -1;}
  
  		if(pktsize > nexthop->mtu_){
    		fprintf(stdout, "Fragmentation of the Bundle \n");
   		bundle->fragment(nexthop->mtu_);
    		bundlesize = bundle->getDataSize();
  		}

  		nexthop->setBusy(sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize);
	}

	size_t realsize=0;

  	Packet* pkt = da_->newpacket(sizeof(hdr_bdlprim)+bundlesize);
  	
	if(!pkt){fprintf(stdout, "ER:Could not create new packet."); return -1;}
  	pkt->allocdata(bundlesize);
 	PacketData* body = (PacketData*) pkt->userdata();

  	if((realsize=bundle->extractData(body->data(),bundlesize)) != bundlesize){
    	Packet::free(pkt);
    	return -1;
  	}
  	hdr_cmn::access(pkt)->size() = sizeof(hdr_bdlprim)+bundlesize;
  
  	hdr_ip* iph = HDR_IP(pkt);
  	if(!iph){fprintf(stdout, "ER:Could not get IP header."); Packet::free(pkt); return -1;}
  	hdr_bdlprim* ph = hdr_bdlprim::access(pkt);
  	if(!ph){fprintf(stdout, "ER:Could not access primary header."); Packet::free(pkt); return -1;}

  	ph->version   = bundle->bdl_prim_->version;
  	ph->next_hdr  = bundle->bdl_prim_->next_hdr;
  	ph->cos       = bundle->bdl_prim_->cos;
  	ph->pld_sec   = bundle->bdl_prim_->pld_sec;
  	ph->dest      = bundle->bdl_prim_->dest;
 	ph->src       = bundle->bdl_prim_->src;
  	ph->rpt_to    = bundle->bdl_prim_->rpt_to;
  	ph->cust      = bundle->bdl_prim_->cust;
  	ph->timestamp = bundle->bdl_prim_->timestamp;
  	ph->exp_time  = bundle->bdl_prim_->exp_time;
 	ph->me        = bundle->bdl_prim_->me;
  	strcpy(ph->region,bundle->bdl_prim_->region);
  	ph->conversation=bundle->bdl_prim_->conversation;
  	ph->source_rule=bundle->bdl_prim_->source_rule;
  	ph->send_type=bundle->bdl_prim_->send_type;
  	ph->hopcount   = bundle->bdl_prim_->hopcount;

	if(strcmp(bundle->bdl_dict_->record->getRecord(((bundle->bdl_prim_->dest)&0xf0)/0x10)->text_,"BROADCAST")!=0)
	{
  		iph->daddr() = nexthop->node_;
 	}else  iph->daddr() = IP_BROADCAST;
 
  	iph->dport() = iph->sport();

  	iph->fid_ = bundle->fid_;

  	if(iph->saddr() == iph->daddr()){
    		fprintf(stdout, "ER:Won't seeed packets to myself. (Should never been queued.)");
    		Packet::free(pkt);
    		return -1;
  	}

 //(sizeof(hdr_bdlprim) + sizeof(hdr_bdldict) +
	switch(bundle->bdl_prim_->conversation)
	{
		case 0:
			da_->data_size +=  bundlesize;
			break;
		case 1:	
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 2:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 3:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 4:
			da_->epidimic_control_data_size += bundlesize;
			break;
		case 5:
			da_->epidimic_control_data_size +=  bundlesize;
			break;
		case 6:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 7:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 8:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;
		case 9:
			da_->stat_data_size += bundlesize;
			da_->avg_per_meeting_stat = (double)da_->stat_data_size / (double)er_->number_of_meeting;
			break;

		default: break;
	};
 
 da_->sendPacket(pkt);

  return 0;
}

/** Appends a bundle to the correct queue.
 * 
 * Sends packets immediately if ready to send.
 *
 * \param bundle  The Bundle that should be enqueued.
 *
 * \returns Status code indicating outcome.
 * \retval  0 on success.
 * \retval -1 on failure.
*/
int BundleManager :: enqueue(Bundle* bundle)
{
  if(!bundle || !bundle->bdl_prim_)
    {return -1;}

  char* temp = NULL;
  StrRec* crec = bundle->bdl_dict_->record->getRecord(bundle->bdl_prim_->dest&0x0f);
  
  StrRec* srcr = bundle->bdl_dict_->record->getRecord((bundle->bdl_prim_->src&0x0f));
  if(! srcr) {fprintf(stdout,"Could not find src record\n");abort();}
  char * temp_src_1 =NULL;
  if(srcr) temp_src_1=srcr->text_;  

  char * local_endpoint=agentEndpoint();
  char* end_src=strchr(local_endpoint,'_');
    

  if(! crec) {fprintf(stdout, "Could not find dest record."); abort(); }
  if(crec) temp = crec->text_;
  if(temp){
    char* end = strchr(temp, ':');
    if(! end) {fprintf(stdout, "Not a valid record."); abort(); }
    char* temp2 = new char[end-temp+1];
    if(!temp2){fprintf(stdout, "Could not allocate memory."); abort();}
    strncpy(temp2,temp, end-temp);
    temp2[end-temp]='\0';
    
	if((strcmp(temp2,"BROADCAST")==0) && (strcmp(temp_src_1,end_src) == 0) ) 
	{

		bundle->bdl_prim_->me=da_->addr();
		strcpy(bundle->bdl_prim_->region,routes_->region());
		return sendBundle(bundle);
	}
 
    if(bundle->bdl_prim_->conversation==1||bundle->bdl_prim_->conversation==2||bundle->bdl_prim_->conversation==3||bundle->bdl_prim_->conversation==4||bundle->bdl_prim_->conversation==5||bundle->bdl_prim_->conversation==6||bundle->bdl_prim_->conversation==7||bundle->bdl_prim_->conversation==8||bundle->bdl_prim_->conversation==9)
	{
		return sendBundle(bundle);
	}

    Node* cnode = (Node*) TclObject::lookup(temp2);
    if(! cnode) {fprintf(stdout, "Not a valid node."); abort(); }
    delete temp2;
    if(cnode->address() == da_->addr()) {
// 	return localDelivery(bundle);
}
    
 }

return  er_->add_bundle(bundle);
}


/** Parses options (in string format) and updates the primary header.
 *
 * \param cos       Class of Service.\ The desired priority of the bundle.
 * \param options   Delivery options.
 * \param bdl_prim  The primary header.
 */
void BundleManager :: parseOptions(const char* cos, const char* options, hdr_bdlprim* bdl_prim)
{
  if(!cos || !options || !bdl_prim){return;}
  int optlen=strlen(options);
  int i=0;
  u_int8_t ret1 = 0;
  u_int8_t ret2 = 0;

  while(i < optlen){
    int start = i;
    while(options[i] && options[i] != ',') i++;
    if(!strncmp(&options[start],BDL_OPT_NONE,i-start)){
	ret1 = 0;
      break;
    }
    else if(!strncmp(&options[start],BDL_OPT_CUST,i-start)) ret1 |= BDL_COS_CUST;
    else if(!strncmp(&options[start],BDL_OPT_EERCPT,i-start)) ret1 |= BDL_COS_RET;
    else if(!strncmp(&options[start],BDL_OPT_RCPT,i-start)) ret1 |= BDL_COS_BREC;
    else if(!strncmp(&options[start],BDL_OPT_FWD,i-start)) ret1 |= BDL_COS_BFWD;
    else if(!strncmp(&options[start],BDL_OPT_CTREP,i-start)) ret1 |= BDL_COS_CTREP;
    else if(!strncmp(&options[start],BDL_OPT_PSAUTH,i-start)) fprintf(stdout, "No support for %s.",BDL_OPT_PSAUTH);
    else if(!strncmp(&options[start],BDL_OPT_PSINT,i-start)) fprintf(stdout, "No support for %s.",BDL_OPT_PSINT);
    else if(!strncmp(&options[start],BDL_OPT_PSENC,i-start)) fprintf(stdout, "No support for %s.",BDL_OPT_PSENC);
    else fprintf(stdout,"Unknown option near %s",  &options[start]);
    i++;
  }

  int tcos = atoi(cos);
  if(tcos>0 && tcos<16) ret1 |= (tcos*BDL_COS_PRISHIFT) & BDL_COS_PRIMASK;
  else if(!strncmp(cos,"NORMAL",7)) ret1 |= BDL_PRIO_NORM;
  else if(!strncmp(cos,"BULK",5)) ret1 |= BDL_PRIO_BULK;
  else if(!strncmp(cos,"EXPEDITED",10)) ret1 |= BDL_PRIO_EXP;
  else ret1 |= BDL_COS_PRIMASK;

  bdl_prim->cos = ret1;
  bdl_prim->pld_sec = ret2;

}

/** Return cos and psec fields to delivery options.
 *
 * \param cos   Class of Service field.
 * \param psec  Payload security field.
 *
 * \returns String containing the options.
 *
 * \note The returned string must be deleted.
*/
char* BundleManager :: parseOptions(u_int8_t cos, u_int8_t psec)
{
  int len = 0;
  char* str;
  if(!(cos & ~BDL_COS_PRIMASK)){
    len = strlen(BDL_OPT_NONE);
    str = new char[len+1];
    if(!str){abort();}
    strncpy(str,BDL_OPT_NONE,len);
    str[len] = '\0';
    return str;
  }
  if(cos & BDL_COS_CUST) len += strlen(BDL_OPT_CUST)+1;
  if(cos & BDL_COS_CTREP) len += strlen(BDL_OPT_CTREP)+1;
  if(cos & BDL_COS_BREC) len += strlen(BDL_OPT_RCPT)+1;
  if(cos & BDL_COS_BFWD) len += strlen(BDL_OPT_FWD)+1;
  if(cos & BDL_COS_RET) len += strlen(BDL_OPT_EERCPT)+1;
  if(psec & BDL_PSEC_ENC) len += strlen(BDL_OPT_PSENC)+1;
  if(psec & BDL_PSEC_AUTH) len += strlen(BDL_OPT_PSAUTH)+1;
  if(psec & BDL_PSEC_INT) len += strlen(BDL_OPT_PSINT)+1;

  str = new char[len];
  if(!str){abort();}
  int i = 0;
  
  if(cos & BDL_COS_CUST){
    strncpy(&str[i],BDL_OPT_CUST,strlen(BDL_OPT_CUST));
    i+=strlen(BDL_OPT_CUST);
    str[i++] = ',';
  }
  if(cos & BDL_COS_CTREP){
    strncpy(&str[i],BDL_OPT_CTREP,strlen(BDL_OPT_CTREP));
    i+=strlen(BDL_OPT_CTREP);
    str[i++] = ',';
  }
  if(cos & BDL_COS_BREC){
    strncpy(&str[i],BDL_OPT_RCPT,strlen(BDL_OPT_RCPT));
    i+=strlen(BDL_OPT_RCPT);
    str[i++] = ',';
  }
  if(cos & BDL_COS_BFWD){
    strncpy(&str[i],BDL_OPT_FWD,strlen(BDL_OPT_FWD));
    i+=strlen(BDL_OPT_FWD);
    str[i++] = ',';
  }
  if(cos & BDL_COS_RET){
    strncpy(&str[i],BDL_OPT_EERCPT,strlen(BDL_OPT_EERCPT));
    i+=strlen(BDL_OPT_EERCPT);
    str[i++] = ',';
  }
  if(psec & BDL_PSEC_ENC){
    strncpy(&str[i],BDL_OPT_PSENC,strlen(BDL_OPT_PSENC));
    i+=strlen(BDL_OPT_PSENC);
    str[i++] = ',';
  }
  if(psec & BDL_PSEC_AUTH){
    strncpy(&str[i],BDL_OPT_PSAUTH,strlen(BDL_OPT_PSAUTH));
    i+=strlen(BDL_OPT_PSAUTH);
    str[i++] = ',';
  }
  if(psec & BDL_PSEC_INT){
    strncpy(&str[i],BDL_OPT_PSINT,strlen(BDL_OPT_PSINT));
    i+=strlen(BDL_OPT_PSINT);
    str[i++] = ',';
  }
  str[i-1] = '\0';
  return str;
}



/** look if a bundle is already delivered
*/
int BundleManager::is_delivered(char *s,int u_id)
{
BundleId* bid = bids_;
while(bid)
  { if((u_id == bid->u_id_) && (strcmp(s,bid->source_)==0)) return 1;
    bid=bid->next_;
  }
return 0;
}


/**  Payload indication callback format. */
#define  DATA_IND_FMT "%s indData %s %s %s %d %s %d %s"

/** Tries to deliver a bundle locally. 
 *
 * If the bundle is bound for the local agent \link agentDelivery(Bundle*) agentDelivery \endlink is called instead.
 * 
 * \param bundle The Bundle to be locally delivered. It will be deleted when this function returns if all went well.
 * \param force  Set to 1 to force delivery to a node with passive bundle reception disabled. 
 *
 * \returns Statuscode indicating outcome of delivery.
 * \retval  0 on success.
 * \retval -1 on failure.
 */
int BundleManager :: localDelivery(Bundle* bundle, int force, int local)
{
	if(! bundle) 
	{
     		fprintf(stdout, "LD: Bundle is null.");
   		return -1;
  	}
	
	string bundleUid;
	er_->getBundleIdUid(bundle, bundleUid);

  	// Locate fragmentation header.
  	hdr_bdlfrag* frag = (hdr_bdlfrag*) bundle->bdl_dict_;
  	while(frag && frag->next_hdr != BDL_FRAG && frag->next_hdr != BDL_PYLD) frag = (hdr_bdlfrag*) frag->next_hdr_p;
  
  	if(frag && frag->next_hdr == BDL_FRAG){
	//     sendReport(bundle);
	bundle = bundle->defragment(&frags_);
    	if(!bundle) return 0;
  	}

  	// Expire waiting fragments.
  	u_int64_t now = getTimestamp();
  	Bundle* cur=frags_;
  	Bundle* prev=NULL;
  	Bundle* curfrag=NULL;
  	Bundle* tobd=NULL;
  	while(cur)
	{
    		if(cur->bdl_prim_->timestamp+cur->bdl_prim_->exp_time*0x100000000LL < now){
      		fprintf(stderr, "LD: Expiring waiting fragments.");
      		curfrag=cur;
      		if(prev) prev->next_=cur->next_;
      		else frags_=cur->next_;
      		cur=cur->next_;
      		while(curfrag)
    		{
			tobd=curfrag;
			curfrag=curfrag->next_frag_;
			tobd->generateReport(BDL_RPT_EXP);
			// 	sendReport(tobd);
			delete tobd;
			tobd=NULL;
      		}
    		}
		else cur=cur->next_;
  	}
  
  
  	char* d=bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->dest  );
  	char* s=bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->src   );
  	char* r=bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->rpt_to);
  	char* opts=parseOptions(bundle->bdl_prim_->cos,bundle->bdl_prim_->pld_sec);

  	if(! (d && s && r && opts)){
    	fprintf(stderr, "LD: Could not allocate memory for or lookup d(%08x), s(%08x), r(%08x) or opts(%08x).",d,s,r,opts);
    	abort();
  	}
  	RegEntry* regentry = reg_->lookup(d);
 	if(! regentry){
    		delete bundle;
    		delete d;
    		delete s;
    		delete r;
    		delete opts;
    		return 0;
  	} 

  	BundleId* bid = bids_;
  	BundleId* prevbid = NULL;
 
 	while(bid)
	{

      		bid->lifespan_+= TIME_IS-bid->timestamp_;
      		bid->timestamp_=TIME_IS;

      		if(bid->lifespan_ > bid->exptime_)
		{
      			if(prevbid) prevbid->next_ = bid->next_;
     	 			else bids_=bid->next_;
     	 		BundleId* delbid = bid;
			bid=bid->next_;
      			delbid->next_ = NULL;
      			delete delbid;
      			delbid=NULL;
    		} else 
		{
      			if((bundle->bdl_prim_->u_id == bid->u_id_) && (strcmp(s,bid->source_)==0)) 	break;
     	 		prevbid = bid;
      			bid=bid->next_;
    		}
  	}

  	if(! local){
    	if(bid){
	//       sendReport(bundle);
      	delete bundle;
     	 delete d;
      	delete s;
     	 delete r;
      	delete opts;
      	return 0;
    	} else {
	
	if(bundle->bdl_prim_->send_type==0)
         	da_->number_recv_bundles++;

      	bid = new BundleId();
    	if(!bid) 
	{
    	  		fprintf(stderr, "LD: Could not allocate memory."); 
      			abort(); 
      	}
      	bid->source_ = bundle->bdl_dict_->record->getRecords(bundle->bdl_prim_->src);
      	bid->lifespan_=0;
     	bid->timestamp_=TIME_IS;
      	bid->exptime_ = bundle->bdl_prim_->exp_time * 100;
      	bid->u_id_=bundle->bdl_prim_->u_id;
      	bid->next_=NULL;
	if(prevbid) prevbid->next_=bid;
      		else bids_=bid;
     
      	//Logging new Bundles local delivery 

	int new_delivered_uid=er_->get_u_id((char*)bundleUid.c_str());

	if(bundle->bdl_prim_->u_id >= da_->start_from_uid && bundle->bdl_prim_->u_id <= da_->stop_at_uid)
	{
		da_->total_delay+=bundle->bdl_prim_->elapsed_time;
		da_->delivered_bundles++;
		//fprintf(stdout, "New Bundle delivered\n");
	}
		 			
 	 // Updating the antipacket mechanism
	 if(da_->anti_packet_mechanism == 1)
		er_->dbl_->add_delivred_bundle(bundleUid);
  
    }
  }
  
  if(! regentry->passive_ && !force){

    if(regentry->failAct_ == FAIL_DEFER){
   	  			// add_to_delivery_queue;
      			if(bundle->next_){fprintf(stdout,"LD: Bundle is not unlinked."); return -1;}
      			Bundle* cur = local_;
      			while(cur && cur->next_) cur = cur->next_;
      			if(! cur) local_=bundle;
      					else cur->next_=bundle;
    			} else if(regentry->failAct_ == FAIL_SINK){
      delete bundle;
      bundle=NULL;
    } else {
      fprintf(stdout, "LD: Bundle dropped due to selected failure action.");
      delete bundle;
      bundle=NULL;
    }
    delete d;
    delete s;
    delete r;
    delete opts;
    return 0;
  } else if(regentry->passive_ && force){
    fprintf(stdout,"LD :Poll request while in passive reception mode.");
  }

    
  void* nextp=bundle->bdl_dict_;
  int next=BDL_DICT;

  while(nextp && next != BDL_PYLD){
    next =((hdr_bdldict*)nextp)->next_hdr;
    nextp=((hdr_bdldict*)nextp)->next_hdr_p;
  }
  if(! nextp){
   fprintf(stderr,"LD: nextp is NULL."); 
    delete d;
    delete s;
    delete r;
    delete opts;
    return  -1;
  }
  char* temp = new char[((hdr_bdlpyld*)nextp)->len+1];
  if(! temp){fprintf(stdout, "LD: Could not allocate memory for temp.");abort();}
  memcpy(temp, ((hdr_bdlpyld*)nextp)->payload, ((hdr_bdlpyld*)nextp)->len);
  temp[((hdr_bdlpyld*)nextp)->len]='\0';
  char* tempvar = allocRetVar(temp,da_->name());
  delete(temp);

  int len =
    strlen(DATA_IND_FMT) + 
    strlen(da_->name()) + 
    strlen(s) +
    strlen(d) +
    strlen(r) +
    strlen(opts) +
    strlen(tempvar) +
    2*10 +
    1
    ;
  
  char* out = new char[len];
  if(! out){fprintf(stderr, "LD: Could not allocate memory for out.");abort();}
  snprintf(out, len, DATA_IND_FMT, da_->name(), s,d,r, ((bundle->bdl_prim_->cos)&BDL_COS_PRIMASK)/BDL_COS_PRISHIFT, opts, 127, tempvar);

  Tcl& tcl = Tcl::instance();
  tcl.eval(out);

  delete d;
  delete s;
  delete r;
  delete opts;
  delete tempvar;
  delete out;
  delete bundle;
  return 0;
}

/** Tries to deliver a bundle to the local agent.
 *
 * \param bundle The received Bundle.
 *
 * \returns Status code indicating outcome.
 * \retval  0 on success.
 * \retval -1 on failure.
 */
int BundleManager :: agentDelivery(Bundle* bundle)
{

//fprintf(stdout,"Agent Delivery ------------------------------------------------\n");
  if(!bundle){return -1;}
  hdr_bdlpyld* pyld = (hdr_bdlpyld*)bundle->bdl_dict_;
  
  int next=BDL_DICT;
  while(pyld && next != BDL_PYLD){
    next = ((hdr_bdldict*)pyld)->next_hdr;
    pyld = (hdr_bdlpyld*)((hdr_bdldict*)pyld)->next_hdr_p;
  }
  if(!pyld){ return -1;}
  bdl_report* rep = NULL;
  if(pyld->pldclass == BDL_PCLASS_CUST)
    rep = parseReport(pyld);
  
  if(rep && rep->flags & BDL_CUST_SUCC && rep->reason == BDL_CUST_ACCEPT){
    char* source = new char[rep->reg_len+1+rep->adm_len+1];
    if(!source) { abort(); }

    memcpy(source,rep->reg_id,rep->reg_len);
    source[rep->reg_len]=',';
    memcpy(&source[rep->reg_len+1],rep->adm_id,rep->adm_len);
    source[rep->reg_len+1+rep->adm_len]='\0';
    
    int offset  = 0;
    int fraglen = 0;
    if(rep->flags & BDL_RPT_FRAG) {
      offset = rep->frag_off;
      fraglen= rep->frag_len;
    }
    delete rep;
    rep = NULL;
    delete source;
    source=NULL;
  }
  delete bundle;
  return 0;
}

/** Rebuilds a status report from payload.
 *
 * \returns A new bdl_report.
 * \retval  NULL on failure.
 *
 * \note The returned report must be deleted.
*/
bdl_report* BundleManager :: parseReport(hdr_bdlpyld* pyldhdr){
  if(!(pyldhdr && (pyldhdr->pldclass == BDL_PCLASS_REPORT || pyldhdr->pldclass == BDL_PCLASS_CUST))){
    return NULL;
  }
  int plen = pyldhdr->len;
  char* pyld = pyldhdr->payload;
  bdl_report* report = new bdl_report;
  if(!report){abort();}
  report->type = pyldhdr->pldclass;
  int pos = 0;
  if(pos+1 > plen){
    return NULL;
  }
  report->flags = pyld[pos++];
  if(report->type == BDL_PCLASS_CUST)
    report->reason = pyld[pos++];
  if(report->flags & BDL_RPT_FRAG){
    if(pos+sizeof(u_int32_t) > plen){ return NULL;}
    memcpy(&report->frag_off,&pyld[pos],sizeof(u_int32_t));
    pos+=sizeof(u_int32_t);
    if(pos+sizeof(u_int32_t) > plen){return NULL;}
    memcpy(&report->frag_len,&pyld[pos],sizeof(u_int32_t));
    pos+=sizeof(u_int32_t);
  }
  if(pos+sizeof(u_int64_t) > plen){return NULL;}
  memcpy(&report->timestamp,&pyld[pos],sizeof(u_int64_t));
  pos+=sizeof(u_int64_t);
  if(report->type == BDL_PCLASS_REPORT){
    if(report->flags & BDL_RPT_RECV){
      if(pos+sizeof(u_int64_t) > plen){return NULL;}
      memcpy(&report->tor,&pyld[pos],sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_FWD){
      if(pos+sizeof(u_int64_t) > plen){return NULL;}
      memcpy(&report->tof,&pyld[pos],sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_DELIV){
      if(pos+sizeof(u_int64_t) > plen){return NULL;}
      memcpy(&report->todeliv,&pyld[pos],sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_EXP){
      if(pos+sizeof(u_int64_t) > plen){return NULL;}
      memcpy(&report->todelet,&pyld[pos],sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
  }else if(report->type == BDL_PCLASS_CUST){
    if(pos+sizeof(u_int64_t) > plen){return NULL;}
    memcpy(&report->tos,&pyld[pos],sizeof(u_int64_t));
    pos+=sizeof(u_int64_t);
  }
  
  if(pos+1 > plen){return NULL;}
  report->reg_len = pyld[pos++];
  if(pos+report->reg_len > plen){return NULL;}
  report->reg_id=new u_int8_t[report->reg_len];
  if(!report->reg_id) {abort(); }
  memcpy(report->reg_id,&pyld[pos],report->reg_len);
  pos+=report->reg_len;
  if(pos+1 > plen){return NULL;}
  report->adm_len = pyld[pos++];
  if(pos+report->adm_len > plen){return NULL;}
  report->adm_id=new u_int8_t[report->adm_len];
  if(!report->adm_id) {abort(); } 
  memcpy(report->adm_id,&pyld[pos],report->adm_len);
  pos+=report->adm_len;
  return report;
}

/** Builds a status report based on requested report type
 *  and which flags that previously has been set.
 * 
 * \param bundle A Bundle for which to send status reports.
 * \param type Which type of status report to send. (BDL_REPT_NORM or BDL_REPT_CUST)
 *
 * \returns A bundle containing requested status report. 
 */
Bundle* BundleManager :: buildReport(Bundle* bundle, int type=BDL_PCLASS_REPORT)
{


  if(!bundle){fprintf(stdout,"BuildReport :: Bundle is null."); return NULL;}
  bdl_report* report = NULL;
  
  if(type == BDL_PCLASS_REPORT && bundle->report_) report = bundle->report_;
  else if(type == BDL_PCLASS_CUST && bundle->custreport_) report = bundle->custreport_;
  else{fprintf(stdout, "No defined report_ (%08x), custreport_ (%08x), or wrong report type (%d).",bundle->report_,bundle->custreport_,type); return NULL;}

  char* drec = report->dest;
  char* srec = agentEndpoint();
  char* rrec = srec;
  char* cust = NULL;
  u_int8_t cos = 0x00;
  int fid = BDL_FID_REPT;
  int pclass = BDL_PCLASS_REPORT;

  if(!strcmp(srec,drec)){ return NULL;}

  if(report->type == BDL_PCLASS_CUST && report->flags & BDL_CUST_SUCC){
    pclass = BDL_PCLASS_CUST;
    int next=BDL_DICT;
    void* nextp = bundle->bdl_dict_;
    report->reason = BDL_CUST_ACCEPT;
    /*Find payload header*/
    while(nextp && next != BDL_PYLD){
      next = ((hdr_bdldict*)nextp)->next_hdr;
      nextp = ((hdr_bdldict*)nextp)->next_hdr_p;
    }
    if(!nextp){ return NULL;}
    // send ack to current custodian, add cos_cust to cos
    if(((hdr_bdlpyld*)nextp)->pldclass == BDL_PCLASS_NORMAL){
      fprintf(stdout, "Buildreport :Preparing to send Custodial Acknowledgement.\n");
      cust = srec;
      cos = BDL_COS_CUST;
      fid = BDL_FID_CACK;
    }
    // if payload is custodial ack
    else if(((hdr_bdlpyld*)nextp)->pldclass == BDL_PCLASS_CUST){
      fprintf(stdout, "BuildReport :Preparing to send Reply on Custodial Ack.\n");
      fid = BDL_FID_ACKR;
    }
else{
      abort();
    }
  }

  /*Length of payload*/
  int len = 0;
  /*Basic report length*/
  len += 3*sizeof(u_int8_t)+sizeof(u_int64_t)+ report->reg_len + report->adm_len;
  /*Fragmentation*/
  if(report->flags & BDL_RPT_FRAG) len+=2*sizeof(u_int32_t);
  if(report->type == BDL_PCLASS_REPORT){
    /*Bundle reception*/
    if(report->flags & BDL_RPT_RECV) len+=sizeof(u_int64_t);
    /*Bundle forwarding*/
    if(report->flags & BDL_RPT_FWD) len+=sizeof(u_int64_t);
    /*Bundle delivery*/
    if(report->flags & BDL_RPT_DELIV) len+=sizeof(u_int64_t);
    /*Bundle deletion*/
    if(report->flags & BDL_RPT_EXP) len+=sizeof(u_int64_t);
  }else if(report->type == BDL_PCLASS_CUST){
    /*Time of signal + reason*/
    len+=sizeof(u_int64_t)+sizeof(u_int8_t);
  }
  //fprintf(stdout, "Report length: %d",len);

  /*Create payload*/
  char* payload = new char[len];
  if(!payload){fprintf(stdout,"Could not allocate memory for payload.\n"); abort();}
  int pos=0;
  /*Add flags*/
  fprintf(stdout, "Report flags: %02x\n",report->flags);
  payload[pos++] = report->flags;
  if(report->type == BDL_PCLASS_CUST) payload[pos++] = report->reason;
  /*Add fragmentation info*/
  if(report->flags & BDL_RPT_FRAG){
    fprintf(stdout, "Adding Fragmentation info\n");
    memcpy(&payload[pos],&(report->frag_off),sizeof(u_int32_t));
    pos+=sizeof(u_int32_t);
    memcpy(&payload[pos],&(report->frag_len),sizeof(u_int32_t));
    pos+=sizeof(u_int32_t);
  }
  
  /*Add orig timestamp*/
  fprintf(stdout, "Orig timestamp: %016llx\n",report->timestamp);
  memcpy(&payload[pos],&(report->timestamp),sizeof(u_int64_t));
  pos+=sizeof(u_int64_t);
;
  if(report->type == BDL_PCLASS_REPORT){
    if(report->flags & BDL_RPT_RECV){
      fprintf(stdout, "Adding Time of Receipt\n");
      memcpy(&payload[pos],&(report->tor),sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_FWD){
      fprintf(stdout, "Adding Time of Forwarding\n");
      memcpy(&payload[pos],&(report->tof),sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_DELIV){
      fprintf(stdout, "Adding Time of Delivery\n");
      memcpy(&payload[pos],&(report->todeliv),sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
    if(report->flags & BDL_RPT_EXP){
      fprintf(stdout, "Adding Time of Deletion\n");
      memcpy(&payload[pos],&(report->todelet),sizeof(u_int64_t));
      pos+=sizeof(u_int64_t);
    }
  }else if(report->type == BDL_PCLASS_CUST){
    /*Add Time of signal*/
    memcpy(&payload[pos],&(report->tos),sizeof(u_int64_t));
    pos+=sizeof(u_int64_t);
  }
 
  /*Add originating bundle source*/
  payload[pos++] = report->reg_len;
  memcpy(&payload[pos],report->reg_id,report->reg_len);
  pos+=report->reg_len;
  payload[pos++] = report->adm_len;
  memcpy(&payload[pos],report->adm_id,report->adm_len);
  pos+=report->adm_len;

  /*Create new headers*/
  hdr_bdlprim* bdl_prim = new hdr_bdlprim;
  hdr_bdldict* bdl_dict = new hdr_bdldict;
  hdr_bdlpyld* bdl_pyld = new hdr_bdlpyld;
  if(!bdl_prim || !bdl_dict || !bdl_pyld){fprintf(stdout,"Could not allocate memory for header."); abort();}
 
  DTNStrRecords* records = new DTNStrRecords();
  if(!records){fprintf(stdout,"Could not allocate memory for string record."); abort();}
   
  bdl_prim->version = BDL_VERSION;
  bdl_prim->next_hdr = BDL_DICT;
  bdl_prim->cos = cos | ((bundle->bdl_prim_->cos) & BDL_COS_PRIMASK);
  bdl_prim->pld_sec = 0x00;
  int tmpval = 0;
  if((tmpval = records->addRecord(drec)) == -1){fprintf(stdout, "Could not add String Record."); abort();}
  bdl_prim->dest = tmpval;
  if((tmpval = records->addRecord(srec)) == -1){fprintf(stdout, "Could not add String Record."); abort();}
  bdl_prim->src = tmpval;
  if((tmpval = records->addRecord(rrec)) == -1){fprintf(stdout,"Could not add String Record."); abort();}
  bdl_prim->rpt_to = tmpval;
  if(cust){
    if((tmpval = records->addRecord(cust)) == -1){fprintf(stdout, "Could not add String Record."); abort();}
    bdl_prim->cust = tmpval;
  }else     bdl_prim->cust = 0x00;
  bdl_prim->timestamp = getTimestamp();
  if(bdl_prim->timestamp <= lastTimestamp_){
    bdl_prim->timestamp = ++lastTimestamp_;
  } else {
    lastTimestamp_ = bdl_prim->timestamp;
  }
  fprintf(stdout, "Report sent at timestamp %016llx with cos %02x\n" , bdl_prim->timestamp  , bdl_prim->cos);
  


  bdl_prim->exp_time = bundle->bdl_prim_->exp_time;


  bdl_prim->elapsed_time = bundle->bdl_prim_->elapsed_time;
  bdl_prim->me = bundle->bdl_prim_->me;
  strcpy(bdl_prim->region, bundle->bdl_prim_->region);
  
  
  bdl_dict->next_hdr = BDL_PYLD;
  bdl_dict->next_hdr_p = bdl_pyld;
  bdl_dict->strcount = records->strcount();
  bdl_dict->record = records;
  
  bdl_pyld->pldclass = pclass;
  bdl_pyld->len = len;

  strcat( bdl_pyld->payload,"");
  strcpy(bdl_pyld->payload, payload);
  strcat( bdl_pyld->payload,"\0");  

  Bundle* temp = new Bundle();
  if(!temp){DPRINT(DEB_ERR,"Could not allocate memory for bundle."); abort();}
  temp->fid_      = fid;
  temp->bdl_prim_ = bdl_prim;
  temp->bdl_dict_ = bdl_dict;
  temp->bdl_prim_->hopcount=da_->max_hop_count;
  
  return temp;
}

/** Builds either a normal status report or a custody acknowledgement. 
 *  The report is then enqueued. 
 * 
 * \param bundle A Bundle for which to send status reports.
 *
 * \retval 0 on success.
 * \retval -1 on failure. 
 */


int BundleManager :: sendReport(Bundle* bundle)
{
  if(!bundle){fprintf(stdout,"Bundle is null."); return -1;}
  int retval = 0;
  if(bundle->custreport_ && bundle->custreport_->flags ){
    fprintf(stdout, "------> Building Custody Acknowledgement.\n");
    Bundle* tmp = buildReport(bundle,BDL_PCLASS_CUST);
  
    if(!tmp){fprintf(stdout,"No custody ack built."); retval = -1;}
    else{ 
      fprintf(stdout,"BM: sendReort  tmp->fid_== %i\n",tmp->fid_);
      fprintf(stdout, "------> Custreport for %d, flags %02x, token %d.\n", bundle->token_,bundle->custreport_->flags,tmp->token_);
      if(er_->add_bundle(tmp) !=-1) bundle->custreport_->flags = 0;
      else{fprintf(stdout,"Could add bundle to the buffer."); delete tmp; retval = -1;}

    }
  }

  if(bundle->report_ && bundle->report_->flags){
    fprintf(stdout,"------> Building Status Report.");
    Bundle* tmp = buildReport(bundle,BDL_PCLASS_REPORT);
    if(!tmp){ retval = -1;}
    else{
      if(er_->add_bundle(tmp) !=-1) bundle->report_->flags = 0;
      else{fprintf(stdout,"Could not enqueue."); delete tmp; retval = -1;}
    }
  }
  return retval;
return 1;

}

/** Get local agent address.
 *
 * Allocates a new string if no previous string exists.
 *
 * \returns Local agent address.
 */
char* BundleManager :: agentEndpoint()
{
  if(agentEndpoint_) return agentEndpoint_;

  Node* local=Node::get_node_by_address(da_->addr());
  if(! local) { abort();}
  char* reg = routes_->region();
  if(! reg) { abort();}
  const char* adm = local->name();
  int reglen = strlen(reg);
  int admlen = strlen(adm);
  int len = reglen + admlen + 5;

  char* str = new char[len];
  if(! str) { abort();}
  strncpy(str,reg,reglen);
  str[reglen] = ',';
  strncpy(&str[reglen+1],adm,admlen);
  strncpy(&str[reglen+1+admlen],":0\0",3);
  agentEndpoint_ = str;
  return agentEndpoint_;
}

/** Calculates current queue size for the specified queue.
 *
 * \param queue Queue to get size of.
 *
 * \returns Queue size.
 */
int BundleManager :: getQueueSize(Bundle* queue)
{
  int size = 0;
  Bundle* cur = queue;
  Bundle* curfrag = NULL;
  while(cur){
    size+=sizeof(hdr_bdlprim) + cur->getDataSize();
    curfrag = cur->next_frag_;
    while(curfrag){
      size+=sizeof(hdr_bdlprim) + curfrag->getDataSize();
      curfrag = curfrag->next_frag_;
    }
    cur = cur->next_;
  }
  return size;
}


/** Informing other nodes about our presence */

int BundleManager :: sendHello()
{
 // Add the estimated number of nodes to the hello message
 char tmp[1024];
 sprintf(tmp, "%f", da_->current_number_of_nodes);
 char length[10];
 sprintf(length, "%i", sizeof(tmp));
 if(newBundle(agentEndpoint(),"BROADCAST,BROADCAST:1",agentEndpoint(),"NORMAL","NONE","100","bindM1",tmp,length,-1))
	fprintf(stdout, "hello Bundle can not be created !\n");

 float t = 0.5*da_->hello_interval+2*(0.5)*da_->hello_interval*Random::uniform();
 //ht_->resched(0.5*da_->hello_interval+2*(0.5)*da_->hello_interval*Random::uniform());
ht_->resched(0.5*da_->hello_interval+2*(0.5)*da_->hello_interval*Random::uniform());
}


/** gathering informations about other nodes */

int BundleManager :: recvHello(Bundle *b,char * from)
{
	//fprintf(stdout, "Number of delivered bundles: %i\n",da_->delivered_bundles);

	// Get the data within the hello message and update the estimated number of nodes
	char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
	strcpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload);
	datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len]='\0';
	//fprintf(stdout," receiving: %s\n", datab);
	double osnn = atof(datab);
	double tmpNn = (osnn + da_->estimated_number_of_nodes)/2;	
	if(tmpNn > 0)
		da_->current_number_of_nodes = tmpNn;
	da_->estimated_number_of_nodes = da_->current_number_of_nodes;	

	Node* prev = Node::get_node_by_address(b->bdl_prim_->me);
 	string dest_to_track;
 
	 if(this->da_->maintain_stat==1)
 		this->er_->initialization();
 
 	char id[15];
 	strcpy(id,"");
	strcat(id,prev->name());
 	strcat(id,"\0");

 	dest_to_track.append(b->bdl_prim_->region);
 	dest_to_track.append(",");
 	dest_to_track.append(id);
 	dest_to_track.append(":0");
	
 	string source_id;
	er_->get_source_id_of_bundle(b, source_id);
	char region[8];
	strcpy(region, b->bdl_prim_->region);	
	u_int8_t rule = b->bdl_prim_->source_rule;
 	
	//fprintf(stdout, "meeting between %s and %s\n",agentEndpoint(), id);
	if(exist_neighbor(id) == -1)
	{
		
   	  		
		er_->total_meeting_samples_not_unique = er_->total_meeting_samples_not_unique + (TIME_IS-er_->last_meeting_instant_not_unique);
		er_->number_of_meeting_not_unique++;		
		er_->last_meeting_instant_not_unique = TIME_IS;

		if(routes_->is_there(b->bdl_prim_->me, region,1,1,10000000) == 0)
		{
			routes_->add(b->bdl_prim_->me, region,1,1,10000000,"wireless");
  		
			er_->number_of_meeting++;
      			er_->total_meeting_samples = er_->total_meeting_samples + (TIME_IS-er_->last_meeting_instant);
      			
			if(this->da_->maintain_stat == 1 && er_->statBloom_ != NULL)
      			{
      				er_->statBloom_->number_of_meeting = er_->number_of_meeting;
      				er_->statBloom_->total_meeting_samples=er_->total_meeting_samples;
      			}
			
			er_->last_meeting_instant =TIME_IS;
 		}

		//if(er_->total_meeting_samples !=0)
		//	fprintf(stdout,"Intermeeting time: %f\n",er_->number_of_meeting/er_->total_meeting_samples);

		delete b;
		nm_->add_neighbor(id, region);
		// Sending the antipacket matrix
		er_->send_antipacket_matrix((char*)source_id.c_str());
 			
	 	// Run EpidemicRouting Session
	 	if(da_->rule > rule)
			er_->init_session(id, region, source_id);
		
		//Maintaining DTN Network Statistics
 		if(da_->maintain_stat == 1 && er_->statBloom_ != NULL)
	 		er_->statBloom_->add_node((char*)source_id.c_str(), TIME_IS);
 		
		// Sending The Stat Matrix
		if(da_->rule > rule && this->da_->maintain_stat == 1 )
			er_->send_stat_matrix((char*)source_id.c_str(), nm_->get_neighbor_arrival_date(id, region), 0); 
		//else fprintf(stdout, "da_->rule %i > rule %i\n", da_->rule, rule);
	
	}else	//if(!er_->IsSessionActive(source_id) )
	{
		er_->total_meeting_samples_not_unique = er_->total_meeting_samples_not_unique + (TIME_IS-er_->last_meeting_instant_not_unique);
		er_->number_of_meeting_not_unique++;		
		er_->last_meeting_instant_not_unique = TIME_IS;
		// Sending The Stat Matrix
		if(da_->rule > rule && this->da_->maintain_stat == 1 )
			er_->send_stat_matrix((char*)source_id.c_str(), nm_->get_neighbor_arrival_date(id, region), 0); 
		delete b;
		// Called just in order to update the data base
		nm_->add_neighbor(id, region);

//&& er_->buffer_last_update > nm_->get_neighbor_arrival_date(id,b->bdl_prim_->region) + 10
		 //er_->send_antipacket_matrix((char*)source_id.c_str());

	 	// Sending The Stat Matrix
		//if(this->da_->maintain_stat == 1)
		//	er_->send_stat_matrix((char*)source_id.c_str(),nm_->get_neighbor_arrival_date(id, region)); 
		
		//Maintaining DTN Network Statistics
 		//sif(da_->maintain_stat == 1)
	 	//	er_->statBloom_->add_node((char*)source_id.c_str(), TIME_IS);
 		
	 	// Run EpidemicRouting Session
	 	//if(da_->rule > rule)
		//	er_->init_session(id,region, source_id);
 	}

	// either for updating or adding a new node
	
	
 	return 0;
}

int BundleManager::exist_neighbor(char *id)
{
 nm_->check_neighbors();
 return nm_->exist_neighbor(id);
}


