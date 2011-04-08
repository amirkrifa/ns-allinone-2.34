/*
 * Copyright (c) 2007,2008 INRIA
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 * Author: Amir Krifa <amir.krifa@sophia.inria.fr>
 */


#include "epidemic-routing.h"
#include <stdlib.h>
#include <time.h>
#include "bundlemanager.h"
#include "dtn.h"
#include "timers.h"
#include <string>
#include "neighbors.h"
#include <cmath>


/** EpidemicRouting Constructor 
 *\param b : A pointer to the BundleManager Object
 */

double *refTab = NULL;
EpidemicRouting::EpidemicRouting (BundleManager *b):number_of_blocks_waiting_for_ack(0),number_of_blocks_to_ack(0)
{

	statBloom_ = NULL;
	cbt_=new CleanBufferTimer(this);
	cbt_->sched(BUFFER_CLEANING_INTERVAL);
	dbl_= new Delivred_Bundles_List();

	traceTimer = new TraceTimer(this);

	traceTimer->sched(4000);

	sem_init(&buffer_sem, 0, 1);

	init_buffers();
	b_to_ack=NULL;
	lw_ack=NULL;
	//srand(time(NULL));
	number_of_blocks_waiting_for_ack=0;
	bm_=b;
	total_meeting_samples=0;
	last_meeting_instant=0;
	number_of_meeting=0;
	total_meeting_samples_not_unique =0;
	last_meeting_instant_not_unique =0;
	number_of_meeting_not_unique = 0;
}


void EpidemicRouting::initialization()
{

	if(bm_->da_->maintain_stat == 1 && statBloom_ == NULL)
	{	
		LoadRefStat();

		string agent(bm_->agentEndpoint());
		if(statSize == NULL)
		{
			string fileName;
			fileName.append("stat");
			fileName.append(agent);
			fileName.append(".txt");
			statSize = fopen((char*)fileName.c_str(),"w+");
			if(statSize == NULL)
			{fprintf(stderr, "Unable to open statSize\n");perror(NULL); exit(-1);}

		}


		if(log_meetric == NULL)
		{
			string drLog("./log_metric-");
			drLog.append(bm_->agentEndpoint());
			drLog.append(".txt");			
			log_meetric = fopen(drLog.c_str(),"w");
		}



		if(statBloom_ == NULL)
		{
			statBloom_ = new Network_Stat_Bloom(dbl_,bm_);
			statBloom_->InitLogFiles();
			if(statBloom_ == NULL)
			{fprintf(stderr, "statBloom_ == NULL\n");exit(-1);}
		}

	}
}
/* EpidemicRouting Destructor */

EpidemicRouting::~EpidemicRouting()
{
	if(statBloom_ != NULL) delete statBloom_;
	if(dbl_ != NULL)   delete dbl_;
	if(cbt_ != NULL)   delete cbt_; 
	if(traceTimer != NULL) delete traceTimer;
	free_buffers();
	sem_destroy(&buffer_sem);
	// Closing the log files

	double avg_stat_per_meeting = (double)bm_->da_->stat_data_size / (double)number_of_meeting;
	fprintf(statSize, "avg_stat_per_meeting %f\n", avg_stat_per_meeting);
	fclose(log_meetric);
	fclose(statSize);
}

/**Init DTNAgent main Buffer*/

void EpidemicRouting::init_buffers()
{
	number_of_buffered_bundles = 0;
}

/** Free Dtn Agent Buffers */

void EpidemicRouting::free_buffers()
{

	// Cleaning the main Buffer 
	while(!bundleStoreList.empty())
	{
		list<BundleStore *>::iterator iter = bundleStoreList.begin();
		delete *(iter);
		bundleStoreList.erase(iter);
	}

	bm_=NULL;

	// Cleaning the visitors map

	while(!visitorsMap.empty())
	{
		map<string, VisitorList*>::iterator iter = visitorsMap.begin();
		delete iter->second;
		visitorsMap.erase(iter);
	}

	while(!sessionsMap.empty())
	{
		map<string, EpidemicSession*>::iterator iter = sessionsMap.begin();
		delete iter->second;
		sessionsMap.erase(iter);
	}
	bundlesSeen.clear();

}

/** BundleStore Constructor 
 * \param mst Max Sprying time
 */

BundleStore::BundleStore (int mst)
{
	time_stamp=TIME_IS;
	b_=NULL;
	forwarding_number=0;
	enable_sprying=1;
	max_sprying_time=mst;
	network_copy_time=0;
}

/** BundleStore Destructor **/
BundleStore::~BundleStore()
{
	delete b_;
}

/** Update a BundleStore Elapsed Time 
 * \param d Current Time
 */

void BundleStore::update_elapsed_time(double d)
{
	(b_->bdl_prim_)->elapsed_time += (d-time_stamp);
	network_copy_time+=(d-time_stamp); 
	time_stamp = d;

	if((b_->bdl_prim_)->elapsed_time > max_sprying_time)
		enable_sprying=0;
}


/** Return a BundleStore Elapsed Time 
 * \returns The Bundle Elapsed Time
 */

double  BundleStore::get_elapsed_time()
{
	return b_->bdl_prim_->elapsed_time;
}

/** Set a BundleStore ID 
 * \param s The budle ID
 */

void BundleStore::setid(char *s)
{
	id.assign(s);
}

/** Set a BundleStore Bunlde Pointer 
 * \param b The Bundle pointer
 */

void BundleStore:: set_bundle(Bundle *b)
{
	b_=b;
}

/** BundlesToAck Constructor
 * \param er A pointer to the EpidemicRouting object
 * \param list The list of Bundles to Ack
 * \param dest The dest of the Ack
 */

BundlesToAck::BundlesToAck(EpidemicRouting *er,char * list,char *dest)
{
	l_to_ack.append(list);
	dest_.append(dest);
	current_ack=0;
	nb_to_ack=er_->number_of_bundles(list,strlen(list));
	ready=0;
	er_=er;
	next=NULL;
}


/** BundlesToAck Destructor*/

BundlesToAck::~BundlesToAck()
{
	er_=NULL;
}

/** Check if another bundle was ack 
 * \param uid The bundle Unique id
 * \returns
 */

int  BundlesToAck::exist_bundle(char *uid)
{
	char l_to_check[strlen(get_list_to_ack())];
	strcpy(l_to_check,"");
	strcat(l_to_check,get_list_to_ack());
	strcat(l_to_check,"\0");
	int i=0;
	int j=0;
	char current_id[100];
	while(l_to_check[i]!='\0')
	{
		if(l_to_check[i]!='-' )
		{
			current_id[j]=l_to_check[i];
			j++;
		}
		else {current_id[j]='\0';
		if(strlen(current_id)>strlen("REGION1:_o32:1"))
		{
			if(strcmp(current_id,uid)==0){// Another Bunlde was ack 
				current_ack++;
				if(current_ack==nb_to_ack) ready=1;
				break;
			}
			strcpy(current_id,"");
			j=0;
		}
		j=0;
		}
		i++;
	}
	return 1;
}
/** Get a bundle UID, Unique ID 
 * \param b The bundle pointer
 * \returns The Bundle unique ID
 */

void EpidemicRouting::getBundleIdUid(Bundle *b, string & bundleUid)
{ 
	char idd[strlen("REGION1,_o")+bm_->da_->max_uid_length+2];
	strcpy(idd,"");
	strncat(idd,getBundleId(b),strlen(getBundleId(b)));
	char isecret[bm_->da_->max_uid_length+1];
	strcpy(isecret,"");
	sprintf(isecret,"r%i",b->bdl_prim_->u_id);
	strncat(idd,isecret,strlen(isecret));
	strcat(idd,"\0");
	bundleUid.assign(idd);
}


/** Delete A Block if it is already acked  
 * \param dest DTN address to which the Ack will be sent
 */

void EpidemicRouting ::delete_bta_block(const char *dest)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		BundlesToAck *bta=b_to_ack;
		BundlesToAck *bta_prev=b_to_ack;
		int del=0;
		while(bta!=NULL)
		{
			if(strcmp(bta->getDest(),dest)==0){
				if(bta->get_state()==1)
				{  number_of_blocks_to_ack--;

				if(bta==b_to_ack)
				{ if(number_of_blocks_to_ack==1)
				{
					b_to_ack->next=NULL;
					delete b_to_ack;
					b_to_ack=NULL;
					bta=NULL;
					bta_prev=NULL;
					del=1;
					break;
				}
				else{
					b_to_ack=b_to_ack->next;
					bta->next=NULL;
					delete bta;
					bta=NULL;
					bta_prev=NULL;del=1;
					break;
				}
				} else if(bta->next==NULL)
				{bta_prev->next=NULL;
				delete bta;
				bta=NULL;
				bta_prev=NULL;
				break;
				} else
				{ bta_prev->next=bta->next;
				bta->next=NULL;
				delete bta;
				bta=NULL;
				bta_prev=NULL;del=1;
				break;
				}


				}
			}
			bta_prev=bta;
			bta=bta->next;
		}
	}
}

/** delete a retransmission list if an ack for that list was recived 
 * \param bloc A pointer to the List that should be retransmitted if no Ack was reciverd for her
 * \returns 1 if the list was suceffuly deleted
           -1 if there was an error when deleting the List
 */

int EpidemicRouting::delete_ret_list(ListWaitingAck * bloc)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		ListWaitingAck *lwa=lw_ack;
		ListWaitingAck *lwa_prev=lw_ack;

		if(lwa==bloc)
		{
			lw_ack=lw_ack->next;
			bloc->next=NULL;
			delete bloc;
			bloc=NULL;
			lwa_prev=NULL;
			lwa=NULL;
			number_of_blocks_waiting_for_ack--;
			return 1;
		}
		else
		{
			while(lwa != bloc)
			{   lwa_prev=lwa;
			lwa=lwa->next;
			}
			if(lwa->next==NULL)
			{
				lwa_prev->next=NULL;
				delete lwa;
				lwa=NULL;
				lwa_prev=NULL;
				number_of_blocks_waiting_for_ack--;
				return 1;
			}
			else
			{
				lwa_prev->next=lwa->next;
				lwa->next=NULL;
				delete lwa;
				lwa_prev=NULL;
				lwa=NULL;
				number_of_blocks_waiting_for_ack--;
				return 1;
			}
		}

		return -1;
	}
	return -1;
}

/**Update the Block of Bundles to ack list, watch if the node have to send an ack for a block or not
 * \param b A pointer to the new recived Bundle
 */

void EpidemicRouting::update_bta_list(Bundle *b)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		Node* dn = Node::get_node_by_address(b->bdl_prim_->me);
		if(bm_->exist_neighbor((char *)dn->name()) != -1  )
		{
			string dest;
			dest.assign(b->bdl_prim_->region);
			dest.append(",");
			dest.append(dn->name());
			dest.append(":0");
			BundlesToAck *bta=b_to_ack;

			while(bta!=NULL)
			{
				if(dest.compare(bta->getDest())==0)
				{
					string bundleUid;
					getBundleIdUid(b, bundleUid);
					bta->exist_bundle((char*)bundleUid.c_str());
					if(bta->get_state()==1)
					{ 
						if(!bm_->newBundle(bm_->agentEndpoint(),dest.c_str(),bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1","","0",4))
						{
							delete_bta_block(dest.c_str());
							break;
						}
					}
				}
				bta=bta->next;
			}
		}
	}
}

/** Called when a block Ack was recived by the DTNAgent
 * \param b A pointer to the Ach which was recived
 */

int EpidemicRouting::recv_block_ack(Bundle *b)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		// Dest UID
		char dest[30];
		strcpy(dest,"");
		strcat(dest,getBundleId(b));
		strcat(dest,"\0");
		ListWaitingAck *lwa= lw_ack;
		while(lwa !=NULL)
		{
			if(strcmp(lwa->getDest(),dest)==0) break;
			lwa=lwa->next;
		}
		if(lwa!=NULL)
			delete_ret_list(lwa);
		else {fprintf(stdout,"RBA Error\n"); return -1;}

		delete b;
		return 0;
	}
	delete b;
	return 0;
}

/** ListWaitingAck Constructor 
 * \param rlt A pointer to the Resend List Timer
 * \param ltr A pointer to the list of bundles to retransmit
 * \param dest The DTN node Adress to which the list of bundles will be retransmitted
 */

ListWaitingAck::ListWaitingAck(ResendListTimer *rlt,char * ltr,char *dest,char * dr, nsaddr_t da,EpidemicRouting *er)
{
	er_=er;
	// A Pointer to the Block Ack Timer
	rlt_=rlt;
	// The list to resend in case of timer expiration
	list_to_retransmit.append(ltr);

	// The destination
	retransmit_to_dest.append(dest);
	next=NULL;
	rlt_->setList(this);
	rlt_->sched(er_->bm_->da_->block_resend_interval);
	// dest Addr
	dest_addr=da;
	// dest Region
	dest_region.append(dr);
	// the number of retransmission
	number_of_ret=0;
}


/** ListWaitingAck Destructor  */
ListWaitingAck::~ListWaitingAck()
{ er_=NULL; 
if(rlt_->status()==TIMER_PENDING )
	rlt_->cancel();
delete rlt_;
}

/** Add a list which waiting for an Ack  
 * \param dest The DTN node Adress to which the list of bundles will be retransmitted
 * \param list The list of Bundles to retransmit
 */

void EpidemicRouting::add_list_wack(char * dest,char * list,char * dr, nsaddr_t da)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		number_of_blocks_waiting_for_ack++;
		char l_temp[strlen(list)];
		strcpy(l_temp,list);
		l_temp[strlen(l_temp)]='\0';
		char d_temp[strlen(dest)];
		strcpy(d_temp,dest);
		d_temp[strlen(d_temp)]='\0';
		char dr_temp[strlen(dr)];
		strcpy(dr_temp,dr);
		dr_temp[strlen(dr)]='\0';
		nsaddr_t da_temp=da;
		if(lw_ack==NULL)
		{
			lw_ack=new ListWaitingAck(new ResendListTimer(this),l_temp,d_temp, dr_temp,da_temp,this);
		}
		else
		{
			ListWaitingAck * lwa_=lw_ack;
			while(lwa_->next!=NULL) lwa_=lwa_->next;
			lwa_->next=new ListWaitingAck(new ResendListTimer(this),l_temp,d_temp,dr_temp,da_temp,this);
			lwa_=NULL;
		}

	}
}

/** add a Bundles List which will be acked later  
 * \param l The list of
 */

