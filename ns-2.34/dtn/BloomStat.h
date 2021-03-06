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


#ifndef BLOOM_STAT_H
#define BLOOM_STAT_H
#include "bundle.h"

#include "bmflags.h"
#include <string>
#include "anti_packet.h"
#include <map>
#include <list>
#include <semaphore.h>
#include <pthread.h>

typedef struct NodeVersion{
	string nodeId;
	int version;
}NodeVersion;



typedef list<NodeVersion> NodeVersionList;

class Network_Stat_Bloom;
class BundleManager;

typedef struct ResultsThreadParam{
	int workToDo;
	Network_Stat_Bloom * nsb_;
	int intRes;
	double floatRes;
	double et;
}ResultsThreadParam;

class Bloom_Axe{
public :
	Bloom_Axe()
	{
		max_lt=0;
		min_lt=0;
		this->ns=NULL;
	};
	void initiate_intervall(double min,double max,Network_Stat_Bloom *nss)
	{
		max_lt=max;
		min_lt=min;
		this->ns=nss;
	};


	double max_lt;
	double min_lt;
	Network_Stat_Bloom *ns;
	
};
class Dtn_Node_2{
public :

	Dtn_Node_2(char *,double , Network_Stat_Bloom *ns);
	~Dtn_Node_2();
	char * get_node_id();
	double get_meeting_time(){return last_meeting_time;};
	void update_meeting_time(double t){last_meeting_time=t;}
	void add_bundle(){number_of_bundles++;};
	void UpdateStatVersion(int v)
	{
		stat_version = v;
	}
	void ShowMiMap()
	{
		fprintf(stdout, "NodeId : %s MiMap version: %i ",node_id.c_str(), stat_version);
		for(int i= 0 ;i<axe_length;i++)
		{
			fprintf(stdout, "%i, ",  miBitMap[i]);
		}	
		fprintf(stdout,"\n");
	}
	void ShowMiMap(FILE *f)
	{
		fprintf(f, "NodeId : %s MiMap version: %i ",node_id.c_str(), stat_version);
		for(int i= 0 ;i<axe_length;i++)
		{
			fprintf(f, "%i, ",  miBitMap[i]);
		}	
		fprintf(f,"\n");
	}

	void ShowBitMap()
	{
		fprintf(stdout, "NodeId : %s BitMap version: %i ",node_id.c_str(), stat_version);
		for(int i= 0 ;i<axe_length;i++)
		{
			fprintf(stdout, "%i, ",  bitMap[i]);
		}	
		fprintf(stdout,"\n");
	}
	void ShowBitMap(FILE *f)
	{
		fprintf(f, "NodeId : %s BitMap version: %i ",node_id.c_str(), stat_version);
		for(int i= 0 ;i<axe_length;i++)
		{
			fprintf(f, "%i, ",  bitMap[i]);
		}	
		fprintf(f,"\n");
	}
		
	// Updating the bitMap
 	void SetTimeBinValue(int bin, int value, bool, bool, double);
	void UpdateBitMap(map<int, int> &map);
	void UpdateMiBitMap()
	{
		if(!miMapDone && miStartIndex != -1)
		{
			for(int i = miStartIndex; i< axe_length; i++)
			{
				miBitMap[i] = 1;
			}
			miMapDone = true;
		}	
	}	
	map<int, int> bitMap;
	map<int, int> bitMapStatus;
	map<int, int> miBitMap;
	int miStartIndex;
	bool miMapDone;
	// Only the source of the message could modify the version of the statistics
	int stat_version;
	double updated;
private :
	
	
	string node_id;
	double last_meeting_time;
	int number_of_bundles;
	Network_Stat_Bloom *nsb_;
	int axe_length;

	// The list length is equal to the axe_length, each bit indicates whether the node has a copy of the message
	// or if it has seen the message

	
};

class Dtn_Message
{
public:
	Dtn_Message(char *, double, Network_Stat_Bloom*);	
	~Dtn_Message();

	char * get_bstat_id();
	void addNode(string nodeId, map<int, int> & m, double lm, int statVersion, int miStartIndex);
	void addNode(string nodeId, int bin, int binValue, double meetingTime, int statVersion);

	int getNumberOfNodes()
	{
		return mapNodes.size();
	}
	void ShowMiMapMessage();
	void ShowNiMapMessage();
	void ShowMiMapMessage(FILE *f);
	void ShowNiMapMessage(FILE *f);

