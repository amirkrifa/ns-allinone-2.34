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

/** \file dtn.h  DTN Agent. 
*/

#ifndef ns_dtn_h
#define ns_dtn_h

#include "common/agent.h"
#include "mobile/dumb-agent.h"
#include "common/packet.h"

#include "routes.h"
#include "bundlemanager.h"
#include <string>

/** Bundle specification version. */
#define BDL_VERSION 0x03

/** Main class for DTN */
class DTNAgent : public Agent {
public:
  DTNAgent();

  virtual int command(int argc, const char*const* argv);
  virtual void recv(Packet*, Handler*);
  Packet* newpacket(int size);
  void sendPacket(Packet* pkt);
  virtual void sendmsg(int nbytes, const char* flags=NULL);
  int custodian(){return custodian_;}  /**< Accepting custody. */
  double retransmit(){return retransmit_;} /**< Retransmission interval. */
  int rule;
  char * getlifespan();
  int epidemic_buffered_bundles;
  int delivered_bundles;
  int deleted_bundles;
// private:
  Routes* routes_;      /**< The routes list.                              */
  BundleManager* bm_;   /**< The bundlemanager.                            */
  Registration* reg_;   /**< Registraton handler.                          */
  char* src_;           /**< Application bundle source endpoint id.        */
  char* dest_;          /**< Application bundle destination endpoint id.   */
  char* rpt_to_;        /**< Application bundle report to endpoint id.     */
  char* cos_;           /**< Application bundle priority.                  */
  char* options_;       /**< Application bundle options.                   */
  char* lifespan_;      /**< Application bundle lifespan.                  */
  int custodian_;       /**< Accepting custody.                            */
  double retransmit_;   /**< Retransmission interval.                      */
public:  
/** DTNAgent configuration **/
  int infinite_ttl;
  int max_bundles_in_local_buffer;
  int max_neighbors;
  int max_hop_count;
  double hello_interval;
  int max_ids_count; 
  int delivered_bundles_cleaning_interval;
  int max_uid_length;
  int local_buffer_cleaning_interval;
  int block_resend_interval;	
  double neighbors_check_interval;
  int number_of_generated_packets;
  int number_of_deleted_bundles_due_to_ret_failure;
  int number_of_asked_bundles;
  int number_recv_bundles;
  int activate_routing;
  string source_log_node_id;
  int drop_policie;
  int drop_oldest_bin;
  int max_block_retransmission_number;
  double total_delay;
  int is_message_here;
  int max_sprying_time;
  int am_i_source;
  int is_delivered;
  int is_visitor_here;
  int anti_packet_mechanism;
  int maintain_stat;
  int activate_ack_mechanism;
  int reference;
  int number_of_sources;
  int scheduling_type;
  int axe_subdivision;
  int axe_length;
  int stat_sprying_policy;
  unsigned int epidimic_control_data_size;
  unsigned int stat_data_size;
  unsigned int data_size; 
  double avg_per_meeting_stat;

  // Related to the PairWiseAvg
  double current_number_of_nodes;
  double estimated_number_of_nodes;
  // Number of stat messages
  int max_number_of_stat_messages;
  double time_to_wait_until_using_new_stat;
  // Calculating the delivery rate and delay starting from the following uid
  int start_from_uid;
  int stop_at_uid;

};

#endif // ns_dtn_h