void EpidemicRouting::add_bl_toack(char *l,char *d)
{if(bm_->da_->activate_ack_mechanism==1)
{
	if(b_to_ack==NULL )
	{
		b_to_ack=new BundlesToAck(this,l,d);
		number_of_blocks_to_ack++;
	}
	else{
		BundlesToAck *bta=b_to_ack;
		while(bta->next!=NULL) bta=bta->next;
		bta->next=new BundlesToAck(this,l,d);
		bta=NULL;
		number_of_blocks_to_ack++;
	}


	BundlesToAck *b=b_to_ack;
	while(b!=NULL)
	{
		b=b->next;
	}
}
}

/** Resend a Bundles Block if the Timer ResendListTimer expire
 **/

void EpidemicRouting::resend_bundles_block(ListWaitingAck * bloc)
{
	if(bm_->da_->activate_ack_mechanism==1)
	{
		Node* nc = Node::get_node_by_address(bloc->dest_addr);
		if(bm_->exist_neighbor((char *)nc->name())!=-1  )
		{
			if( bloc->number_of_ret< bm_->da_->max_block_retransmission_number)
			{
				send_bundles_list((char*)bloc->get_list_to_retransmit(),strlen(bloc->get_list_to_retransmit()),bloc->dest_addr,(char *)bloc->get_dest_region(),1,bloc->getDest());
				bloc->number_of_ret+=1;
				bloc->rlt_->resched(bm_->da_->block_resend_interval);
			}
			else
				/** deleting the block **/
			{
				bm_->da_->number_of_deleted_bundles_due_to_ret_failure+=number_of_bundles((char	 *)bloc->get_list_to_retransmit(),strlen(bloc->get_list_to_retransmit()));
				if(delete_ret_list(bloc)==-1) {fprintf(stdout,"RET: Error when deleting a bloc \n");}
			}
		}
		else
		{//deleting the block 
			bm_->da_->number_of_deleted_bundles_due_to_ret_failure += number_of_bundles((char *)bloc->get_list_to_retransmit(),strlen(bloc->get_list_to_retransmit()));
			bm_->da_->number_of_deleted_bundles_due_to_ret_failure +=1;
			if(delete_ret_list(bloc)==-1) {fprintf(stdout,"RET: Error when deleting a bloc \n");}
		}
	}
}

/** Check for Bundles elapsed Time and cleaning Buffers
 */

void EpidemicRouting::make_clean()
{
	//clean_visitor_list();

	if(bm_->da_->infinite_ttl == 0)
	{
		if(number_of_buffered_bundles > 0 )
		{
			sem_wait(&buffer_sem);

			list<BundleStore *>::iterator iter = bundleStoreList.begin();
			while(iter != bundleStoreList.end())
			{
				// Updating Bundles elapsed Time
				(*iter)->update_elapsed_time(TIME_IS);
				if((*iter)->get_elapsed_time() >= (*iter)->b_->bdl_prim_->exp_time )
				{ 
					buffer_last_update = TIME_IS;

					if(bm_->da_->maintain_stat==1)
					{
						string bundleUid;
						this->getBundleIdUid((*iter)->b_, bundleUid);


						statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),0,(*iter)->get_elapsed_time(),(*iter)->b_->bdl_prim_->exp_time);	
					}
					delete (*iter);
					iter = bundleStoreList.erase(iter);
					number_of_buffered_bundles--;
					bm_->da_->epidemic_buffered_bundles = number_of_buffered_bundles;
					continue;
				}
				iter++;
			}
			sem_post(&buffer_sem);
		}

		cbt_->resched(BUFFER_CLEANING_INTERVAL);
	}
}

/** Length of the local list of Bundles UID's
 * \param dest the Bundles destination, used if copies sprying was stopped and we should give messages directely to their destinations
 */

int EpidemicRouting::length_of_local_list(char * dest)
{
	int l = 0;
	char *d;

	if(bm_->da_->activate_routing == 1)
	{
		for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end(); iter++)
		{
			l += (strlen((*iter)->getid())+1);
		}
	}
	else if(bm_->da_->activate_routing == 2	)
	{
		for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end(); iter++)
		{
			d = (*iter)->b_->bdl_dict_->record->getRecords((*iter)->b_->bdl_prim_->dest);
			if((*iter)->enable_sprying==1 || strcmp(dest,d) == 0)
				l += (strlen((*iter)->getid())+1);
		}
	}

	return l;
}

/** Return the UID's list of local Bundles 
 * \param dest the Bundles destination, used if copies sprying was stopped and we should give messages directely to their destinations
 */

void EpidemicRouting::bundles_id_list(char * dest, string & strList)
{ 

	if(number_of_buffered_bundles > 0)
	{
		char* d;
		int i = 0;

		for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end(); iter++)
		{
			d = (*iter)->b_->bdl_dict_->record->getRecords((*iter)->b_->bdl_prim_->dest);

			if(bm_->da_->activate_routing == 2)
			{
				if((*iter)->enable_sprying == 1 || strcmp(d,dest)==0 )
				{
					strList.append((*iter)->getid());
					strList.append("-");
				}
			}
			else if(bm_->da_->activate_routing == 1)
			{
				strList.append((*iter)->getid());
				strList.append("-");
			}
			i++;
		}

	}
}



/** Retunr the Bundle ID
 * \param b the Bundle pointer
 * \returns The Bundle id
 * \retval  ID on success.
 * \retval NULL on failure.
 */

char *EpidemicRouting::getBundleId(Bundle *b)
{
	if(b == NULL) fprintf(stderr, "Error\n");
	//b->ShowDetails();
	return b->bdl_dict_->record->getRecords(b->bdl_prim_->src);
}


/** Check if the Bundle ID is present or not in the DTN Agent Buffer 
 * \param id The Bundle id
 * \param u_id The Bundle unique id
 * \retval BundleStore* on success
 * \retval NULL on failure
 */
BundleStore* EpidemicRouting::exist_id(char * id,int u_id)
{
	string idd;
	if(u_id != -1)
	{
		idd.append(id);
		char isecret[bm_->da_->max_uid_length+1];
		strcpy(isecret,"");
		sprintf(isecret,"r%i",u_id);
		idd.append(isecret);
	}
	else
	{
		idd.append(id);
	}
	for(list<BundleStore *>::iterator iter = bundleStoreList.begin();iter != bundleStoreList.end();iter++ )
	{
		if(strcmp((*iter)->getid(), (char*)idd.c_str()) == 0) 
			return (*iter);
	}

	return NULL;
}

/** Check if there is place in the DtnAgent Local Buffer  
 * \retval  1 if there is space in the main Buffer
 * \retval -1 if there is no space in the main Buffer
 */

int EpidemicRouting::free_place()
{
	if(number_of_buffered_bundles < bm_->da_->max_bundles_in_local_buffer)
		return 1;
	else
		return -1;
}


/** return the u_id of a Bundle 
 *  \param id The Bundle ID
 *  \retval u_id on success
 *  \retval 0 on failure
 */

int EpidemicRouting::get_u_id(char *id)
{
	char *s =strchr(id,'r');
	char ss[strlen(s)];
	strcpy(ss,"");
	int i=1;
	int j=0;
	while(s[i]!='\0'){ss[j]=s[i];i++;j++;}
	ss[j]='\0';
	return atoi(ss);
}

/** return id REGION:NODE 
 * \param id The Bundle id.
 * \retval Region on success
 * \retval NULL on failure
 */
void EpidemicRouting::get_rn(char *id, string & rn)
{
	char *s =strchr(id,'r');
	char temp[strlen(id)-strlen(s)+1];
	strncpy(temp,id, strlen(id)-strlen(s));
	temp[strlen(id)-strlen(s)]='\0';
	rn.assign(temp);
}

/** Convert a numerical position in the main Buffer to a BundleStore* 
 * \param  position The position in the main buffer.
 * \retval BundleStore* on success.
 * \retval NULL on failure.
 */
BundleStore * EpidemicRouting::get_pointer_to(int position)
{
	if(position <= number_of_buffered_bundles)
	{
		int i=1;
		for(list<BundleStore *>::iterator iter = bundleStoreList.begin();iter != bundleStoreList.end(); iter++, i++)
		{
			if(i == position)
				return (*iter);

		}
	}
	return NULL;
}

/** Applicate the drop tail policy (+ Source Priority) in case of congestion
 * \param b A pointer to the Bundle to be added
 * \retval 1 on success.
 * \retval 0 on failure.
 */
int EpidemicRouting::add_with_drop_tail(Bundle *b)
{
	if(is_my_message(b) != 1)
	{
		bm_->da_->deleted_bundles ++;
		delete b;
		b = NULL;
		return 1;
	}
	else 
	{ 

		int lp = number_of_buffered_bundles;
		while(is_my_message(get_pointer_to(lp)->b_) == 1 && lp > 0)
			lp--;
		if(lp == 0)
		{
			bm_->da_->deleted_bundles ++;
			delete b;
			b = NULL;
			return 1;
		}
		else 
		{ 
			list<BundleStore*>::iterator iter = bundleStoreList.begin();
			while(iter != bundleStoreList.end())
			{
				if(strcmp(get_pointer_to(lp)->getid(), (*iter)->getid()) == 0)
					break;
				iter++;
			}

			if( iter != bundleStoreList.end())
			{
				delete *iter;
				bundleStoreList.erase(iter);
			}
		}

		string bundleUid;
		this->getBundleIdUid(b, bundleUid);

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->b_ = b;
		tmp->setid((char*)bundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		bundleStoreList.push_back(tmp);
		tmp = NULL;
		return 1;
	}

	return 0;
}

int EpidemicRouting::add_with_drop_from_front(Bundle *b)
{
	list<BundleStore *>::iterator iter =  bundleStoreList.begin();

	if(is_my_message((*iter)->b_) != 1)
	{ 
		string bundleUid;
		this->getBundleIdUid((*iter)->b_, bundleUid);
		delete (*iter);
		bundleStoreList.pop_front();

	}else 
	{ 
		list<BundleStore *>::iterator iter =  bundleStoreList.begin();

		while(iter != bundleStoreList.end() && is_my_message((*iter)->b_) == 1 )
		{
			iter++;
		}

		if(iter != bundleStoreList.end())
		{
			// Updating the status of a bundle in the Stat Matrix
			string bundleUid;
			this->getBundleIdUid((*iter)->b_, bundleUid);
			delete *iter;
			bundleStoreList.erase(iter);
		}
		else 
		{ 
			list<BundleStore *>::iterator iter =  bundleStoreList.begin();

			delete (*iter);
			bundleStoreList.erase(iter);
		}
	}

	string bundleUid;
	this->getBundleIdUid(b, bundleUid);

	// Add the new bundle at the end of the queue 
	BundleStore * tmp =new BundleStore(bm_->da_->max_sprying_time);
	tmp->setid((char*)bundleUid.c_str());
	tmp->set_arrival_date(TIME_IS);
	tmp->b_ = b;
	bundleStoreList.push_back(tmp);
	tmp = NULL;
	return 1;
}


/** Return a pointer to the previous BundleStore (FIFO Queue) of the youngest one 
 * \retval BundleStore* on success.
 * \retval NULL on failure.
 */

list<BundleStore *>::iterator EpidemicRouting::get_youngest_message(int *c)
{
	double min = 99999;
	int i = 0;
	*c = i;
	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	list<BundleStore *>::iterator return_p = bundleStoreList.begin();

	while(iter != bundleStoreList.end())
	{
		if((*iter)->get_elapsed_time()< min && is_my_message((*iter)->b_)!=1)
		{ 
			min = (*iter)->get_elapsed_time();
			return_p = iter;
			*c = i;
		}
		i++;
		iter++;
	}

	return return_p;
}



/** Return a pointer to the BundleStore (FIFO Queue) of the oldest one 
 * \retval BundleStore* on success.
 * \retval NULL on failure.
 */

list<BundleStore *>::iterator EpidemicRouting::get_oldest_message(int *c)
{
	double max =0;
	int i = 0;
	*c = i;

	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	list<BundleStore *>::iterator return_p = bundleStoreList.begin();

	while(iter != bundleStoreList.end())
	{
		if((*iter)->get_elapsed_time() > max && is_my_message((*iter)->b_) != 1)
		{ 
			max = (*iter)->get_elapsed_time();
			return_p = iter;
			*c = i;
		}

		iter++;
		i++;
	}

	return return_p;
}

/** Applicate the drop youngest policie (+ Source Priority) in case of congestion. 
 * \param b A pointer to the Bundle to be added.
 * \retval 1 on success.
 * \retval 0 on failure.
 */

int EpidemicRouting::add_with_drop_youngest_message(Bundle *b)
{

	int position; 
	list<BundleStore *>::iterator message_to_delete = get_youngest_message(&position);
	//|| is_my_message(b)==1 
	if(b->bdl_prim_->elapsed_time > (*message_to_delete)->get_elapsed_time() || is_my_message(b)==1)
	{

		// Removing the message to delete from the list 
		//ShowBuffredBundles();
		//fprintf(stdout,"Dropping the message: \n");
		//(*message_to_delete)->b_->ShowDetails(); 	
		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);

		string bundleUid;
		this->getBundleIdUid(b, bundleUid);


		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)bundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		//if(strcmp(getBundleId(s->b_), bm_->agentEndpoint())!=0 )
		{
			//s->b_->bdl_prim_->hopcount += 1;
		}
		tmp = NULL;
	}
	else delete b;

	return 1;
}


/** Applicate the drop oldest policie (+Source priority) in case of congestion.
 * \param b A pointer to the Bundle to be added
 * \retval 1 on success.
 * \retval 0 on failure.
 */

int EpidemicRouting::add_with_drop_oldest_message(Bundle *b)
{
	int position;
	list<BundleStore *>::iterator message_to_delete = get_oldest_message(&position);
	//|| is_my_message(b)==1
	if(b->bdl_prim_->elapsed_time < (*message_to_delete)->get_elapsed_time() || is_my_message(b)==1)
	{

		//ShowBuffredBundles();
		//fprintf(stdout,"Dropping the message: \n");
		//(*message_to_delete)->b_->ShowDetails(); 
		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);

		string bundleUid;
		this->getBundleIdUid(b, bundleUid);

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)bundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;
	} else
	{
		delete b;
	}

	return 1;
}

/** Applicate the bin based drop oldest policie (+Source priority) in case of congestion.
 * \param b A pointer to the Bundle to be added
 * \retval 1 on success.
 * \retval 0 on failure.
 */