	void ShowNodes()
	{
		fprintf(stdout, "Number of nodes: %i\n", mapNodes.size());
		for(map<string, Dtn_Node_2 * >::iterator iter = mapNodes.begin();iter != mapNodes.end();iter++)
		{
			fprintf(stdout, "	Node id: %s\n", iter->first.c_str());
		}
	}
	// Returns the number of copies at a given time bin
	int ShowStatistics();
	int ShowStatistics(FILE *f);

	// Returns the hole description
	void GetDescription(string &d);
	void GetSubSetDescription(string &d, map<string, int> & nodesList);
	// Return a (MessageId, NodeId, Version) description
	void GetVersionBasedDescription(string &d);
	int GetNodeVersion(string & nodeId)
	{
		map<string, Dtn_Node_2 * >::iterator iter = mapNodes.find(nodeId);
		if(iter != mapNodes.end())
		{
			return (iter->second)->stat_version;
		}else {
			// it should take this message into consideration, it does not have it
			return -1;
		}
	}

	void UpdateMiMap(int binIndex);
	void UpdateNiMap(int binIndex);
	void UpdateDdMap();
	void UpdateDrMap();

	void ImmediateUpdateMiMap();
	void ImmediateUpdateNiMap();
	void ImmediateUpdateDdMap();
	void ImmediateUpdateDrMap();

	int GetNumberOfNodesThatHaveSeeniT(int binIndex)
	{
		if(last_mi_report < updated && mapNodes.size()>0)
		{	
			UpdateMiMap(binIndex);
			last_mi_report = TIME_IS;
		}
		if(miMap[binIndex] == 0) return 1;
		return miMap[binIndex];
	}

	int GetNumberOfCopiesAt(int binIndex)
	{
		if(last_ni_report < updated && mapNodes.size()>0)
		{	
			UpdateNiMap(binIndex);
			last_ni_report = TIME_IS;
		}
		if(niMap[binIndex] == 0) return 1;
		return niMap[binIndex];
	}

	double GetAvgDdAt(int binIndex)
	{
		if(last_dd_report < updated && mapNodes.size()>0)
		{
			UpdateDdMap();
			last_dd_report = TIME_IS;
		}
		return ddMap[binIndex];
	}
	double GetAvgDrAt(int binIndex)
	{
		if(last_dr_report < updated && mapNodes.size()>0)
		{
			UpdateDrMap();
			last_dr_report = TIME_IS;
		}
		return drMap[binIndex];
	}
	bool IsValid(int binIndex);
	int GetMaxVersion(int & smallerVersion);
	void SetLifeTime(double lt)
	{
		life_time = lt;
		last_lt_update = TIME_IS;
	}
	double GetLifeTime()
	{
		life_time = (TIME_IS - last_lt_update) + life_time;
		last_lt_update = TIME_IS;
		return life_time;
	}

	double life_time;
	double last_lt_update;
	double ttl;
	double updated;
	double last_mi_report;
	double last_ni_report;
	double last_dd_report;
	double last_dr_report;
	// indicates if the stat message is old enought to be removed from
	// the active stats and put within the old ones
	// message_status = 0 the message is still considered during the stat exchanges between two nodes
	// message_status = 1 the message is old enought ( version >= axe_lenght) the node will not ask for stat updates related to this message
	int message_status;
	map<string, Dtn_Node_2 * > mapNodes;
	int message_number;
	bool toDelete;
private :
	string bundle_id;
	Network_Stat_Bloom *nsb_;
	// Map for maintaining the number of nodes that have seen these message for each time bin
	map<int, int> miMap;
	// Map for maintaining the number of copies of the message at each time bin
	map<int, int> niMap;
	// map for maintaining the a dd value for each time bin
	map<int, double> ddMap;
	// map for maintaining the dr value for each time bin
	map<int, double > drMap;
};





class Delivred_Bundles_List;

class Network_Stat_Bloom{

public :

	Network_Stat_Bloom(Delivred_Bundles_List *,BundleManager *b);
	~Network_Stat_Bloom();
	int  get_number_of_nodes()
	{
		if(number_of_nodes == 0 || number_of_nodes == 1) return 2;
		else return number_of_nodes;
	}
	void InitLogFiles();
	void ShowAllMessagesStatistics();
	void ShowAllMessagesStatistics(FILE *);

