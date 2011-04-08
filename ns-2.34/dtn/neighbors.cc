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


#include "neighbors.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "bmflags.h"
#include "timers.h"
#include "routes.h"
#include "bundlemanager.h"
#include "dtn.h"
/** DtnNeighbor constructor */

DtnNeighbor::DtnNeighbor(char *id,double ad,char *r )
{
	strcpy(region,"");
	strcat(region,r);
	strcat(region,"\0");
	strcpy(id_,"");
	strcat(id_,id);
	strcat(id_,"\0");
	arrival_date_=ad;
}

/** DtnNeighborsManager Constructor */

DtnNeighborsManager::DtnNeighborsManager(BundleManager *bm)
{
	bm_=bm;
	number_of_neighbors=0;
	nt_=new DtnNeighborTimer(this);
	nt_->sched(bm_->da_->neighbors_check_interval);
	antipacket_update = 0;
}

/** DtnNeighborsManager Destructor */

DtnNeighborsManager::~DtnNeighborsManager()
{
	delete nt_;
	while(!neighborsMap.empty())
	{
		map<string, DtnNeighbor *>::iterator iter = neighborsMap.begin();
		delete iter->second;
		neighborsMap.erase(iter);
	}
}

/** Check if a Neighbor is Already in transmission range */

int DtnNeighborsManager::exist_neighbor(char *id)
{

	if(neighborsMap.find(string(id)) == neighborsMap.end())
		return -1;
	else return 1;
}

/** Updating a neighbor arrival date */

int DtnNeighborsManager::update_arrival_date(char *id)
{
	
	map<string, DtnNeighbor *>::iterator iter = neighborsMap.find(string(id));
	if(iter != neighborsMap.end())
	{
		(iter->second)->set_arrival_date(TIME_IS);
		return 1;
	}else return -1;

			
}

/** Add a Neighbor */

int DtnNeighborsManager::add_neighbor(char *id,char *r)
{
	
	if(neighborsMap.size() == bm_->da_->max_neighbors) 
	{
			fprintf(stdout,"Nombre max de voisins atteints \n");
			return -1;
	}
		
	int i=0;
	int pos=0;
	
	if(exist_neighbor(id) == -1)
	{
		neighborsMap[string(id)] = new DtnNeighbor(id,TIME_IS,r);
		number_of_neighbors++;
	} else 
	{
 		update_arrival_date(id); 
		return 1;
	}
}


 

/** Update Neighbors List, eliminate a Neighbor if its not in the trasmission range */

void  DtnNeighborsManager::check_neighbors()
{
	map<string, DtnNeighbor *>::iterator iter = neighborsMap.begin();
	
	while(iter != neighborsMap.end())
	{
		if((TIME_IS - (iter->second)->get_arrival_date()) > bm_->da_->neighbors_check_interval)
		{
			number_of_neighbors--;
			delete (iter->second);
			neighborsMap.erase(iter);
		}
		iter++;
	}

	nt_->resched(bm_->da_->neighbors_check_interval);
}

double DtnNeighborsManager::get_neighbor_arrival_date(char *id,char *r)
{
	map<string, DtnNeighbor *>::iterator iter = neighborsMap.find(string(id));
	if(iter != neighborsMap.end())
	{
		return (iter->second)->get_arrival_date();
	}else return 0;
}