int EpidemicRouting::add_with_bin_drop_oldest(Bundle * b)
{
	int position;
	list<BundleStore *>::iterator message_to_delete = get_oldest_message(&position);
	if(is_my_message(b) == 1)
	{
		// Then drop the selected oldest
		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);

		string bundleUid;
		this->getBundleIdUid(b, bundleUid);

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)bundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;

	}else 
		if((*message_to_delete)->get_elapsed_time() >= b->bdl_prim_->elapsed_time )
		{
			if( ((*message_to_delete)->get_elapsed_time() - b->bdl_prim_->elapsed_time) >= bm_->da_->drop_oldest_bin)
			{
				// Out of the drop_oldest_bin so we drop the oldest message
				delete *message_to_delete;
				bundleStoreList.erase(message_to_delete);

				string bundleUid;
				this->getBundleIdUid(b, bundleUid);

				BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
				tmp->setid((char*)bundleUid.c_str());
				tmp->set_arrival_date(TIME_IS);
				tmp->b_ = b;
				bundleStoreList.push_back(tmp);
				tmp = NULL;

			}else if(((*message_to_delete)->get_elapsed_time() - b->bdl_prim_->elapsed_time) < bm_->da_->drop_oldest_bin)
			{
				// Select randomly one of the two messages and drop it
				double r = Random::uniform(0, 1);
				if(r > 0.5)
				{
					// drop the oldest
					delete *message_to_delete;
					bundleStoreList.erase(message_to_delete);

					string bundleUid;
					this->getBundleIdUid(b, bundleUid);

					BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
					tmp->setid((char*)bundleUid.c_str());
					tmp->set_arrival_date(TIME_IS);
					tmp->b_ = b;
					bundleStoreList.push_back(tmp);
					tmp = NULL;
				}else
				{
					// drop the new message
					delete b;
				}

			}

		}else if((*message_to_delete)->get_elapsed_time() < b->bdl_prim_->elapsed_time )
		{

			if((b->bdl_prim_->elapsed_time - (*message_to_delete)->get_elapsed_time()) >= bm_->da_->drop_oldest_bin)
			{
				//  Drop the new message
				delete b;
			}
			else if((b->bdl_prim_->elapsed_time - (*message_to_delete)->get_elapsed_time()) < bm_->da_->drop_oldest_bin)
			{
				// Drops randomly
				double r = Random::uniform(0, 1);
				if(r > 0.5)
				{
					// drop the oldest
					delete *message_to_delete;
					bundleStoreList.erase(message_to_delete);

					string bundleUid;
					this->getBundleIdUid(b, bundleUid);

					BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
					tmp->setid((char*)bundleUid.c_str());
					tmp->set_arrival_date(TIME_IS);
					tmp->b_ = b;
					bundleStoreList.push_back(tmp);
					tmp = NULL;

				}else
				{
					delete b;
				}
			}
		}


	return 1;

}


/** Return a pointer to the BundleStore of the  message which has the the smallest metric value 
 * \retval BundleStore* on success.
 * \retval NULL on failure.
 */

list<BundleStore *>::iterator EpidemicRouting::get_message_with_smallest_metric_value_reference(Bundle *b,int *i,double *metric)
{

	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES-1);	
	double min=99999;
	double current_value=0;

	list<BundleStore *>::iterator return_p = bundleStoreList.begin();
	list<BundleStore *>::iterator iter = bundleStoreList.begin();

	int c = 0;
	*i=c;

	while(iter != bundleStoreList.end())
	{ 
		int nons = exact_number_of_node_that_have_seen_it((*iter)->b_);
		int escn = exact_copys_number((*iter)->b_);

		current_value = (1/alpha)*(1-(float)nons/(NUMBER_OF_NODES-1))*((*iter)->b_->bdl_prim_->exp_time - (*iter)->b_->bdl_prim_->elapsed_time)*exp((-1*(escn))*(((*iter)->b_->bdl_prim_->exp_time - (*iter)->b_->bdl_prim_->elapsed_time)/alpha));
		//fprintf(stdout, "nons: %i  escn %i current_value: %f\n", nons, escn, current_value);
		if((current_value < min) && is_my_message((*iter)->b_)!=1 )
		{ 
			min = current_value;
			return_p = iter;
			*i = c;
		}
		c++;
		iter++;
	}

	*metric = min;
	return return_p;
}

list<BundleStore *>::iterator EpidemicRouting::get_message_with_smallest_metric_value_approximation(Bundle *b,int *i,double *metric)
{
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES -1);
	double min=99999;
	double current_value=0;

	list<BundleStore *>::iterator return_p = bundleStoreList.begin();
	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	int c=0;
	*i=c;
	double ni_t,mi_t;
	double dd_t,dr_t;
	while(iter != bundleStoreList.end())
	{ 
		string bundleUid;
		this->getBundleIdUid((*iter)->b_, bundleUid);

		statBloom_->get_stat_from_axe((*iter)->get_elapsed_time(),(char*)bundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);

		current_value = (1/(alpha))*((*iter)->b_->bdl_prim_->exp_time - (*iter)->get_elapsed_time())*dr_t;

		if((current_value < min) && is_my_message((*iter)->b_)!=1 )
		{ 
			min = current_value;
			return_p = iter;
			*i = c;
		}
		c++;
		iter++;
	}

	*metric = min;
	return return_p;
}

/** Return the estimated copys number on the network of a specific message 
 * \param b A pointer the Bundle ( the message )
 * \returns The estimated copys number on the network for a specific message
 */

int EpidemicRouting::exact_copys_number(Bundle *b)
{	
	int nc;
	if(bm_->da_->reference==1)
	{
		nc = get_number_of_copies(b);
		if(nc==0) return 1;
		else return nc;
	}
	return -1;
}	

/** Return the exact number of nodes that have seen the message
 */

int EpidemicRouting::exact_number_of_node_that_have_seen_it(Bundle *b)
{
	if(bm_->da_->reference==1)
		return get_number_of_nodes_that_have_seen_it(b);
	return -1;
}


int EpidemicRouting::add_and_maximize_total_delivery_rate_reference(Bundle *b)
{
	string newBundleUid;
	getBundleIdUid(b, newBundleUid);
	int position=0;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES-1);
	double smallest_metric_in_the_queue;

	double new_bundle_metric_value = (1/alpha)*(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*exp((-1*(exact_copys_number(b)))*((b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)/alpha));

	list<BundleStore *>::iterator message_to_delete = get_message_with_smallest_metric_value_reference(b,&position,&smallest_metric_in_the_queue);
	//fprintf(stdout, "new message metric value: %u smallest buffred message metric value: %u\n",new_bundle_metric_value, smallest_metric_in_the_queue);

	if(new_bundle_metric_value >= smallest_metric_in_the_queue || is_my_message(b) == 1)
	{

		delete (*message_to_delete);
		bundleStoreList.erase(message_to_delete);

		BundleStore * tmp =new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)newBundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;
	} else 
	{ 	
		delete b;
	}

	return 1;
}

void EpidemicRouting::Trace()
{
	/*
if(statBloom_ != NULL)
{
	// Logging convergence informations
	double amtnu;
	if(number_of_meeting_not_unique == 0) amtnu = 1;
	else amtnu = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;

	double alpha = MEETING_TIME*(statBloom_->get_number_of_nodes()  - 1);

	int ttl  = atoi(bm_->da_->getlifespan());

	double ni_t, mi_t;
	double dd_t, dr_t;
	double et;

	statBloom_->get_stat_from_axe(1800.0, NULL, &ni_t, &mi_t, &dd_t, &dr_t);

	et = 1800.0;

	//double metric_dr = (1 / (alpha)) * (ttl - et) * dr_t;


	int nn = NUMBER_OF_NODES ;
	double metric_dd = 0;
	if(nn > 1 + mi_t)
	{
		metric_dd = (double)((pow(dd_t, 2) *alpha) / ( (nn - 1) * (nn - 1 - mi_t)));

		//fprintf(log_meetric,"bin_index %i ni_t %f mi_t %f dr: %f dd: %f et %f dd_up %f dd_down %f\n",i, ni_t, mi_t, metric_dr, metric_dd, et,pow(dd_t, 2), 1/((1 / (alpha))* (nn - 1) * (nn - 1 - mi_t) ));
	}

	string fName;
	fName.append(bm_->agentEndpoint());
	fName.append(".txt");
	FILE * log_convergence;
	log_convergence = fopen(fName.c_str(), "a");
	fprintf(log_convergence,"%f %f\n",TIME_IS, metric_dd);
	fclose(log_convergence);

	traceTimer->resched(200);

}

	if(statBloom_ != NULL)
	{
		// Calculating the sum square error
		// Verify if we should clear the messages matrix:
		/*if(TIME_IS > statBloom_->clearFlag*7200)
	{
		statBloom_->ClearMessages();
		bundlesSeen.clear();
		statBloom_->clearFlag++;
	}

		save_sum_square_error(TIME_IS);

		traceTimer->resched(100);
	}
	 */

}

int EpidemicRouting::add_and_maximize_total_delivery_rate_approximation(Bundle *b)
{
	string newBundleUid;
	getBundleIdUid(b, newBundleUid);
	int position=0;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES  - 1);
	double smallest_metric_in_the_queue;

	double ni_t,mi_t;
	double dd_t,dr_t;

	statBloom_->get_stat_from_axe(b->bdl_prim_->elapsed_time, (char*)newBundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);
	//	if(b->bdl_prim_->elapsed_time > 3200 && b->bdl_prim_->elapsed_time <= 3250)
	//	{
	//		//statBloom_->ShowAllMessagesStatistics();
	//		fprintf(stdout,"elapsed time: %f estimated n: %f exact n: %i\n",b->bdl_prim_->elapsed_time,ni_t, exact_number_of_node_that_have_seen_it(b));
	//	}

	double new_bundle_metric_value=(1/(alpha))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*dr_t;

	list<BundleStore *>::iterator message_to_delete = get_message_with_smallest_metric_value_approximation(b,&position,&smallest_metric_in_the_queue);


	//fprintf(stdout, "new message metric value: %f lt %f nc %f smallest buffred message metric value: %f lt %f \n",new_bundle_metric_value,b->bdl_prim_->elapsed_time, ni_t, smallest_metric_in_the_queue, (*message_to_delete)->get_elapsed_time());

	if(new_bundle_metric_value > smallest_metric_in_the_queue || is_my_message(b) == 1)
	{
		// Updating the status of a bundle in the Stat Matrix
		if(bm_->da_->maintain_stat==1)
		{  
			string bundleUid;
			getBundleIdUid((*message_to_delete)->b_, bundleUid);
			statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),0,(*message_to_delete)->get_elapsed_time(),(*message_to_delete)->b_->bdl_prim_->exp_time);

		}

		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);
		BundleStore * tmp =new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)newBundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;
		// Adding the Bundle to the Stat Matrix
		if(bm_->da_->maintain_stat==1)
		{
			statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);	
		}	
	}else if(new_bundle_metric_value == smallest_metric_in_the_queue)
	{
		double r = Random::uniform(0, 1);
		if(r > 0.5)
		{
			if(bm_->da_->maintain_stat==1)
			{
				string bundleUid;
				getBundleIdUid((*message_to_delete)->b_, bundleUid);
				statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),0,(*message_to_delete)->get_elapsed_time(),(*message_to_delete)->b_->bdl_prim_->exp_time);
			}

			delete *message_to_delete;
			bundleStoreList.erase(message_to_delete);

			BundleStore * tmp =new BundleStore(bm_->da_->max_sprying_time);
			tmp->setid((char*)newBundleUid.c_str());
			tmp->set_arrival_date(TIME_IS);
			tmp->b_ = b;
			bundleStoreList.push_back(tmp);
			tmp = NULL;
			// Adding the Bundle to the Stat Matrix
			if(bm_->da_->maintain_stat==1)
			{
				statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);	

			}	
		}else{

			if(bm_->da_->maintain_stat==1)
			{
				statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
				statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)newBundleUid.c_str(),0,b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
			}
			delete b;

		}
	} 
	else 
	{ 
		if(bm_->da_->maintain_stat==1)
		{
			statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
			statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)newBundleUid.c_str(),0,b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
		}
		delete b;
	}
	return 1;

}

/** Get the message to Drop to minimize the total Avg Delay 
 * \param b The Bundle pointer.
 * \returns Return a BundleStore* which point the the BundleStore* to delete.
 * \retval BundleStore* on success.
 * \retval NULL on failure.
 */
list<BundleStore *>::iterator EpidemicRouting::get_message_with_delay_metric_reference(Bundle *b,int *i,double *metric)
{
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES-1);
	double min = 99999;
	double current_value = 0;
	int c=0;
	*i=c;
	list<BundleStore *>::iterator return_p = bundleStoreList.begin();
	list<BundleStore *>::iterator iter = bundleStoreList.begin();


	while(iter != bundleStoreList.end())
	{
		current_value=(1-(float)(exact_number_of_node_that_have_seen_it((*iter)->b_)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)exact_copys_number((*iter)->b_),2.0));
		if( current_value < min && is_my_message((*iter)->b_) != 1 )
		{ 
			min = current_value;
			return_p = iter;
			*i=c;
		}
		c++;
		iter++;
	}
	*metric=min;
	return return_p;
}

list<BundleStore *>::iterator EpidemicRouting::get_message_with_delay_metric_approximation(Bundle *b,int *i,double *metric)
{
	int nn = NUMBER_OF_NODES ;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha=MEETING_TIME*(nn - 1);
	double min=99999;
	double current_value=0;
	int c=0;
	*i=c;
	double ni_t,mi_t;
	double dd_t,dr_t;

	list<BundleStore *>::iterator return_p = bundleStoreList.begin();
	list<BundleStore *>::iterator iter = bundleStoreList.begin();


	while(iter != bundleStoreList.end())
	{
		string bundleUid;
		getBundleIdUid((*iter)->b_, bundleUid);
		statBloom_->get_stat_from_axe((*iter)->get_elapsed_time(),(char*)bundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);
		current_value = ((alpha/(nn - 1))*pow((double)dd_t,2.0))/(nn - 1 - mi_t);


		if( current_value < min && is_my_message((*iter)->b_) != 1  )
		{ 
			min = current_value;
			return_p = iter;
			*i=c;
		}
		c++;
		iter++;
	}

	*metric=min;
	return return_p;
}


