/*
Copyright (c) 2005 Henrik Eriksson and Patrik Jönsson.
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

/** \file registration.cc  Delivery registration.
*/

#include "registration.h"
#include "debug.h"

/** RegEntry constructor. 
 */
RegEntry :: RegEntry() : failAct_(FAIL_UNDEF), endpoint_(NULL), passive_(0), next_(NULL)
{
  static int token=1;
  token_=token++;
}

/** Destroy RegEntry. 
 */
RegEntry :: ~RegEntry()
{
  delete endpoint_;
  if(next_) DPRINT(DEB_WARN, "Deleting a linked registration.");
}

/** Registration constructor. 
 */
Registration :: Registration() : reg_(NULL)
{

}

/** Destroy Registration.
 */
Registration :: ~Registration()
{
  while(reg_){
    RegEntry* temp = reg_;
    reg_ = reg_-> next_;
    temp->next_=NULL;
    delete temp;
  }
}

/** Register an endpoint id for passive reception.
 *
 * \param failact   Requested failure action. (DEFER or ABORT)
 * \param endpoint  The endpoint that the registration is for.
 *
 * \returns  Registration token.
 * \retval -1 on failure.
*/
int Registration :: addReg(const char* failact, const char* endpoint){
  if(! (failact && endpoint)){
    DPRINT(DEB_WARN, "At least one argument to reg is null.");
    return -1;
  }

  RegEntry* temp = new RegEntry();
  if(! temp){ DPRINT(DEB_ERR, "Could not allocate memory for new RegEntry."); abort(); }
  
  temp->endpoint_ = new char[strlen(endpoint)+1];
  if(! temp->endpoint_){ DPRINT(DEB_ERR, "Could not allocate memory for new char*."); abort(); }
  strcpy(temp->endpoint_, endpoint);
  
  if(! reg_) reg_=temp;
  else{
    RegEntry* tail=reg_;
    while(tail->next_) tail=tail->next_;
    tail->next_=temp;
  }
  
  if     (strcmp(failact, "DEFER") == 0) temp->failAct_ = FAIL_DEFER;
  else if(strcmp(failact, "ABORT") == 0) temp->failAct_ = FAIL_ABORT;
  else if(strcmp(failact, "SINK" ) == 0) temp->failAct_ = FAIL_SINK;
  else  { fprintf(stderr, "'%s' is not a valid failure action.", failact);  return -1; }

  fprintf(stdout, "Registered '%s' to %d with action '%s'(%d).\n",endpoint,temp->token_,failact,temp->failAct_);

  return temp->token_;
}

/** Deregister an endpoint id.
 *
 * \param ctoken  Registration token.
 *
 * \returns  Status code indicating outcome.
 * \retval  0 on success.
 * \retval -1 on failure.
*/
int Registration :: deReg(const char* ctoken){
  if(! ctoken){DPRINT(DEB_WARN, "ctoken is null."); return -1; }
  int token = atoi(ctoken);
  if(! token){ DPRINT(DEB_NOTICE, "Invalid Token."); return -1;}
  RegEntry* cur = reg_;
  RegEntry* prev = NULL;
  while(cur && cur->token_ != token) {
    prev = cur;
    cur = cur->next_;
  }
  if(cur){
    if(prev) prev->next_=cur->next_;
    else reg_=cur->next_;
    cur->next_=NULL;
    delete cur;
    return 0;
  }
  fprintf(stderr, "Could not find a matching registration for token %d.", token);
  return -1;
}

/** Look up a registration.
 *
 * \param endpoint  Destination endpoint to lookup.
 *
 * \returns Pointer to RegToken.
 * \retval NULL on failure.
 */
RegEntry* Registration :: lookup(const char* endpoint)
{
  RegEntry* cur = reg_;
  while(cur && strcmp(endpoint, cur->endpoint_) != 0) cur = cur->next_;
  return cur;
}

/** Finds endpoint for a registration token.
 *
 * \param ctoken Token identifying the registration.
 *
 * \returns endpoint id for the token.
 * \retval NULL if token wasn't found.
 */
const char* Registration :: lookup_id(const char* ctoken)
{
  if(! ctoken){DPRINT(DEB_WARN, "ctoken is null."); return NULL; }
  int token = atoi(ctoken);
  if(! token){ DPRINT(DEB_NOTICE, "Invalid Token."); return NULL;}

  RegEntry* cur = reg_;
  while(cur && token != cur->token_) cur=cur->next_;
  if(cur) return cur->endpoint_;
  return NULL;  
}

/** Starts delivery for a registration.
 * 
 * \param ctoken Token identifying the registration.
 *
 * \returns Status code indicating succes.
 * \retval 0 If the call succeded.
 * \retval -1 If the call failed.
*/
int Registration :: start(const char* ctoken)
{
  if(! ctoken){fprintf(stderr, "ctoken is null."); return -1; }
  int token = atoi(ctoken);
  if(! token){ fprintf(stderr, "Invalid Token."); return -1;}

  RegEntry* cur = reg_;
  while(cur && token != cur->token_) cur=cur->next_;
  if(! cur) return -1;
  cur->passive_=1;
  return 0;
}

/** Stops delivery for a registration.
 * 
 * \param ctoken Token identifying the registration.
 *
 * \returns Status code indicating succes.
 * \retval 0 If the call succeded.
 * \retval -1 If the call failed.
*/
int Registration :: stop(const char* ctoken)
{
  if(! ctoken){DPRINT(DEB_WARN, "ctoken is null."); return -1; }
  int token = atoi(ctoken);
  if(! token){ DPRINT(DEB_NOTICE, "Invalid Token."); return -1;}

  RegEntry* cur = reg_;
  while(cur && token != cur->token_) cur=cur->next_;
  if(! cur) return -1;
  cur->passive_=0;
  return 0;
}

/** Changes failure action for a registration.
 * 
 * \param ctoken Token identifying the registration.
 * \param failact Requested failure action. (DEFER or ABORT).
 *
 * \returns Status code indicating succes.
 * \retval 0 If the call succeded.
 * \retval -1 If the call failed.
*/
int Registration :: change(const char* ctoken, const char* failact)
{
  if(! ctoken || ! failact){fprintf(stderr, "ctoken or failact is null."); return -1; }
  int token = atoi(ctoken);
  if(! token){ fprintf(stderr, "Invalid Token."); return -1;}
  
  RegEntry* cur = reg_;
  while(cur && token != cur->token_) cur=cur->next_;
  if(! cur) return -1;

  if     (strcmp(failact, "DEFER") == 0) cur->failAct_ = FAIL_DEFER;
  else if(strcmp(failact, "ABORT") == 0) cur->failAct_ = FAIL_ABORT;
  else if(strcmp(failact, "SINK" ) == 0) cur->failAct_ = FAIL_SINK;
  else  { fprintf(stderr, "'%s' is not a valid failure action.", failact);  return -1; }  
  return 0;
}
