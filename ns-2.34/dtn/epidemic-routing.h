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


#ifndef EPIDEMIC_ROUTING_H
#define EPIDEMIC_ROUTING_H

#include "routes.h"
#include "bundle.h"
#include "common/node.h"
#include "bmflags.h"
#include "anti_packet.h"
//#include "metrics_approximation.h"
#include "BloomStat.h"
#include <string>
#include <list>
#include <map>
#include <vector>
#include <semaphore.h>

class VisitorList{
public :
	VisitorList();
	~VisitorList();
	void update_elapsed_time(double);
	double  get_elapsed_time();
	const char * getid(){return id.c_str();};
	void set_arrival_date(double x){this->time_stamp=x;}
	void setid(char *s);
	void set_elapsed_time(double e);
	void set_ttl(double e);
	double  get_ttl(){return ttl;}
	int hopcount;
	double time_stamp;
	double elapsed_time;
	double ttl;
private:
	string id;
};

class BundleStore{
public :
	 BundleStore(int mst);
	~BundleStore();
	void update_elapsed_time(double);
	double  get_elapsed_time();
	const char * getid(){return id.c_str();};
	void set_arrival_date(double x){this->time_stamp=x;};
	void add_copie(int r){};
	void setid(char *s);
	void set_bundle(Bundle *b);
	Bundle *b_;
	double time_stamp;
	int forwarding_number;
	int enable_sprying;
	int max_sprying_time;
	double network_copy_time;
	void ShowDetails();
private:
	string id;
	
};


class ResendListTimer;
class EpidemicRouting;

/** Created after needed Bundles List was sent to the destination 
*/

class BundlesToAck {
public :
	BundlesToAck(EpidemicRouting *er,char * list, char *dest );
	~BundlesToAck();
	char * getDest(){return (char *)dest_.c_str();};
	int    get_state(){return ready;};
	char * get_list_to_ack(){return (char *)l_to_ack.c_str();};
	int    exist_bundle(char *uid);
	BundlesToAck * next;
	EpidemicRouting *er_;
	u_int8_t  nb_to_ack;
private :
	string dest_; 
	string l_to_ack;
	u_int8_t current_ack;
	u_int8_t ready;
};

/** Created after needed Bundles List was recived 
*/

class ListWaitingAck {
public :
	ListWaitingAck(ResendListTimer *rlt,char * ltr,char *dest,char * dr, nsaddr_t da,EpidemicRouting *);
	~ListWaitingAck();
	void stopTimer();
	ListWaitingAck *next;
	const char * getDest(){return retransmit_to_dest.c_str();}
	const char * get_list_to_retransmit(){return list_to_retransmit.c_str();}
	const char * get_dest_region(){return dest_region.c_str();}
	nsaddr_t dest_addr;
	ResendListTimer *rlt_;
	EpidemicRouting *er_;
	int number_of_ret;

private :
	string list_to_retransmit;
	string retransmit_to_dest;
	string dest_region;
	
};

class EpidemicSession
{

public:
	EpidemicSession(string x):peerId(x)
	{
	}
	string peerId;
};

class CleanBufferTimer;
class BundleManager;
class TraceTimer;

typedef struct stat_node{
	int n, m;
	int total;
	double avgN;
	double avgM;
	double avgDr;
	double avgDd;
}stat_node;


class EpidemicRouting {

public  :
		EpidemicRouting(BundleManager *b);
		~EpidemicRouting();

		char * getBundleId(Bundle *b);
		BundleStore *  exist_id(char *id,int u_id);
		int  add_id(char * id,int u_id);
		int  add_bundle(Bundle *b);
		int  free_place();
		void init_buffers();
		void free_buffers();
		void  list_diff(char *recv,int len,int rule,Bundle *b, string &difference);
		int  send_bundles_list(char * recv, int len, nsaddr_t from_me_, char * from_region_,int type,const char * dest);
		void bundles_id_list(char * dest, string & strList);
		int  number_of_bundles(char *l,int len);
		void make_clean();

		// Epidemic session
		void init_session(char * id,char *r, string source_id);
		int  recv_c1(Bundle *b);
		int  recv_c2(Bundle *b);
		int  recv_c3(Bundle *b);

		int  add_delivered_bundle(Bundle *b);
		int get_u_id(char *id);
		void get_rn(char *id, string & rn);
		int id_length(char * list, int id);
		int length_of_local_list(char * dest);
		void arrange_all();
		void check_to_send_ack(Bundle *b);
		void update_bta_list(Bundle *b);
		void getBundleIdUid(Bundle *b, string &bundleUid);
		void add_bl_toack(char *,char *);
		int recv_block_ack(Bundle *b);
		void add_list_wack(char *,char *,char * , nsaddr_t );
		void resend_bundles_block(ListWaitingAck *);
		u_int32_t  get_min_diff();
		void delete_bta_block(const char *dest);
		int delete_ret_list(ListWaitingAck * bloc);
		/** Dropping policies */
		int add_with_drop_tail(Bundle *b);
		int add_with_drop_from_front(Bundle *b);
		int add_with_drop_from_front2(Bundle *b);
		
		list<BundleStore *>::iterator get_youngest_message(int *);
		list<BundleStore *>::iterator get_oldest_message(int *);
		
		int add_with_drop_oldest_message(Bundle *b);
		int add_with_bin_drop_oldest(Bundle * b);
		int add_with_drop_youngest_message(Bundle *b);
		int add_and_minimize_total_delay_reference(Bundle *b);
		int add_and_minimize_total_delay_approximation(Bundle *b);
		