void EpidemicRouting::LogForSensitivityAnalysis()
{
	make_clean();
	if(TIME_IS < 7200)
	{
		// get the meeting time
		double amt = 400;
		//if(number_of_meeting == 0) amt = 1;
		//else amt = total_meeting_samples/number_of_meeting;

		string agentId;
		agentId.assign(bm_->agentEndpoint());

		list<BundleStore *>::iterator iter = bundleStoreList.begin();


		//number of messages in the network
		int K = 400;
		int L = 70;
		int B = 20;
		double ttl =  3600;

		FILE * log_sensitivity;
		log_sensitivity = fopen("sensitivity.txt", "a");

		while(iter != bundleStoreList.end())
		{
			if(amt > 0)
			{
				double et = (*iter)->get_elapsed_time();
				double rt = ttl - et;

				double a = amt/rt;
				double b = log(rt/amt);

				double c1 = 0;

				list<BundleStore *>::iterator iter2 = bundleStoreList.begin();
				while(iter2 != bundleStoreList.end())
				{
					//get bundle R_i
					double et = (*iter2)->get_elapsed_time();
					double rt = ttl - et;

					c1 += log(rt/amt)/rt;

					iter2++;
				}

				c1 = c1 - L*B*(1/amt);

				double c2 = 0;
				list<BundleStore *>::iterator iter3 = bundleStoreList.begin();
				while(iter3 != bundleStoreList.end())
				{
					//get bundle R_i
					double et = (*iter3)->get_elapsed_time();
					double rt = ttl - et;

					c2 += 1/rt;

					iter3++;
				}

				double c = c1/c2 ;


				int n = floor(a*(b - c));

				// exact number of copies
				int en = exact_copys_number((*iter)->b_);

				string id;
				getBundleIdUid((*iter)->b_, id);
				//getBundleId((*iter)->b_)

				fprintf(log_sensitivity,"%f %s %s %i %f %i\n",TIME_IS, (char*)agentId.c_str(), (char*)id.c_str(), n,floor((*iter)->get_elapsed_time()),en);

			}
			iter++;
		}

		fclose(log_sensitivity);

		traceTimer->resched(60);
	}
}

/** Call the new drop policie to minimize the Delay (+ Source Priority) in case of congestion.
 * \param b A pointer to the Bundle to be added
 * \retval 1 on success.
 * \retval 0 on failure.
 */

int EpidemicRouting::add_and_minimize_total_delay_reference(Bundle *b)
{
	string newBundleUid;
	getBundleIdUid(b, newBundleUid);
	int position = 0;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES-1);
	double metric_from_the_queue;
	assert(s != NULL);
	double new_bundle_metric_value=(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)exact_copys_number(b),2.0));
	list<BundleStore *>::iterator message_to_delete = get_message_with_delay_metric_reference(b,&position,&metric_from_the_queue);

	if( metric_from_the_queue <= new_bundle_metric_value || is_my_message(b)==1 )
	{

		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)newBundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;
	} else 
	{
		delete b;
	}
	return 1;
}




int EpidemicRouting::add_and_minimize_total_delay_approximation(Bundle *b)
{

	string newBundleUid;
	getBundleIdUid(b, newBundleUid);
	int position=0;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES -1);
	double metric_from_the_queue;
	double ni_t,mi_t;
	double dd_t,dr_t;
	statBloom_->get_stat_from_axe(b->bdl_prim_->elapsed_time,(char*)newBundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);
	double 	new_bundle_metric_value=((alpha/(NUMBER_OF_NODES -1))*pow((double)dd_t,2.0))/(NUMBER_OF_NODES -1-mi_t);


	list<BundleStore *>::iterator message_to_delete = get_message_with_delay_metric_approximation(b,&position,&metric_from_the_queue);
	if( metric_from_the_queue <= new_bundle_metric_value || is_my_message(b) == 1 )
	{

		// Updating the status of a bundle in the Stat Matrix
		if(bm_->da_->maintain_stat==1)
		{
			string bundleUid;
			getBundleIdUid((*message_to_delete)->b_, bundleUid);
			statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),0,(*message_to_delete)->get_elapsed_time(),(*message_to_delete)->b_->bdl_prim_->exp_time);

		}

		delete *message_to_delete;
		bundleStoreList.erase(message_to_delete);

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->setid((char*)newBundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		tmp->b_ = b;
		bundleStoreList.push_back(tmp);
		tmp = NULL;
		// Adding the Bundle to the Stat Matrix
		if(bm_->da_->maintain_stat==1)
		{
			statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
		}
	} else
	{
		delete b;
	}

	return 1;
}

/** Get if a Bundle is a source copy or not
 * \param b The Bundle pointer.
 * \retval 1 if the message is a source copy.
 * \retval 0 if the message is not a source copy.
 */
int EpidemicRouting::is_my_message(Bundle *b)
{
	if(strcmp(getBundleId(b), bm_->agentEndpoint()) == 0 )
		return 1;
	else return 0;
}


/** Add a bundle in the DTNAgent Bundles main Buffer
 * \param b A pointer to the Bundle to add.
 * \retval 1 If the new Bundle was Successfuly added.
 * \retval -1 If a problem appeared when we try to add the new Bundle.
 */