	void ShowAvgStatistics();
	void LogAvgStatistics();

	Dtn_Message *is_bundle_here(char *bundle_id);
	void add_node(char * node_id, double t_view);
	int add_bundle(char * bundle_id,char * nodeId, double lt,double ttl);

	// Called from the outside to get the statistics results
	void get_stat_from_axe(double et,char *,double *ni,double *mi,double *dd_m,double *dr_m);
	static void * GetResultsThreadProcess(void * arg);

	// Used for logging
	void get_stat_from_axe(int binIndex, double *ni, double *mi, double *dd_m, double *dr_m, double *et);
	int  get_number_of_copies(char * bundle_id, double lt);
	int  get_number_of_nodes_that_have_seen_it(char * bundle_id, double lt);
	double get_avg_number_of_copies(double lt);
	double get_avg_number_of_nodes_that_have_seen_it(double lt);
	double get_avg_dr_at(double lt);
	double get_avg_dd_at(double lt);	
	double get_avg_number_of_copies(int binIndex);
	double get_avg_number_of_nodes_that_have_seen_it(int binIndex);
	
	double get_avg_dr_at(int binIndex);
	double get_avg_dd_at(int binIndex);

	void update_bundle_status(char * node_id, char * bundle_id, int del,double lt,double ttl);
	void add_message(char * message,char * messageId);

	double getNodeMeetingTime(string nodeId)
	{
		map<string, double>::iterator iter = nodesMatrix.find(nodeId);
		if(iter != nodesMatrix.end())
		{
			return iter->second;
		}
		else return 0;	
	}	
	// Used for the pushing policy
	void get_stat_to_send(string &);
	// Used for the reverse schema policy
	void get_stat_to_send_based_on_received_ids(string & ids, string & stat);
	void get_message_nodes_couples(string &message_id, string &description);
	void get_stat_to_send_based_on_versions(string & ids, string & stat);
	void convert_versions_based_stat_to_map(string &stat, map<string, NodeVersionList> &);
	

	//Managing the received stat string
	int  get_number_of_messages_from_stat(char * recived_stat);
	void get_message_i_from_stat(char *recived_stat, int p, string &id);
	void get_bundle_id(char * bundle, string & id);
	int get_number_of_nodes_for_a_bundle(char *bundle);
	void get_node_from_stat(char *bundle,int p, string & node);
	void get_node_bitMap(string & node, map<int, int> &m);
	double get_bundle_ttl(char * bundle);
	int  get_bundle_del(char * node);
	void get_node_id_from_stat(char *node, string &id);
	double get_node_lm(char *node);
	int get_node_stat_version(char *node);
	int get_miIndex_from_stat(char *node);
	
	// Called from the outside to update the collected statistics	
	void update_network_stat(char * recived_stat);
	void update_message( string&,string&, Dtn_Message*);

	// Convert an elapsed time to a bin index
	int ConvertElapsedTimeToBinIndex(double et);
	double ConvertBinINdexToElapsedTime(int binIndex);
	// Returns a bloom filter of all messages	
	void getMessagesBloomFilter(string & bf);
	int GetNumberOfMessages(){return messagesMatrix.size();}
	
	// Returns the number of invalid messages i'm interested in
	int GetNumberOfInvalidMessages();
	int GetNumberOfValidMessages(map<string, Dtn_Message *>::iterator & iterOldest);
	void DeleteMessage(map<string, Dtn_Message *>::iterator & iterOldest)
	{
		delete iterOldest->second; 
		messagesMatrix.erase(iterOldest);
	}
	bool IsMessageValid(string msgId);
	void ClearMessages();
	
	Bloom_Axe *stat_axe;


	double 	stat_last_update;
	int number_of_meeting;
   	double total_meeting_samples;
  	int axe_length;
	int axe_subdivision;
	BundleManager * bm_;
	int messages_number;
	bool someThingToClean;
	unsigned int clearFlag;
private :
	
	map<string, Dtn_Message *> messagesMatrix;
	//list<Dtn_Message *> validMessages;
	map<string, double > nodesMatrix;
	int number_of_nodes;
	
	// LogFiles
	FILE * stat_axe_log;
	
};
 

#endif