		void god_check_for_message(const char * id,int u_id,int *x);
		void god_check_for_visitor(const char * id,int u_id,int *x);
		int add_and_maximize_total_delivery_rate_reference(Bundle *b);
		int add_and_maximize_total_delivery_rate_approximation(Bundle *b);

		list<BundleStore *>::iterator get_message_with_smallest_metric_value_reference(Bundle *b,int *,double *);
		list<BundleStore *>::iterator get_message_with_delay_metric_reference(Bundle *b,int *,double *);
		list<BundleStore *>::iterator get_message_with_smallest_metric_value_approximation(Bundle *b,int *,double *);
		list<BundleStore *>::iterator get_message_with_delay_metric_approximation(Bundle *b,int *,double *);

		int exact_copys_number(Bundle *b);
		int is_my_message(Bundle *b);
		int get_number_of_copies(Bundle *b);
		int is_message_delivered(Bundle *b);
		float get_min( float a, float b){ if( a>=b) return b; else return a;}
		BundleStore * get_pointer_to(int position);
		int is_it_a_newVisitor(Bundle *b);
		int add_new_visitor(Bundle *b);
		void clean_visitor_list();
		int get_number_of_nodes_that_have_seen_it(Bundle *b);
		
		void get_source_id_of_bundle(Bundle *b, string &);
		void delete_delivred_bundles();
		int recv_anti_packet(Bundle *b);
		int stat_matrix_recived_first(Bundle *b);
		int stat_matrix_recived_last(Bundle *b);
		int stat_matrix_recived_versions_based_reverse_schema_first(Bundle *b);
		int stat_matrix_recived_versions_based_reverse_schema_last(Bundle *b);
		void get_messages_ids_for_stat_request(string & msg);	
		void get_map_ids_for_stat_request(string & msg, map<string, int> &map);
		void get_messages_ids_based_on_version_for_stat_request(string & msg);		

		void UpdateLocalMessagesStat();
		void send_stat_matrix(char * dest, double, int sequence);
		void send_antipacket_matrix(char * dest);
		int exact_number_of_node_that_have_seen_it(Bundle *b);
		void test_method_1();
		void Trace();
		int is_it_a_destination_for_one_of_my_messages( string node_id);
		
		// Scheduling of bundles 
		double get_delivery_rate_reference_metric(Bundle *b);
		double get_delivery_delay_reference_metric(Bundle *b);
		
		double get_delivery_rate_history_based_metric(Bundle *b);
		double get_delivery_delay_history_based_metric(Bundle *b);
		
		double get_delivery_rate_flooding_based_metric(Bundle *b);
		double get_delivery_delay_flooding_based_metric(Bundle *b);
		
		void arrange_bundles_to_send_according_to_reference_delivery_metric(vector<string> &  bundles_table, int table_length);
		void arrange_bundles_to_send_according_to_reference_delay_metric(vector<string> & bundles_table , int table_length);
		void arrange_bundles_to_send_according_to_history_based_delay_metric(vector<string> &  bundles_table, int table_length);
		void arrange_bundles_to_send_according_to_history_based_delivery_metric(vector<string> &  bundles_table, int table_length);
		void arrange_bundles_to_send_according_to_flooding_based_delay_metric(vector<string> &  bundles_table, int table_length);
		void arrange_bundles_to_send_according_to_flooding_based_delivery_metric(vector<string> &  bundles_table, int table_length);
		void initialization();
		
		void view_bundles_to_send_metrics(char ** bundles_table,int table_length);
		

		void ShowBuffredBundles();
		bool IsSessionActive(string peer);
		void InitNewSession(string peer);
		void RemoveSession(string peer);

		// Maintaining list of all messages that i have seen
		void AddSeenMessage(string id);
		bool DidISeeMessage(string &id);
				
		// Logging functions
		int LogDrandDdMetrics();	
	
		//update reference stat
		void update_reference_stat_map(double et, Bundle* b);
		void save_avg_reference_stat();
		void save_sum_square_error(double time);
		
		void LogForSensitivityAnalysis();

		void LoadRefStat();

		double time_since_i_recived_a_copy;	
		double buffer_last_update;

		BundleManager* bm_;
		Delivred_Bundles_List * dbl_;
		Network_Stat_Bloom * statBloom_;
		BundlesToAck * b_to_ack;
		ListWaitingAck * lw_ack;

		double total_meeting_samples;
		double last_meeting_instant;
		int number_of_meeting;		

		// not unique meetings
		double total_meeting_samples_not_unique;
		double last_meeting_instant_not_unique;
		int number_of_meeting_not_unique;		
		int get_closest_et(int et);
		
private :
		
		int number_of_buffered_bundles;
		
		map<string, EpidemicSession*> sessionsMap;
 		
		sem_t buffer_sem;
		list<BundleStore *> bundleStoreList;
		list<string> bundlesSeen;
		map<string , VisitorList *> visitorsMap;

		CleanBufferTimer * cbt_;
		int number_of_blocks_waiting_for_ack;
		int number_of_blocks_to_ack;
		int max_block_retransmission;
		TraceTimer * traceTimer;
		// LogFiles
		FILE * statSize;
		FILE * log_meetric;
		// Reference Stat
		map<int ,stat_node> ref_stat_map;
		
};

#endif