int EpidemicRouting::add_bundle(Bundle *b)
{
	// Getting the reference DR metric value for bundles with elapsed tine between 1750 et 1850
	/*
if(b->bdl_prim_->elapsed_time >= 1795.0 && b->bdl_prim_->elapsed_time <= 1805.0)
{
	fprintf(stdout, "We catch it :)\n");
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double alpha = MEETING_TIME*(NUMBER_OF_NODES-1);
	double smallest_metric_in_the_queue;

	//double new_bundle_metric_value = (1/alpha)*(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*exp((-1*(exact_copys_number(b)))*((b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)/alpha));

	double new_bundle_metric_value=(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)exact_copys_number(b),2.0));

	FILE * log_convergence;
	log_convergence = fopen("ReferenceDRMetric.txt", "a");
	fprintf(log_convergence,"%f\n",new_bundle_metric_value);
	fclose(log_convergence);

}
	 */


	//if(statBloom_ != NULL)
	//	update_reference_stat_map(b->bdl_prim_->elapsed_time, b);


	if(statBloom_ == NULL)
		initialization();

	//fprintf(stdout, "\n\nAgent: %s number of buffred bundles %i\n",bm_->agentEndpoint(), number_of_buffered_bundles);
	//ShowBuffredBundles();
	//statBloom_->ShowAllMessagesStatistics();
	//fprintf(stderr, "New message ID: %s\n", getBundleId(b));
	//b->ShowDetails();

	add_new_visitor(b);

	string id;
	id.append(getBundleId(b));

	string newBundleUid;
	getBundleIdUid(b, newBundleUid);
	//fprintf(stdout, "Bundle Uid: %s\n", newBundleUid.c_str());
	AddSeenMessage(newBundleUid);

	//fprintf(stdout, "%s: Number of buffered bundles: %i\n",bm_->agentEndpoint(), number_of_buffered_bundles);

	// Adding the local Node to the Network Stat !

	if(strcmp(getBundleId(b),bm_->agentEndpoint())!=0 )
	{
		if((exist_id((char *)id.c_str(),b->bdl_prim_->u_id)==NULL && b->bdl_prim_->send_type==1) || (exist_id((char *)id.c_str(),b->bdl_prim_->u_id)==NULL && b->bdl_prim_->send_type==0)
				|| (exist_id((char *)id.c_str(),b->bdl_prim_->u_id)!=NULL && b->bdl_prim_->send_type==0))
			// check if the DTN Agent must send an block ack
			update_bta_list(b);
		bm_->da_->number_recv_bundles++;
	}



	bool newMessageDoesNotExists = (exist_id((char *)id.c_str(),b->bdl_prim_->u_id) == NULL);
	bool freePlaceExists = (free_place() == 1);

	if(newMessageDoesNotExists && freePlaceExists)
	{
		sem_wait(&buffer_sem);
		//fprintf(stderr, "newMessageDoesNotExists && freePlaceExists bm_->da_->epidemic_buffered_bundles: %i\n", bm_->da_->epidemic_buffered_bundles);

		// Adding the Bundle to the Stat Matrix
		if(bm_->da_->maintain_stat == 1 && statBloom_ != NULL)
		{
			statBloom_->add_bundle((char*)newBundleUid.c_str(),bm_->agentEndpoint(),b->bdl_prim_->elapsed_time,b->bdl_prim_->exp_time);
		}

		if(is_my_message(b)!=1)
		{
			b->bdl_prim_->hopcount += 1;
		}

		BundleStore * tmp = new BundleStore(bm_->da_->max_sprying_time);
		tmp->b_=b;
		tmp->setid((char*)newBundleUid.c_str());
		tmp->set_arrival_date(TIME_IS);
		bundleStoreList.push_back(tmp);
		tmp = NULL;
		number_of_buffered_bundles++;
		bm_->da_->epidemic_buffered_bundles = number_of_buffered_bundles;
		buffer_last_update = TIME_IS;

		//fprintf(stderr, "Message added Store Size: %i\n",bundleStoreList.size());
		sem_post(&buffer_sem);
		return 1;
	}else if(!freePlaceExists && newMessageDoesNotExists)
	{
		//fprintf(stdout," Congestion : Drop policy will be called \n");
		int r;
		sem_wait(&buffer_sem);
		buffer_last_update = TIME_IS;

		switch(bm_->da_->drop_policie)
		{
		case 1:
		{
			//fprintf(stderr, "1 Called\n");
			r = add_with_drop_tail(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 2: 
		{
			r = add_with_drop_from_front(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 3:
		{
			r = add_with_drop_youngest_message(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 4:
		{
			r = add_with_drop_oldest_message(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 5: 
		{
			r = add_and_maximize_total_delivery_rate_reference(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 6:
		{ 
			r = add_and_minimize_total_delay_reference(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 7:
		{
			r = add_and_maximize_total_delivery_rate_approximation(b);
			sem_post(&buffer_sem);
			return r;
		}	
		case 8: 
		{
			r = add_and_minimize_total_delay_approximation(b);
			sem_post(&buffer_sem);
			return r;
		}
		case 9: 
		{
			r = add_with_bin_drop_oldest(b);
			sem_post(&buffer_sem);
			return r;
		}
		default:
		{ 
			fprintf(stdout," you must select a drop policie %i!\n",bm_->da_->drop_policie);
			delete b;
			return 0;
		}
		}

		if(exist_id((char *)id.c_str(),b->bdl_prim_->u_id) == NULL)
		{
			fprintf(stdout," freePlace %i message type %i\n",free_place(), b->bdl_prim_->conversation);
			delete b;
			sem_post(&buffer_sem);
			return 0;
		}
	}else
	{
		//fprintf(stderr, "New message deleted\n");
		delete b;
		return 0;
	}
	delete b;
	return 0;
}

/** Return the number of Bundles ids in the List 
 * \param l The list of Bundles ids.
 * \param len the length of the list.
 * \retval 0 on failure.
 * \retval Int#0 on success.
 */

int EpidemicRouting::number_of_bundles(char *l,int len)
{
	if(strlen(l)>strlen("REGION1,_o21"))
	{
		int n=0;
		char recv_list_[len];
		strcpy(recv_list_,"");
		strcat(recv_list_,l);
		recv_list_[len]='\0';
		int i=0;
		while(recv_list_[i]!='\0')
		{
			if(recv_list_[i]=='-' && recv_list_[i+1]=='R'&& (strlen(strchr(l,'R'))>strlen("REGION1:_o32:1r1"))) n++;
			i++;
		}
		return n+1;
	}
	else return 0;
}

/** Send a list of Bundles  
 * \param List_to_send The list of Bundles to send.
 * \param len the length of the list to send.
 * \param From_me_ the local node address.
 * \param from_rxegion_ he local node region.
 * \param type 0 if the message is sent for the first time and 1 if its retransmitted.
 * \retval -1 on failure
 */
int EpidemicRouting::send_bundles_list(char * list_to_send, int len, nsaddr_t from_me_, char * from_region_,int type,const char *dest)
{
	Node* prev = Node::get_node_by_address(from_me_);

	int bts_= number_of_bundles(list_to_send,strlen(list_to_send));
	vector<string> bundles_to_send_;
	int dn=0;
	int i=0;
	int j=0;
	//fprintf(stdout, "Sending %i bundles\n",bts_);
	char  current_id[bm_->da_->max_uid_length +1+strlen("REGION1:_o3000000")];

	// construction de la liste des Bundles �envoyer en unicast
	while( list_to_send[i]!='\0')
	{
		if(list_to_send[i]!='-')
		{
			current_id[j]=list_to_send[i];
			j++;
		}
		else
		{
			if(strlen(current_id)>strlen("REGION1:_o32:1"))
			{ 

				current_id[j]='\0';
				//bundles_to_send_[dn] = (char *)malloc(strlen(current_id));
				//strcpy(bundles_to_send_[dn],"");
				//strcat(bundles_to_send_[dn],current_id);
				//strcat(bundles_to_send_[dn],"\0");
				bundles_to_send_.push_back(string(current_id));
				dn++;
				strcpy(current_id,""); 
				j=0;
			}
			j=0;
		}
		i++;
	}

	if(bts_!=0&& dn>0)
	{
		// Bundles scheduling
		//view_bundles_to_send_metrics(bundles_to_send_,bts_);
		if(bts_ > 1)
		{
			switch(this->bm_->da_->scheduling_type){
			case 0:
				break;
			case 1:this->arrange_bundles_to_send_according_to_reference_delivery_metric(bundles_to_send_,bts_);break;
			case 2:this->arrange_bundles_to_send_according_to_reference_delay_metric(bundles_to_send_,bts_);break;
			case 3:this->arrange_bundles_to_send_according_to_history_based_delivery_metric(bundles_to_send_,bts_);break;
			case 4:this->arrange_bundles_to_send_according_to_history_based_delay_metric(bundles_to_send_,bts_);break;
			case 5:this->arrange_bundles_to_send_according_to_flooding_based_delivery_metric(bundles_to_send_,bts_);break;
			case 6:this->arrange_bundles_to_send_according_to_flooding_based_delay_metric(bundles_to_send_,bts_);break;

			default: printf("Unkwon scheduling type !\n"); break;
			}
		}
		//view_bundles_to_send_metrics(bundles_to_send_,bts_);

		// sending first the bundles having the current node as a destination

		for(vector<string>::iterator iter = bundles_to_send_.begin(); iter != bundles_to_send_.end(); iter++)
		{
			if((*iter).length() > 0)
			{
				BundleStore *s = exist_id((char*)(*iter).c_str(),-1);
				if(s!=NULL)
				{
					if(strcmp(s->b_->bdl_dict_->record->getRecords(s->b_->bdl_prim_->dest),dest) == 0)
					{
						size_t bundlesize = (s->b_)->getDataSize();
						size_t pktsize    = sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize;
						LinkInfo * next_hop_ = (bm_->routes_)->getNextHop(from_region_,
								prev->name(),
								(s->b_)->prevhop_,  (s->b_)->recvtime_,
								pktsize, (s->b_)->bdl_prim_->cos & BDL_COS_CUST);
						if(type==1)
							s->b_->bdl_prim_->send_type=1;
						else
							s->b_->bdl_prim_->send_type=0;
						s->forwarding_number++;

						if(bm_->sendBundle( (s->b_),next_hop_)==-1)
							return -1;
						else {}
						//Deleting the bundle from the list
						(*iter).erase();
					}
					else continue;
				}

			}
		}


		// sending the rest of bundles
		for(vector<string>::iterator iter = bundles_to_send_.begin(); iter != bundles_to_send_.end(); iter++)
		{
			if((*iter).length() > 0)
			{

				BundleStore *s = exist_id((char*)(*iter).c_str(),-1);
				if(s!=NULL)
				{
					//fprintf(stdout, "Sending the bundle: %s\n",s->getid());
					size_t bundlesize = (s->b_)->getDataSize();
					size_t pktsize    = sizeof(hdr_ip) + sizeof(hdr_bdlprim) + bundlesize;
					LinkInfo * next_hop_ = (bm_->routes_)->getNextHop(from_region_,
							prev->name(),
							(s->b_)->prevhop_,  (s->b_)->recvtime_,
							pktsize, (s->b_)->bdl_prim_->cos & BDL_COS_CUST);
					if(type==1)
						s->b_->bdl_prim_->send_type = 1;
					else 
						s->b_->bdl_prim_->send_type = 0;
					s->forwarding_number++;
					if(bm_->sendBundle( (s->b_),next_hop_)==-1)
						return -1;
					else {}
				}	
				else continue;
			}
		}

		// Cleaning
		//for(int i=0;i<bts_;i++)
		//free(bundles_to_send_[i]);
		bundles_to_send_.clear();
	}
	return -1;
}


/** Initiate a new Epidemic Session when we meet another DTN Agent 
 * \param id The neighbor id.
 * \param r The neighbor region
 */
void EpidemicRouting::init_session(char *id,char *r, string source_id)
{

	//if(bm_->da_->maintain_stat == 0)
	//InitNewSession(source_id);

	char dest[30];
	strcpy(dest,"");
	strcat(dest,r);
	strcat(dest,",");
	strcat(dest,id);
	strcat(dest,":0");
	strcat(dest,"\0");
	int lengthOfLocalList = length_of_local_list(dest);
	//fprintf(stdout, "\n New session between %s and %s\n", bm_->agentEndpoint(), dest);
	//fprintf(stderr, "Initializing a new session: %s Store Size: %i\n", bm_->agentEndpoint(), bundleStoreList.size());
	//ShowBuffredBundles();
	if(lengthOfLocalList == 0)
	{
		//fprintf(stdout,"lengthOfLocalList == 0\n");
		if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1","","0",1)){
		}

	}else if (lengthOfLocalList > 0)
	{
		string strList;
		bundles_id_list(dest, strList);

		//	char data[strList.length()+1];
		//	strcpy(data,"");
		//	strcat(data, (char *)strList.c_str());
		//	strcat(data,"\0");

		char length[10];
		sprintf(length,"%i", (int)strList.length());
		//fprintf(stdout, "localList: %s\n", strList.c_str());
		//fprintf(stdout, "strList: %s \nlengthOfLocalList: %i \ndata: %s\n", strList.c_str(),lengthOfLocalList,data  );
		if(atoi(length) > MIN_MESSAGE_LENGTH)
		{
			if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char*)strList.c_str(),length,1))
			{
			}
		}
		strList.clear();
	}
}

/** Called when the node recive the message c1 from a neighbor
 * \param b A pointer to the recived message.
 * \retval 0 on success.
 * \retval -1 on failure.
 */

int EpidemicRouting::recv_c1(Bundle *b)
{
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);

	if(bm_->exist_neighbor((char *)p->name()) != -1)
	{
		char id[15];
		strcpy(id,"");
		strcat(id,p->name());
		strcat(id,"\0");

		char dest[30];
		strcpy(dest,"");
		strcat(dest,b->bdl_prim_->region);
		strcat(dest,",");
		strcat(dest,id);
		strcat(dest,":0");
		strcat(dest,"\0");

		// extraction des donn�s re�s du Bundle c1
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		memset(datab, '\0', ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);
		strncpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload, ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);

		//fprintf(stdout, "%s: rscv_c1: datab: %s\n",bm_->agentEndpoint(), datab);
		char length[100];
		string lDifference;
		list_diff(datab,strlen(datab), b->bdl_prim_->source_rule,b, lDifference);

		//fprintf(stdout, "List difference: %s Number of buffered bundles %i number of offred bundles %i\n", lDifference.c_str(), number_of_buffered_bundles, number_of_bundles(datab, strlen(datab)));
		int tl=0;

		int lenghtOfLocalList = length_of_local_list(dest);
		string strLocalList;
		bundles_id_list(dest, strLocalList);
		int lw = lDifference.length();
		if(lw != 0 && strlen(datab) != 0 && lenghtOfLocalList > 0)
		{
			// Add the list of needed Bundles that will be acked
			add_bl_toack((char*)lDifference.c_str(), dest);
			tl = lw + lenghtOfLocalList+1;
			sprintf(length,"%i",tl);

			char ll[lenghtOfLocalList];

			if(lenghtOfLocalList > 0)
			{
				strcpy(ll, (char *)strLocalList.c_str());
				strcat(ll,"\0");
			}

			char data[tl];
			strcpy(data,"");
			if(lenghtOfLocalList > 0)
				strcat(data,ll);
			strcat(data,"*");
			if(lw!=0)
			{
				bm_->da_->number_of_asked_bundles += number_of_bundles((char*)lDifference.c_str(), lDifference.length());
				strcat(data, (char*)lDifference.c_str());
			}

			strcat(data,"\0");
			delete b;
			//fprintf(stdout, "%s: rscv_c1: send datab: %s\n",bm_->agentEndpoint(), data);
			if(atoi(length) > MIN_MESSAGE_LENGTH)
			{
				if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",data,length,2))
				{}
			}

		}else
		{
			if(lenghtOfLocalList != 0)
			{
				tl= lenghtOfLocalList + 1;
				sprintf(length,"%i",tl);
				char ll[lenghtOfLocalList];
				strcpy(ll, (char*)strLocalList.c_str());
				strcat(ll,"\0");
				char data[tl];
				strcpy(data,"");
				strcat(data,ll);
				strcat(data,"*");
				strcat(data,"\0");
				//	fprintf(stdout, "%s: rscv_c1: send local list only: %s\n",bm_->agentEndpoint(), data);
				delete b;
				if(atoi(length) > MIN_MESSAGE_LENGTH)
				{
					if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",data,length,2))
					{
					}
				}
			}
			else
			{
				delete b;
				//	fprintf(stdout, "%s: rscv_c1: send nothing ll = 0\n",bm_->agentEndpoint());
				if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1","*","1",2))
				{
				}
			}
		}
		return 0;
	}
	delete b;
	return 0;
}

/** Called when the node recive the message c2 from a neighbor
 * \param b A pointer to the recived message.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
int EpidemicRouting::recv_c2(Bundle *b)
{
	Node* prev = Node::get_node_by_address(b->bdl_prim_->me);
	//fprintf(stdout, "%s: rscv_c2 from %s: number of buffred bun: %i\n",bm_->agentEndpoint(),prev->name(), number_of_buffered_bundles);
	//fprintf(stdout, "recv_c2\n");
	//ShowBuffredBundles();
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);
	if(bm_->exist_neighbor((char *)p->name())!=-1)
	{
		// extract data from the received Bundle
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		memset(datab, '\0', ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);
		strncpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload, ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);
		strcat(datab, "-");

		//fprintf(stdout, "%s: recv_c2 datab: %s\n",bm_->agentEndpoint(), datab);

		// setting  what he want
		char* s = strchr(datab,'*');
		int n=0;
		if(strchr(s,'R')==NULL) n=1;
		else n=strlen(strchr(s,'R'));
		// what he offer
		char what_he_offer[strlen(datab)-n-1];
		if((strlen(datab)-n-1)>0)
		{
			strcpy(what_he_offer,"");
			strncat(what_he_offer,datab,strlen(datab)-n-1);
			strcat(what_he_offer,"\0");
		} else strcpy( what_he_offer,"");
		// what i want
		string lDifference;
		list_diff(what_he_offer,strlen(what_he_offer),b->bdl_prim_->source_rule,b, lDifference);
		int lw = lDifference.length();
		// length of what i want
		char length[10];
		sprintf(length,"%i",lw);
		// construction de l'EID de la destination
		char id[15];
		strcpy(id,"");
		strcat(id,p->name());
		strcat(id,"\0");
		char dest[30];
		strcpy(dest,"");
		strcat(dest,b->bdl_prim_->region);
		strcat(dest,",");
		strcat(dest,id);
		strcat(dest,":0");
		strcat(dest,"\0");
		// fprintf(stdout, "List difference: %s Number of buffered bundles %i number of offred bundles %i\n", lDifference
		//.c_str(), number_of_buffered_bundles, number_of_bundles(datab, strlen(datab)));

		string source_id;
		get_source_id_of_bundle(b, source_id);

		if( lw==0)
		{
		}
		else if(lw>0)
		{
			char what_i_want [lw];
			strcpy(what_i_want,"");
			strcat(what_i_want,(char*)lDifference.c_str());
			strcat(what_i_want,"-");
			strcat(what_i_want,"\0");
			bm_->da_->number_of_asked_bundles += number_of_bundles(what_i_want,strlen(what_i_want));
			//Add the list of needed Bundles that will be acked
			add_bl_toack(what_i_want,dest);
			//fprintf(stdout, "%s: recv_c2 send what_i_want: %s\n",bm_->agentEndpoint(),what_i_want);
			if(atoi(length) > MIN_MESSAGE_LENGTH)
			{
				if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",what_i_want,length,3))
				{
				}
			}
		}

		if(strchr(s,'R')!=NULL)
		{
			char what_he_want[strlen(strchr(s,'R'))];
			strcpy(what_he_want,"");
			strncat(what_he_want,strchr(s,'R'),strlen(strchr(s,'R'))-1);
			strcat(what_he_want,"-");
			what_he_want[strlen(what_he_want)]='\0';
			// Adding a Bundles list wich will be acked
			add_list_wack(dest,what_he_want,b->bdl_prim_->region,b->bdl_prim_->me);
			//	fprintf(stdout, "%s: recv_c2 send what_he_want: %s\n",bm_->agentEndpoint(),what_he_want);

			send_bundles_list(what_he_want,strlen(what_he_want),b->bdl_prim_->me,b->bdl_prim_->region,0,this->getBundleId(b));
		}

		//RemoveSession(source_id);
		delete b;

		return 0;

	}
	delete b;
	return 0;
}


/** Called when the node recive the message c3 from a neighbor
 * \param b A pointer to the recived message.
 * \retval 0 on success.
 * \retval -1 on failure.
 */

int EpidemicRouting::recv_c3(Bundle *b)
{
	//  Node* prev = Node::get_node_by_address(b->bdl_prim_->me);
	//fprintf(stdout, "%s: rscv_c3 from %s: number of buffred bun: %i\n",bm_->agentEndpoint(),prev->name(), number_of_buffered_bundles);
	//fprintf(stdout, "recv_c3\n");
	//ShowBuffredBundles();
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);
	if(bm_->exist_neighbor((char *)p->name())!=-1)
	{
		// construction de l'EID de la destination
		char id[15];
		strcpy(id,"");
		strcat(id,p->name());
		strcat(id,"\0");
		char dest[30];
		strcpy(dest,"");
		strcat(dest,b->bdl_prim_->region);
		strcat(dest,",");
		strcat(dest,id);
		strcat(dest,":0");
		strcat(dest,"\0");

		// extract data from the received Bundle
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		memset(datab, '\0', ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);
		strncpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload, ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);

		//fprintf(stdout, "%s: recv_c3 datab: %s\n",bm_->agentEndpoint(), datab);


		// Adding a Bundles list wich will be acked
		if(strlen(datab)>0)
			add_list_wack(dest,datab,b->bdl_prim_->region,b->bdl_prim_->me);


		if(strlen(datab)>0)
		{
			//  	fprintf(stdout, "%s: recv_c3 sending list: %s\n",bm_->agentEndpoint(), datab);
			send_bundles_list(datab,strlen(datab),b->bdl_prim_->me,b->bdl_prim_->region,0,this->getBundleId(b));
		}

		delete b;
		return 0;

	}
	delete b;
	return 0;
}
/** Return the length of a bundle uid from the Bundle's List  
 * \param list The list of Bundles ids.
 * \param id the id position on the list.
 * \retval -1 on failure.
 */

int EpidemicRouting:: id_length(char * list, int id)
{
	if(id<= number_of_bundles(list,strlen(list)))
	{
		if(id ==1) return strlen(list)-strlen(strchr(list,'-'));
		else 
		{
			char *test=list;
			int i=1;
			while(i<id) {test=strchr(test,'-');i++;}
			int j=1;
			int l=0;
			while(test[j]!='-'){l++;j++;}
			return l;
		}
		return -1;
	}
	return -1;
}

/** Called to return the difference between a recived bundles List and the local List 
 * \param recv The recived Bundles List.
 * \param len The length of the recived List
 * \retval NULL on failure
 */

void EpidemicRouting::list_diff(char* recv,int len,int rule,Bundle *b, string &difference)
{

	string recv_bundles_ids_[number_of_bundles(recv,len)];

	int dn=0;
	int i=0;
	int j=0;
	int deb=0;
	int nr = number_of_bundles(recv,len);


	int deleted = 1;
	if(nr > 0)
	{      
		string current_id;

		while( recv[i]!= '\0')
		{
			if( deb==0) 
			{
				current_id.clear();
				deb=1;
				deleted=0;
			}

			if(recv[i]!='-')
			{
				current_id.push_back(recv[i]);
				j++;
			}
			else
			{
				if(current_id.length() > strlen("REGION1:_o32:1"))
				{  
					recv_bundles_ids_[dn].assign(current_id);
					deleted=1;
					dn++;
					if(dn < nr)
					{
						deleted=0;
						current_id.clear();	
					}

					j=0;
				}

				j=0;
			}

			i++;
		}
	}


	//	if(deleted==0) current_id.clear();


	//fprintf(stdout, "Received bundles\n");
	//for(int i = 0; i < dn; i++)
	//{
	//	fprintf(stdout, "bundleID: %s\n",recv_bundles_ids_[i].c_str());
	//}

	list<string> wanted_bundles_;
	//fprintf(stdout, " dn = %i\n", dn);	
	if(dn>0)
	{
		i=0;
		j=0;
		int test=0;int already_del = 0; int here = 0;
		for(i=0;i<dn;i++)
		{
			test=0;
			// Verify from the local delivre bundles list if the bundle was delivred or NOT
			string tmpId;
			get_rn((char*)recv_bundles_ids_[i].c_str(), tmpId);

			if(bm_->is_delivered((char*)tmpId.c_str(), get_u_id((char*)recv_bundles_ids_[i].c_str()))==1)
			{
				test=1;
				already_del++;
				continue;
			}

			// Verify from the antipacket list if the Bundle is already delivred or not
			if(bm_->da_->anti_packet_mechanism == 1)
			{
				if(dbl_->is_bundle_delivred((char*)recv_bundles_ids_[i].c_str()) == 1)
				{
					test=1;
					already_del++;
					continue;
				}
			}

			for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end(); iter++)
			{
				if(strcmp((*iter)->getid(),(char*)recv_bundles_ids_[i].c_str()) == 0) 
				{
					test = 1;
					here++;
					break;
				}
			}

			if(test==0) 
			{
				wanted_bundles_.push_back(recv_bundles_ids_[i]);
			}

		}




		int l_wb=0;
		//fprintf(stdout, "Wanted bundles : %i already here: %i delivered %i\n", wanted_bundles_.size(), here, already_del);
		for (list<string>::iterator iter = wanted_bundles_.begin(); iter != wanted_bundles_.end(); iter++)
			l_wb += (*iter).length();

		char  listw[l_wb+ wanted_bundles_.size()];
		strcpy(listw,"");

		for(list<string>::iterator iter = wanted_bundles_.begin(); iter != wanted_bundles_.end(); iter++)
		{
			strcat(listw, (char*)(*iter).c_str());
			strcat(listw,"-");
		}

		strcat(listw,"\0");

		difference.assign(listw);

	}else if(number_of_buffered_bundles > 0)
	{
		// copy the local list
		bundles_id_list("", difference);
	}


	// fprintf(stdout, "\nlist_diff: nbr recv bundles: %i \n Number of buffred bundles: %i\n Recv: %s \nLocal List: %s ----- \nDifference: %s\n",nr,number_of_buffered_bundles, recv,strList.c_str(),difference.c_str());
}

/** Stop a ListWaitingAck Retransmission Timer */

void ListWaitingAck::stopTimer()
{
	if(rlt_->status()!=TIMER_IDLE)
		rlt_->cancel();
}

/** Called to get the exact Number of a message copys in the network 
 * \param b The Bundle pointer.
 */
int EpidemicRouting::get_number_of_copies(Bundle *b)
{
	if(bm_->da_->reference==1)
	{
		Tcl& tcl = Tcl::instance();
		tcl.evalf("track_message_2 %s %i",getBundleId(b),b->bdl_prim_->u_id);
		const char * result = tcl.result();
		return atoi(result);
	}
	return -1;
}

/** Called to verify if a message was already deliver or not 
 * \param b The message pointer.
 * \retval 1 on success.
 * \retval 0 on failure.
 */
int EpidemicRouting::is_message_delivered(Bundle *b)
{
	if(bm_->da_->reference==1)
	{
		Tcl& tcl = Tcl::instance();
		tcl.evalf("check_dest_for_message %s %i",getBundleId(b),b->bdl_prim_->u_id);
		const char * result = tcl.result();
		return atoi(result);
	}
	return -1;
}

/** Called to get the number of nodes that have seen the message 
 * \param b The message Pointer.
 */
int EpidemicRouting::get_number_of_nodes_that_have_seen_it(Bundle *b)
{
	if(bm_->da_->reference==1)
	{
		Tcl& tcl = Tcl::instance();
		tcl.evalf("check_nodes_for_visitor %s %i",getBundleId(b),b->bdl_prim_->u_id);
		const char * result = tcl.result();
		int wit_dest = atoi(result);
		return wit_dest;
	}
	return -1;
}

/** God search for visitor */
void EpidemicRouting::god_check_for_visitor(const char * id,int u_id,int *x)
{
	if(bm_->da_->reference==1)
	{
		string idd;
		if(u_id!=-1)
		{
			idd.append(id);
			char isecret[bm_->da_->max_uid_length+1];
			strcpy(isecret,"");
			sprintf(isecret,"r%i",u_id);
			idd.append(isecret);
		}
		else
		{
			idd.append(id);
		}
		map<string, VisitorList*>::iterator iter = visitorsMap.begin();

		for(; iter != visitorsMap.end(); iter ++)
		{
			if(idd.compare(iter->first) == 0)
			{
				*x=1;
				break;
			}
		}

		if(iter == visitorsMap.end()) *x=0;

		if(this->dbl_->is_bundle_delivred((char *)idd.c_str())==1)
			*x=1;
	}
}

/** God search for message */

void EpidemicRouting::god_check_for_message(const char * id,int u_id,int *x)
{
	string idd;	
	if(u_id != -1)
	{
		idd.append(id);
		char isecret[bm_->da_->max_uid_length+1];
		strcpy(isecret,"");
		sprintf(isecret,"r%i",u_id);
		idd.append(isecret);
	}
	else
	{
		idd.append(id);
	}
	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	for(;iter != bundleStoreList.end();iter++)
	{
		if(strcmp((char *)idd.c_str(), (*iter)->getid())==0) 
		{
			*x=1;
			break;
		}
	}

	if(iter == bundleStoreList.end()) *x=0;

}


/** Verify if the Bundle is a new visitor or not
 * \param b The message pointer.
 * \retval 0 If the bundle is a new visitor.
 * \retval 1 If the message is not a new visitor.
 */

int EpidemicRouting::is_it_a_newVisitor(Bundle *b)
{
	if(bm_->da_->reference==1)
	{
		string newBundleUid;
		getBundleIdUid(b, newBundleUid);
		if(visitorsMap.find((char*)newBundleUid.c_str()) == visitorsMap.end())
			return 0;
		else return 1;
	}
	return -1;
}

/** add a new visitor if he do not exit 
 * \param b The pointer to the visitor to add
 */

int EpidemicRouting::add_new_visitor(Bundle *b)
{
	if(bm_->da_->reference==1)
	{
		if(is_it_a_newVisitor(b) == 1)
		{
			return 0;
		}
		else {
			// adding a new visitor to the map
			VisitorList * tmp =new VisitorList();
			string bundleUid;
			getBundleIdUid(b, bundleUid);
			tmp->setid((char*)bundleUid.c_str());
			tmp->set_arrival_date(TIME_IS);
			tmp->set_elapsed_time(b->bdl_prim_->elapsed_time);
			tmp->set_ttl(2*b->bdl_prim_->exp_time);
			tmp->hopcount = b->bdl_prim_->hopcount;
			visitorsMap[string(tmp->getid())] = tmp;
			tmp = NULL;
			return 1;
		}
		return 0;
	}
	return -1;
}


void VisitorList::set_elapsed_time(double e)
{
	elapsed_time=e;
}

void VisitorList::update_elapsed_time(double d)
{
	elapsed_time+= (d-time_stamp);
	time_stamp = d;
}

double  VisitorList::get_elapsed_time()
{
	return elapsed_time;
}

void VisitorList::setid(char *s)
{
	id.assign(s);
}

void VisitorList::set_ttl(double e)
{
	ttl=e;
}

VisitorList::VisitorList ()
{
	time_stamp=TIME_IS;
	ttl=0;
	elapsed_time=0;
	time_stamp=0;
}

/** BundleStore Destructor **/

VisitorList::~VisitorList()
{
}

/** Cleaning the visitor List*/

void EpidemicRouting::clean_visitor_list()
{
	if(bm_->da_->reference==1)
	{
		map<string, VisitorList *>::iterator iter = visitorsMap.begin();
		while(iter != visitorsMap.end())
		{
			(iter->second)->update_elapsed_time(TIME_IS);
			if((iter->second)->get_elapsed_time() > (iter->second)->get_ttl())
			{
				delete iter->second;
				visitorsMap.erase(iter);
			}
			iter++;
		}
	}
}

/** Delete From the Buffer Delivred Bundles when the DTN node recive a new anti packet
 * 
 */
void EpidemicRouting::delete_delivred_bundles()
{
	if(bm_->da_->anti_packet_mechanism==1 )
	{	
		sem_wait(&buffer_sem);
		if(number_of_buffered_bundles > 0 )
		{

			list<BundleStore *>::iterator iter = bundleStoreList.begin();
			while(iter != bundleStoreList.end())
			{
				string bundleUid;
				getBundleIdUid((*iter)->b_, bundleUid);

				if( dbl_->is_bundle_delivred((char*)bundleUid.c_str()) == 1 )
				{	
					buffer_last_update = TIME_IS;
					// Bundle must be deleted

					// Updating the status of a bundle in the Stat Matrix
					//if(bm_->da_->maintain_stat==1)
					//statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),0,(*iter)->get_elapsed_time(),(*iter)->b_->bdl_prim_->exp_time);

					delete *iter;
					iter  = bundleStoreList.erase(iter);
					number_of_buffered_bundles --;
					bm_->da_->epidemic_buffered_bundles = number_of_buffered_bundles;
					continue;
				}
				iter++;
			}
		}
		sem_post(&buffer_sem);

	}

}

/** Return the DTN Node id (source of the Bundle)
 */
void EpidemicRouting::get_source_id_of_bundle(Bundle *b, string &sourceId){

	Node* p = Node::get_node_by_address(b->bdl_prim_->me);
	char id[15];
	strcpy(id,"");
	strcat(id,p->name());
	strcat(id,"\0");
	char source[sizeof(char)*30];
	strcpy(source,"");
	strcat(source,b->bdl_prim_->region);
	strcat(source,",");
	strcat(source,id);
	strcat(source,":0");
	strcat(source,"\0");
	sourceId.assign(source);
}

/** Called when a new Stat Matrix was recived
 */
int EpidemicRouting::stat_matrix_recived_first(Bundle *b)
{
	// Sending back the  stat
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);

	if(statBloom_ != NULL && bm_->exist_neighbor((char *)p->name()) != -1)
	{


		char id[15];
		strcpy(id,"");
		strcat(id, p->name());
		strcat(id,"\0");


		string source_id;
		get_source_id_of_bundle(b, source_id);

		char region[8];
		strcpy(region, b->bdl_prim_->region);	

		send_stat_matrix((char*)source_id.c_str(), bm_->nm_->get_neighbor_arrival_date(id, region), 1);

		// Extract Data recived on the FAntipacket
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		strcpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload);
		datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len]='\0';

		//fprintf(stdout, "Received stat matrix: %s\n\n",datab);
		statBloom_->update_network_stat(datab);
	}

	delete b;
	return 0;
}

