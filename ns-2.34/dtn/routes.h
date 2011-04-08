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

/** \file routes.h  Routes.
*/

#ifndef dtn_routes_h
#define dtn_routes_h

#include "common/node.h"
#include "config.h"
#include <sys/types.h>

#include "common/timer-handler.h"

class Routes;
class Route;

/** Info about a link.
 */

class LinkInfo{
 public:
  LinkInfo() : next_(NULL), busyTo_(0.0), node_(-1), mtu_(1000) {strcpy(linkObj_,"wireless");}  /**< Constructor. */
  ~LinkInfo() { delete(next_); delete(linkObj_); }  /**< Destructor.\ Deletes any linked linkinfos as well. */
  void setBusy(int size);
  LinkInfo* next_;
  
  char linkObj_[9];      /**< Object name of the link serving this route. */
  double busyTo_;      /**< At which time the route will be available again */
  nsaddr_t node_;      /**< Adress to the next hop. */  
  int mtu_;
};

/** A specific route.
 */

class Route{
public:
  Route(Routes *r, double duration ); /**<   Constructor. */
  ~Route(){ delete(next_); delete(route_); } /**<  Destructor.\ Deletes any linked routes as well. */
  
  Route* next_;        /**< Next route in list. */
  char* route_;        /**< Region this route is for, or NULL if it is an default route. */
  int custodian_;      /**< Flag to indicate whether this hop should accept custody. */
  int metric_;         /**< The priority of this route. */
  LinkInfo* linfo_;    /**< The link that this route uses. */
  Routes *routes_;

};

/** The routing table.
 */
class Routes {
 public:
  Routes(ns_addr_t* here);
  ~Routes();
  
  LinkInfo* getNextHop(const char* region, const char* admdest, nsaddr_t prev, double recvtime, int size, int custody);
  
  int add(nsaddr_t node, const char* route, int custodian, int metric, int mtu, const char* linkObj);
  int add(nsaddr_t node, const char* route, int custodian, int metric, int mtu);
  int delete_route(char *id);
  void setLocal(const char* region);
  char* region(){return region_;} /**< Retrieves the set region. */
  void  show_route_table(const char * rs);
  int 	get_number_of_routes(){ Route* s=list_;int i=0; 
				while(s!=NULL){s=s->next_;i++;} 
				return i;	
			      }
  int   is_there(nsaddr_t node,const char * region,int custodian, int metric, int mtu);
 private:
  int isAvail(LinkInfo* link);
  Route* list_;         /**< Pointer to list head.         */
  char* region_;        /**< The set region.               */
  LinkInfo* links_;     /**< Connected links.              */
  public: 
  ns_addr_t* here_;     /**< The address of this node.     */

};

#endif // dtn_routes_h