/** Called when a new Stat Matrix was recived
 */
int EpidemicRouting::stat_matrix_recived_last(Bundle *b)
{
	if(statBloom_ != NULL)
	{
		// Extract Data recived on the FAntipacket
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		memset(datab, '\0', ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);
		strncpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload, ((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len);

		//fprintf(stdout, "Received stat matrix: %s\n\n",datab);
		statBloom_->update_network_stat(datab);
	}

	delete b;
	return 0;
}
int EpidemicRouting::stat_matrix_recived_versions_based_reverse_schema_first(Bundle *b)
{
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);

	if(bm_->exist_neighbor((char *)p->name()) != -1)
	{
		// Sending back the  stat
		char id[15];
		strcpy(id,"");
		strcat(id, p->name());
		strcat(id,"\0");


		string source_id;
		get_source_id_of_bundle(b, source_id);

		char region[8];
		strcpy(region, b->bdl_prim_->region);	

		send_stat_matrix((char*)source_id.c_str(), bm_->nm_->get_neighbor_arrival_date(id, region), 1);


		// Extract Data recived on the FAntipacket
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		strcpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload);
		datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len]='\0';
		UpdateLocalMessagesStat();
		string ids;
		ids.append(datab);
		string stat_to_send;
		statBloom_->get_stat_to_send_based_on_versions(ids, stat_to_send);

		char length[10];
		sprintf(length,"%i", (int)stat_to_send.length());

		delete b;
		if(stat_to_send.length() > MIN_MESSAGE_LENGTH)
		{
			fprintf(stdout, "\n\n\nStat to send: %s\n", stat_to_send.c_str());
			if(!bm_->newBundle(bm_->agentEndpoint(), (char*)source_id.c_str(),bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *)stat_to_send.c_str(), length, 7))
			{
			}
		}
		stat_to_send.clear();
	}


	return 0;
}

int EpidemicRouting::stat_matrix_recived_versions_based_reverse_schema_last(Bundle *b)
{
	Node* p = Node::get_node_by_address(b->bdl_prim_->me);
	if(statBloom_ == NULL)
		initialization();

	if(bm_->exist_neighbor((char *)p->name()) != -1)
	{

		// Extract Data recived on the FAntipacket
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		strcpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload);
		datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len]='\0';
		UpdateLocalMessagesStat();
		string ids;
		ids.append(datab);
		string stat_to_send;
		statBloom_->get_stat_to_send_based_on_versions(ids, stat_to_send);

		char length[10];
		sprintf(length,"%i", (int)stat_to_send.length());

		string source_id;
		get_source_id_of_bundle(b, source_id);
		delete b;
		if(stat_to_send.length() > MIN_MESSAGE_LENGTH)
		{
			fprintf(stdout, "\n\n\nStat to send: %s\n", stat_to_send.c_str());
			if(!bm_->newBundle(bm_->agentEndpoint(), (char*)source_id.c_str(),bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *)stat_to_send.c_str(), length, 7))
			{
			}
		}
		stat_to_send.clear();
	}


	return 0;
}

void EpidemicRouting::send_stat_matrix(char * dest, double neighbor_last_meeting, int sequence)
{	
	if(bm_->da_->maintain_stat==1 ) 
	{	
		switch(bm_->da_->stat_sprying_policy)
		{
		case 0:
		{
			if(sequence == 0)
			{
				if(neighbor_last_meeting < statBloom_->stat_last_update)
				{

					// Pushing Strategy
					string localList;

					//statBloom_->update_node_last_meeting_time(dest);
					// Updating the local bundles Elapsed time
					// Updating stats:
					UpdateLocalMessagesStat();
					statBloom_->get_stat_to_send(localList);
					//fprintf(stdout, "Local list: %s\n\n\n", localList.c_str());
					//fprintf(statSize, "%f %i\n",TIME_IS, localList.length());

					char length[10];
					sprintf(length,"%i", (int)localList.length());
					if(atoi(length) > MIN_MESSAGE_LENGTH)
					{
						if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *) localList.c_str(), length, 6))
						{
						}
					}
					localList.clear();

				}
			}else if(sequence == 1)
			{
				// Pushing Strategy
				string localList;

				//statBloom_->update_node_last_meeting_time(dest);
				// Updating the local bundles Elapsed time
				// Updating stats:
				UpdateLocalMessagesStat();
				statBloom_->get_stat_to_send(localList);
				//fprintf(stdout, "Local list: %s\n\n\n", localList.c_str());
				//fprintf(statSize, "%f %i\n",TIME_IS, localList.length());
				char length[10];
				sprintf(length,"%i", (int)localList.length());
				if(atoi(length) > MIN_MESSAGE_LENGTH)
				{
					if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *) localList.c_str(), length, 7))
					{
					}
				}
				localList.clear();


			}

		}
		break;
		case 2:
		{	if(sequence == 0 )
		{
			//fprintf(stdout, "Test statBloom_->stat_last_update %f %f\n",statBloom_->stat_last_update, neighbor_last_meeting + 10);

			if(neighbor_last_meeting  < statBloom_->stat_last_update)
			{
				//fprintf(stdout, "OK statBloom_->stat_last_update %f %f\n",statBloom_->stat_last_update, neighbor_last_meeting + 10);

				string stat_request;
				get_messages_ids_based_on_version_for_stat_request(stat_request);

				fprintf(stdout, "\n\n\nget_messages_ids_based_on_version_for_stat_request: %s\n", stat_request.c_str());

				char length[10];
				sprintf(length,"%i", (int)stat_request.length());

				if(atoi(length) > MIN_MESSAGE_LENGTH)
				{
					//fprintf(stdout, "Sending stat request : %s\n\n", stat_request.c_str());
					if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *) stat_request.c_str(), length, 8))
					{
					}
				}
				stat_request.clear();
			}

		}else if(sequence == 1)
		{
			string stat_request;
			get_messages_ids_based_on_version_for_stat_request(stat_request);

			char length[10];
			sprintf(length,"%i", (int)stat_request.length());

			if(atoi(length) > MIN_MESSAGE_LENGTH)
			{
				//fprintf(stdout, "Sending stat request : %s\n\n", stat_request.c_str());
				if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char *) stat_request.c_str(), length, 9))
				{
				}
			}
			stat_request.clear();

		}
		}
		break;
		default:
			fprintf(stdout, "Invalid Sprying policy: %i!\n", bm_->da_->stat_sprying_policy);exit(-1);	
		};



	}
}

void EpidemicRouting::UpdateLocalMessagesStat()
{
	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	while(iter != bundleStoreList.end())
	{ 
		string bundleUid;

		this->getBundleIdUid((*iter)->b_, bundleUid);
		(*iter)->update_elapsed_time(TIME_IS);
		//statBloom_->add_bundle((char*)bundleUid.c_str(),bm_->agentEndpoint(),(*iter)->get_elapsed_time(),(*iter)->b_->bdl_prim_->exp_time);

		statBloom_->update_bundle_status(bm_->agentEndpoint(),(char*)bundleUid.c_str(),1,(*iter)->get_elapsed_time(),(*iter)->b_->bdl_prim_->exp_time);

		iter++;
	}

}
/**Called when the DTN Agent recive a new Anti Packet (conversation == 5)
 * \param b : A pointer to the Anti Packet recived by the DTN Agent 
 */
int EpidemicRouting::recv_anti_packet(Bundle *b)
{

	if(bm_->da_->anti_packet_mechanism == 1 )
	{
		// Extract Data recived on the FAntipacket
		char datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len];
		strcpy(datab,((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->payload);
		datab[((hdr_bdlpyld *)b->bdl_dict_->next_hdr_p)->len]='\0';

		// Updating Delivred Bundles List
		dbl_->update_delivred_bundles_list(datab, strlen(datab));
		//fprintf(stdout, "Antipacket message received\n");

		// Cleaning the Main Buffer From Delivred Bundles if the list was really updated 
		if(dbl_->updated == 1)
		{
			//	fprintf(stdout, "Cleaning the local buffer from delivred bundles\n");
			//	fprintf(stdout,"SB: %i\n", bundleStoreList.size());
			delete_delivred_bundles();
			//	fprintf(stdout,"SA: %i\n", bundleStoreList.size());
			dbl_->updated = 0;
		}

		delete b;
		return 0;
	}
	delete b;
	return 0;
}


void EpidemicRouting:: send_antipacket_matrix(char * dest)
{			
	if(bm_->da_->anti_packet_mechanism ==1 )
	{
		if(dbl_->get_number_of_delivred_bundles() > 0)
		{
			string list;
			dbl_->get_delivred_bundles_list(list);
			list.append("-");

			//char data_to_send[list.length()+2];
			//strcpy(data_to_send,"");
			//strcat(data_to_send, (char*)list.c_str());
			//strcat(data_to_send,"-");
			//strcat(data_to_send,"\0");

			//fprintf(stdout, "Sending the antipacket: %s\n", data_to_send);

			char length[10];
			sprintf(length,"%i", (int)list.length());
			if(atoi(length) > MIN_MESSAGE_LENGTH)
			{
				if(!bm_->newBundle(bm_->agentEndpoint(),dest,bm_->agentEndpoint(),"NORMAL","NONE","100","bindM1",(char*)list.c_str(),length,5))
				{
				}
			} 
			list.clear();
		}

		bm_->nm_->antipacket_update = TIME_IS;
	}
}

void EpidemicRouting::test_method_1()
{
	list<BundleStore *>::iterator iter = bundleStoreList.begin();
	while(iter != bundleStoreList.end())
	{
		string bundleUid;
		this->getBundleIdUid((*iter)->b_, bundleUid);
		fprintf(stdout," Bundle Id %s\n", (char*)bundleUid.c_str());
		iter++;
	}

}

// Look if the node it meet is a destination for one of the stored ;essages in his buffer 
int EpidemicRouting::is_it_a_destination_for_one_of_my_messages( string node_id)
{
	char * current_message_d;
	for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end();iter++)
	{
		current_message_d = (*iter)->b_->bdl_dict_->record->getRecords((*iter)->b_->bdl_prim_->dest);
		if(strcmp(node_id.c_str(),current_message_d)==0)
		{
			return 1;
		}
	}
	return 0;
}

double EpidemicRouting::get_delivery_rate_reference_metric(Bundle *b)
{
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES-1);
	return (1/alpha)*(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*exp((-1*(exact_copys_number(b)))*((b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)/alpha));
}

double EpidemicRouting::get_delivery_delay_reference_metric(Bundle *b){

	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES-1);
	return (1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)exact_copys_number(b),2.0));

}

double EpidemicRouting::get_delivery_rate_history_based_metric(Bundle *b)
{
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES -1);
	double ni_t,mi_t;
	double dd_t,dr_t;
	string bundleUid;
	this->getBundleIdUid(b, bundleUid);
	statBloom_->get_stat_from_axe(b->bdl_prim_->elapsed_time,(char*)bundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);

	return (1/(alpha))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*dr_t;

}


double EpidemicRouting::get_delivery_delay_history_based_metric(Bundle *b){

	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES -1);
	double ni_t,mi_t;
	double dd_t,dr_t;
	string bundleUid;
	this->getBundleIdUid(b, bundleUid);

	statBloom_->get_stat_from_axe(b->bdl_prim_->elapsed_time,(char*)bundleUid.c_str(),&ni_t,&mi_t,&dd_t,&dr_t);
	return ((alpha/(NUMBER_OF_NODES -1))*(pow((double)dd_t,2.0)/NUMBER_OF_NODES -1-mi_t));
}

double EpidemicRouting::get_delivery_rate_flooding_based_metric(Bundle *b)
{
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES-1);
	string bundleUid;
	this->getBundleIdUid(b, bundleUid);

	return (1/alpha)*(1-(float)(statBloom_->get_number_of_nodes_that_have_seen_it((char*)bundleUid.c_str(), b->bdl_prim_->elapsed_time) /(NUMBER_OF_NODES-1)))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*exp((-1*(statBloom_->get_number_of_copies((char*)bundleUid.c_str(), b->bdl_prim_->elapsed_time)))*((b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)/alpha));

}

double EpidemicRouting::get_delivery_delay_flooding_based_metric(Bundle *b){

	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	double alpha=MEETING_TIME*(NUMBER_OF_NODES-1);
	string bundleUid;
	this->getBundleIdUid(b, bundleUid);

	return (1-(float)(statBloom_->get_number_of_nodes_that_have_seen_it((char*)bundleUid.c_str(),b->bdl_prim_->elapsed_time)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)statBloom_->get_number_of_copies((char*)bundleUid.c_str(), b->bdl_prim_->elapsed_time),2.0));

}


void EpidemicRouting::view_bundles_to_send_metrics(char ** bundles_table,int table_length)
{
	BundleStore *c_s=NULL;
	double c_m;
	for(int i=0;i<table_length;i++)
	{
		c_s=exist_id(bundles_table[i],-1);
		if(c_s!=NULL)
		{
			c_m=this->get_delivery_rate_reference_metric(c_s->b_);
			printf("bundles_table[%i]= %f\n",i,c_m);
		}

	}
}

void EpidemicRouting::arrange_bundles_to_send_according_to_reference_delivery_metric(vector<string> & bundles_table, int table_length)
{

	BundleStore *m_s = NULL;
	BundleStore *c_s = NULL;
	double m_dr = 0;
	double c_dr = 0;

	for(int i=0; i < table_length; i++)
	{
		m_s = exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s != NULL)
		{
			m_dr = this->get_delivery_rate_reference_metric(m_s->b_);
			for(int j=i+1; j < table_length; j++)
			{
				c_s = exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s != NULL)
				{
					c_dr = this->get_delivery_rate_reference_metric(c_s->b_);
					if(c_dr > m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp);
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}
}


void EpidemicRouting::arrange_bundles_to_send_according_to_reference_delay_metric(vector<string> &  bundles_table, int table_length){


	BundleStore *m_s=NULL;
	BundleStore *c_s=NULL;
	double m_dr=0;
	double c_dr=0;

	for(int i=0;i<table_length;i++)
	{
		m_s=exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s!=NULL)
		{
			m_dr=this->get_delivery_delay_reference_metric(m_s->b_);
			for(int j=i+1;j<table_length;j++)
			{
				c_s=exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s!=NULL)
				{
					c_dr=this->get_delivery_delay_reference_metric(c_s->b_);
					if(c_dr>m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp.c_str());
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}

}



void EpidemicRouting::arrange_bundles_to_send_according_to_history_based_delivery_metric(vector<string> &  bundles_table, int table_length){


	BundleStore *m_s=NULL;
	BundleStore *c_s=NULL;
	double m_dr=0;
	double c_dr=0;

	for(int i=0;i<table_length;i++)
	{
		m_s=exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s!=NULL)
		{
			m_dr=this->get_delivery_rate_history_based_metric(m_s->b_);
			for(int j=i+1;j<table_length;j++)
			{
				c_s=exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s!=NULL)
				{
					c_dr=this->get_delivery_rate_history_based_metric(c_s->b_);
					if(c_dr>m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp.c_str());
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}

}


void EpidemicRouting::arrange_bundles_to_send_according_to_history_based_delay_metric(vector<string> & bundles_table, int table_length){


	BundleStore *m_s=NULL;
	BundleStore *c_s=NULL;
	double m_dr=0;
	double c_dr=0;

	for(int i=0;i<table_length;i++)
	{
		m_s=exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s!=NULL)
		{
			m_dr=this->get_delivery_delay_history_based_metric(m_s->b_);
			for(int j=i+1;j<table_length;j++)
			{
				c_s=exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s!=NULL)
				{
					c_dr=this->get_delivery_delay_history_based_metric(c_s->b_);
					if(c_dr>m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp.c_str());
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}

}

void EpidemicRouting::arrange_bundles_to_send_according_to_flooding_based_delivery_metric(vector<string> & bundles_table, int table_length){


	BundleStore *m_s=NULL;
	BundleStore *c_s=NULL;
	double m_dr=0;
	double c_dr=0;

	for(int i=0;i<table_length;i++)
	{
		m_s=exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s!=NULL)
		{
			m_dr=this->get_delivery_rate_flooding_based_metric(m_s->b_);
			for(int j=i+1;j<table_length;j++)
			{
				c_s=exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s!=NULL)
				{
					c_dr=this->get_delivery_rate_flooding_based_metric(c_s->b_);
					if(c_dr>m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp.c_str());
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}

}


void EpidemicRouting::arrange_bundles_to_send_according_to_flooding_based_delay_metric(vector<string> &  bundles_table, int table_length)
{

	BundleStore *m_s=NULL;
	BundleStore *c_s=NULL;
	double m_dr=0;
	double c_dr=0;

	for(int i=0;i<table_length;i++)
	{
		m_s=exist_id((char*)bundles_table[i].c_str(),-1);
		if(m_s!=NULL)
		{
			m_dr=this->get_delivery_delay_flooding_based_metric(m_s->b_);
			for(int j=i+1;j<table_length;j++)
			{
				c_s=exist_id((char*)bundles_table[j].c_str(),-1);
				if(c_s!=NULL)
				{
					c_dr=this->get_delivery_delay_flooding_based_metric(c_s->b_);
					if(c_dr>m_dr)
					{
						string temp;
						temp.assign(bundles_table[i]);
						bundles_table[i].assign(bundles_table[j]);
						bundles_table[j].assign(temp.c_str());
					}
				} else printf("c_dr = NULL\n");
			}
		} else printf(" m_s == NULL\n");
	}

}

void EpidemicRouting::ShowBuffredBundles()
{
	fprintf(stdout, "Agent: %s Number of buffred messages: %i\n", bm_->agentEndpoint(), (int)bundleStoreList.size());
	for(list<BundleStore *>::iterator iter = bundleStoreList.begin(); iter != bundleStoreList.end();iter++)
	{
		if((*iter)->b_ == NULL) fprintf(stdout, "Bundle equalt to NULL");
		string newBundleUid;
		getBundleIdUid((*iter)->b_, newBundleUid);
		fprintf(stdout,"Bundle Id: %s\n", newBundleUid.c_str());
		//(*iter)->b_->ShowDetails();
	}
}

void BundleStore::ShowDetails()
{
	fprintf(stdout,"Bundle Id: %s Elapsed Time: %f Source: %s Dest %s \n", id.c_str(), get_elapsed_time(), b_->bdl_dict_->record->getRecords(b_->bdl_prim_->src), b_->bdl_dict_->record->getRecords(b_->bdl_prim_->dest)); 
}


void EpidemicRouting::AddSeenMessage(string id)
{
	if(DidISeeMessage(id) == false)	
		bundlesSeen.push_back(id);

}

bool EpidemicRouting::DidISeeMessage(string &id)
{
	bool exist = false;
	for(list<string>::iterator iter= bundlesSeen.begin(); iter != bundlesSeen.end(); iter++)
	{
		if((*iter).compare(id) == 0)
		{
			return true;
		}
	}
	return exist;
}

// Returns the set of couples (msgId, NodeId, Version)
void EpidemicRouting::get_messages_ids_based_on_version_for_stat_request(string & msg)
{

	int max = 0;
	list<string>::iterator iter= bundlesSeen.begin();
	while(iter != bundlesSeen.end() && max < bm_->da_->max_number_of_stat_messages)
	{
		if(!statBloom_->IsMessageValid(*iter))
		{
			string desc;
			statBloom_->get_message_nodes_couples(*iter, desc);
			if(iter != bundlesSeen.begin() && desc.length() > 0)
				msg.append(sizeof(char), '\\');

			msg.append(desc);
			max++;
			iter++;
		}else
		{
			// remove it so it will not be considered for the next request
			// a new younger message will be added
			iter = bundlesSeen.erase(iter);
		}
	}
	//fprintf(stdout, "NumberOfRequestedMessgaes at %f %i Max: %i\n",TIME_IS, max,bm_->da_->max_number_of_stat_messages);

	msg.append(sizeof(char), '#');
}



// returns only the set of messages ids
void EpidemicRouting::get_messages_ids_for_stat_request(string & msg)
{
	for(list<string>::iterator iter= bundlesSeen.begin(); iter != bundlesSeen.end(); iter++)
	{
		if(iter != bundlesSeen.begin())
			msg.append(sizeof(char), '*');
		msg.append(*iter);
	}
}

void EpidemicRouting::get_map_ids_for_stat_request(string & msg, map<string, int> &map)
{
	size_t pos = msg.find_first_of('*');
	while(pos != string::npos)
	{
		string tmp = msg.substr(0, pos);
		map[tmp] = 1;	
		msg = msg.substr(pos+1);
		pos = msg.find_first_of('*');
	}
	map[msg] = 1;
}


/*
bool EpidemicRouting::IsSessionActive(string peer)
{

	map<string, EpidemicSession*>::iterator iter = sessionsMap.find(peer);
	if(iter != sessionsMap.end())
		return true;
	else return false;
}
void EpidemicRouting::InitNewSession(string peer)
{
	if(!IsSessionActive(peer))
	{
		sessionsMap[peer] = new EpidemicSession(peer); 
	}

}
 */
/*
void EpidemicRouting::RemoveSession(string peer)
{
	map<string, EpidemicSession*>::iterator iter = sessionsMap.find(peer);
	if(iter != sessionsMap.end())
	{
		delete iter->second;
		sessionsMap.erase(iter);
	}	
}
 */


int EpidemicRouting::LogDrandDdMetrics()
{
	double amtnu;
	if(number_of_meeting_not_unique == 0) amtnu = 1;
	else amtnu = total_meeting_samples_not_unique/number_of_meeting_not_unique;
	double amt;
	if(number_of_meeting == 0) amt = 1;
	else amt = total_meeting_samples/number_of_meeting;
	fprintf(stdout, "average meeting time: %f %f\n", amt,amtnu);
	//save_avg_reference_stat();


	// Logging the dr and dd metrics for each time bin
	if(bm_->da_->maintain_stat==1 && statBloom_ != NULL)
	{
		fprintf(stdout, " Current Number of messages %i\n", statBloom_->GetNumberOfMessages());
		//statBloom_->LogAvgStatistics();
		double alpha = MEETING_TIME*(NUMBER_OF_NODES  - 1);
		if(alpha <= 0) return TCL_OK;
		int ttl  = atoi(bm_->da_->getlifespan());
		//fprintf(stdout, "Axe length: %i\n", statBloom_->axe_length);
		int iTtl = (int)ttl;

		double totalErrorDr = 0;
		double totalErrorDd = 0;	

		for(int i = 0;i < iTtl; i+=50)
		{	
			double ni_t, mi_t;
			double dd_t, dr_t;
			double et;

			statBloom_->get_stat_from_axe((double)i, NULL, &ni_t, &mi_t, &dd_t, &dr_t);
			et = (double)i;
			{	
				double metric_dr = (1 / (alpha)) * (ttl - et) * dr_t;
				int nn= NUMBER_OF_NODES ;
				double metric_dd = 0;
				if(nn > 1 + mi_t)
				{
					metric_dd = (double)((pow(dd_t, 2) *alpha) / ( (nn - 1) * (nn - 1 - mi_t)));

					if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0)
					{
						// calcul de l'erreur
						if(ref_stat_map[(int)et].avgDr > 0)
							totalErrorDr += pow(ref_stat_map[(int)et].avgDr - metric_dr, 2)/pow(ref_stat_map[(int)et].avgDr, 2);
						if(ref_stat_map[(int)et].avgDd > 0)
							totalErrorDd += pow(ref_stat_map[(int)et].avgDd - metric_dd, 2)/pow(ref_stat_map[(int)et].avgDd, 2);
					}

					fprintf(log_meetric,"bin_index %i ni_t %f mi_t %f dr: %f dd: %f et %f dd_up %f dd_down %f refDr %f refDD %f\n",i, ni_t, mi_t, metric_dr, metric_dd, et,pow(dd_t, 2), 1/((1 / (alpha))* (nn - 1) * (nn - 1 - mi_t) ), ref_stat_map[(int)et].avgDr, ref_stat_map[(int)et].avgDd);
				}	


			}

		}

		//statBloom_->ShowAllMessagesStatistics(log_meetric);
		//statBloom_->ShowAvgStatistics();

		return TCL_OK;
	} else return TCL_OK;

}

void EpidemicRouting::save_sum_square_error(double time)
{
	if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0)
	{
		double amtnu;
		if(number_of_meeting_not_unique == 0) amtnu = 1;
		else amtnu = total_meeting_samples_not_unique/number_of_meeting_not_unique;

		double amt;
		if(number_of_meeting == 0) amt = 1;
		else amt = total_meeting_samples/number_of_meeting;

		int ttl  = atoi(bm_->da_->getlifespan());

		double alpha = MEETING_TIME*(NUMBER_OF_NODES - 1);


		int iTtl = (int)ttl;

		double totalErrorDr = 0;

		for(int i = 0;i < iTtl; i+=50)
		{	
			double ni_t= 0, mi_t =0;
			double dd_t = 0, dr_t =0;
			double et =0;

			statBloom_->get_stat_from_axe((double)i, NULL, &ni_t, &mi_t, &dd_t, &dr_t);
			et = (double)i;

			{	
				double metric_dr = (1 / (alpha)) * (ttl - et) * dr_t;
				int nn= NUMBER_OF_NODES ;

				double metric_dd = 0;

				if(nn > 1 + mi_t)
				{
					metric_dd = (double)((pow(dd_t, 2) *alpha) / ( (nn - 1) * (nn - 1 - mi_t)));

					if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0)
					{
						double toAdd = 0;
						// calcul de l'erreur
						if(refTab[(int)et/50] > 0)
						{
							toAdd = pow((refTab[(int)et/100] - metric_dr)/refTab[(int)et/100], 2);
						}
						else 
						{
							toAdd = pow((refTab[(int)et/100] - metric_dr), 2);
						}		

						//fprintf(stdout, "refTab[%i] : %f metric_dr: %f toAdd: %f\n", (int)et/50, refTab[(int)et/50], metric_dr, toAdd);

						totalErrorDr += toAdd;

						//totalErrorDd += pow((refTab[(int)et/50] - metric_dd)/refTab[(int)et/50], 2);
					}
					//fprintf(log_meetric,"bin_index %i ni_t %f mi_t %f dr: %f dd: %f et %f dd_up %f dd_down %f refDr %f\n",i, ni_t, mi_t, metric_dr, metric_dd, et,pow(dd_t, 2), 1/((1 / (alpha))* (nn - 1) * (nn - 1 - mi_t) ), refTab[(int)et/50]);
				}	


			}

		}



		FILE * log_error;
		string refFile;
		refFile.append("Error");
		refFile.append(bm_->agentEndpoint());
		refFile.append("*.txt");
		log_error = fopen(refFile.c_str(), "a");
		if(totalErrorDr > 0)
		{
			fprintf(log_error, " time: %f totalErrorDr: %f Avg error DR: %f  \n",time, totalErrorDr, totalErrorDr/72);
			fprintf(stdout, " time: %f totalErrorDr: %f Avg error DR: %f nbr Buffered Bundles: %i\n",time, totalErrorDr, totalErrorDr/72,number_of_buffered_bundles);
		}else
		{
			fprintf(log_error, " time: %f totalErrorDr: 0 Avg error DR: 1  \n",time);
			fprintf(stdout, " time: %f totalErrorDr: 0 Avg error DR: 1\n",time);

		}
		fclose(log_error);

	}
}

int EpidemicRouting::get_closest_et(int et)
{
	int last = 0;
	for(map<int , stat_node>::iterator iter = ref_stat_map.begin();iter != ref_stat_map.end();iter++)
	{

		if(et <= iter->first && et >= last )
		{
			if(et == last)
			{
				//fprintf(stdout, " I %i %i et %i R %i\n", last, iter->first, et, last);
				return last;
			}
			else if(et == iter->first)
			{
				//fprintf(stdout, " I %i %i et %i R %i\n", last, iter->first, et, iter->first);
				return iter->first;
			}
			//fprintf(stdout, " I %i %i et %i R %i\n", last, iter->first, et, last);
		}	
		last = iter->first;
	}
	return last;

}
void EpidemicRouting::save_avg_reference_stat()
{
	if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0 || strcmp("REGION1,_o607:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o175:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o1007:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o831:0", bm_->agentEndpoint()) == 0 )
	{
		double amtnu;
		if(number_of_meeting_not_unique == 0) amtnu = 1;
		else amtnu = total_meeting_samples_not_unique/number_of_meeting_not_unique;
		double amt;
		if(number_of_meeting == 0) amt = 1;
		else amt = total_meeting_samples/number_of_meeting;
		int ttl  = atoi(bm_->da_->getlifespan());
		double alpha = MEETING_TIME*(NUMBER_OF_NODES - 1);

		//fprintf(stdout, "--------------------------------------------------\n");

		FILE * log_convergence;
		string refFile;
		refFile.append("refStat");
		refFile.append(bm_->agentEndpoint());
		refFile.append("*.txt");
		log_convergence = fopen(refFile.c_str(), "w");


		for(map<int , stat_node>::iterator iter = ref_stat_map.begin();iter != ref_stat_map.end();iter++)
		{
			int time = iter->first;

			if((iter->second).total > 0)
			{
				(iter->second).avgN = (double)(iter->second).n / (iter->second).total;

				(iter->second).avgM = (double)(iter->second).m / (iter->second).total;

				double dr_t = (1 - ((iter->second).avgM / (NUMBER_OF_NODES - 1) )) * exp(-1*(ttl - time)*(iter->second).avgN*(1/(alpha)));

				(iter->second).avgDr = (1 / (alpha)) * (ttl - time) * dr_t;

				double dd_t = (double)((NUMBER_OF_NODES - 1 - (iter->second).avgM)/(iter->second).avgN);

				//fprintf(stdout,"avgM: %f\n", (iter->second).avgM);

				double tmp = (NUMBER_OF_NODES - 1 - (iter->second).avgM);
				if(tmp == 0) tmp = 1;
				(iter->second).avgDd = (double)((pow(dd_t, 2) *alpha) / ( (NUMBER_OF_NODES - 1) * (tmp)));

				fprintf(log_convergence,"%i %f %f %f %f\n",time, (iter->second).avgN, (iter->second).avgM, (iter->second).avgDr,(iter->second).avgDd);
			}
		}
		fclose(log_convergence);
	}
}

void EpidemicRouting::update_reference_stat_map(double et, Bundle* b)
{
	if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0 || strcmp("REGION1,_o607:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o175:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o1007:0", bm_->agentEndpoint()) == 0 ||
			strcmp("REGION1,_o831:0", bm_->agentEndpoint()) == 0 )
	{
		//double new_bundle_metric_value = (1/alpha)*(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)*exp((-1*(exact_copys_number(b)))*((b->bdl_prim_->exp_time-b->bdl_prim_->elapsed_time)/alpha));

		//double new_bundle_metric_value=(1-(float)(exact_number_of_node_that_have_seen_it(b)/(NUMBER_OF_NODES-1)))*(alpha/pow((double)exact_copys_number(b),2.0));
		int iet = (int)ceil(et);
		int n10 = iet - (iet % 50);

		int exnc = exact_copys_number(b);
		int exns = exact_number_of_node_that_have_seen_it(b);
		map<int , stat_node>::iterator iter =  ref_stat_map.find(n10);
		//fprintf(stdout, "et: %i estimation %i pm: %i exnc %i exns %i\n", iet, n10, n10, exnc, exns);
		if(iter != ref_stat_map.end())
		{
			(iter->second).n += exnc;
			(iter->second).m += exns;
			(iter->second).total ++;
		}else
		{
			stat_node sn;
			sn.n = exnc;
			sn.m = exns;
			sn.total = 1;
			sn.avgDr = 0;
			sn.avgDd = 0;
			sn.avgN = 0;
			sn.avgM = 0;

			ref_stat_map[n10]= sn;
		}


	}

}

void EpidemicRouting::LoadRefStat()
{
	if(strcmp("REGION1,_o191:0", bm_->agentEndpoint()) == 0 && refTab == NULL)
	{
		refTab  = (double*)malloc(sizeof(double) * 72);
		FILE * refErr;
		refErr = fopen("refErrDr", "r");
		if(refErr != NULL)
		{
			char tmp[100];
			for(int i=0; i<72;i++)
			{

				fscanf(refErr, "%s\n", tmp);
				refTab[i] = atof(tmp);
				fprintf(stdout, "Loading refErrDr: %i %f\n", i, refTab[i]);
			}
			fclose(refErr);
		}else
		{fprintf(stderr, "unable to open refErrDr\n");exit(1);}

	}
}
