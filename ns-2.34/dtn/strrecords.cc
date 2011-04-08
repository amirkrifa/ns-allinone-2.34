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

/** \file strrecords.cc  String records. 
*/

#include <string.h>
#include "strrecords.h"
#include "headers.h"
#include "debug.h"

/** String records constructor. 
 */
DTNStrRecords :: DTNStrRecords() : strcount_(0),first_(NULL),last_(NULL)
{
  
}

/** Destroy string records. 
 */
DTNStrRecords :: ~DTNStrRecords()
{
  DPRINT(DEB_DEBUG, "Removing String Records.");
  delete first_;
}

/** Adds a single record to the string records. It does not check for previous appearances of the text. 
 *
 * \param text A text string to add. 
 * \param len Length of the text string. 
 *
 * \retval 0 on success. 
 * \retval -1 if nothing could be added. 
 */
int DTNStrRecords :: addRecord(u_int8_t* text, int len)
{
  if(!text){DPRINT(DEB_WARN, "Invalid argument text."); return -1;}
  StrRec* tmp = new StrRec();
  if(!tmp){DPRINT(DEB_ERR, "Could not allocate memory for new string record."); return -1;}
  tmp->text_ = new char[len+1];
  if(!tmp->text_){DPRINT(DEB_ERR, "Could not allocate memory for string record text."); delete tmp; return -1;}
  tmp->len_  = len;
  memcpy(tmp->text_,text,(size_t) len);
  tmp->text_[len]='\0';
  tmp->next_ = NULL;
  if(!first_) first_ = tmp;
  if(last_) last_->next_ = tmp;
  last_ = tmp;
  strcount_++;
  return 0;
}

/** Adds two records from a tuple separated with a comma (,). It checks for existing records, thus avoiding duplicate string records. 
 *
 * \param text A text string to add. 
 *
 * \returns Eight bits containting two four-bit long positions of the first and second record respectively. 
 * \retval -1 if nothing could be added. 
 */
int DTNStrRecords :: addRecord(const char* text)
{
  if(!text){DPRINT(DEB_WARN, "Invalid argument."); return -1;}
  int i=0;
  int pos1;
  int pos2;
  StrRec* tmp;

  //first part
  while(text[i] != '\0' && text[i] != ',') i++;
  char* temptext = NULL;
  if(i){
    temptext = new char[i+1];
    if(!temptext){DPRINT(DEB_ERR, "Could not allocate memory for temptext."); return -1;}
    strncpy(temptext,text,i);
    temptext[i]='\0';
  }
  int id = lookup(temptext);
  if(id>=0){
    pos1 = id;
    delete []temptext;
  }else if(i){
    //create new record
    tmp = new StrRec();
    if(!tmp){DPRINT(DEB_ERR, "Could not allocate memory for new string record."); delete [] temptext; return -1;}
    tmp->len_  = strlen(temptext);
    tmp->text_ = temptext;
    tmp->next_ = NULL;
    if(!first_) first_ = tmp;
    if(last_) last_->next_ = tmp;
    last_ = tmp;
    pos1 = strcount_++;
  }else{
    DPRINT(DEB_WARN, "Syntax error in string record.");
    delete []temptext;
    return -1;
  }
  //end create
  //end first part

  //second part
  int len = strlen(text)-i;
  temptext = NULL;
  if(len>0){
    temptext = new char[len];
    if(!temptext){DPRINT(DEB_ERR, "Could not allocate memory for temptext."); return -1;}
    strncpy(temptext,&text[i+1],len);
  }
  id = lookup(temptext);
  if(id>=0){
    pos2 = id;
    delete []temptext;
  }else if(len>0){
    //create new record
    tmp = new StrRec();
    if(!tmp){DPRINT(DEB_ERR, "Could not allocate memory for new string record."); delete [] temptext; return -1;}
    tmp->len_  = strlen(temptext);
    tmp->text_ = temptext;
    tmp->next_ = NULL;
    if(last_) last_->next_ = tmp;
    last_ = tmp;
    pos2 = strcount_++;
  }else{
    DPRINT(DEB_WARN, "Syntax error in string record.");
    delete []temptext;
    return -1;
  }
  int pos = ((pos1*0x10) & 0xf0) + (pos2 & 0x0f);
  //end create
  //end second part
  return pos;
}

/** Checks all string records for a match on the specified text. 
 *
 * \param text A text string to search for. 
 *
 * \returns The id of the text. 
 * \retval -1 if no match is found. 
 */
int DTNStrRecords :: lookup(const char* text)
{
  if(!text){DPRINT(DEB_INFO, "Argument text is null.",text); return -1;}
  int id = 0;
  int textlen = strlen(text);
  StrRec* cur = first_;
  while(cur){
    if(textlen==cur->len_ && !strncmp(text,cur->text_,textlen)) return id;
    cur = cur->next_;
    id++;
  }
  return -1;
}

/** Finds string records corresponding to a combination of two string id:s. 
 *
 * \param number Two four-byte string id:s combined as an eight-byte integer. 
 *
 * \returns A string with the two requested records separated by a comma (,). When not used anymore, the string should be deleted. 
 */
char* DTNStrRecords :: getRecords(u_int8_t number)
{
  StrRec* one = getRecord((number & 0xf0)/0x10);
  StrRec* two = getRecord(number & 0x0f);
  if(!one || !two) return NULL;
  int len = one->len_ + two->len_ + 2;
  char* str = new char[len];
  if(!str){DPRINT(DEB_ERR, "Could not allocate memory."); abort();}
  strncpy(str,one->text_,one->len_);
  str[one->len_] = ',';
  strncpy(&str[one->len_+1],two->text_,two->len_);
  str[len-1] = '\0';
  return str;
}

/** Finds a string record corresponding to a combination of two four-byte string id:s.
 *
 * \param number A string id in range [0,15].
 *
 * \returns A pointer to the requested string record. 
 */
StrRec* DTNStrRecords :: getRecord(int number)
{
  int pos=0;
  StrRec* curr = first_;
  if(number>=0 && number<strcount_){
    while(pos++<number)
      if(curr) curr = curr->next_;
    if(curr) return curr;
  }
  DPRINT(DEB_WARN,"No string record found.");
  return NULL;
}

/** Clone string records.
 * 
 * \returns The cloned records.
 */
DTNStrRecords* DTNStrRecords :: clone()
{
  DTNStrRecords* rec = new DTNStrRecords();
  if(! rec) {DPRINT(DEB_ERR,"Could not allocate memory."); abort(); }
  int i=0;
  StrRec* curr = NULL;

  for(i=0;i<strcount_;i++){
    curr=getRecord(i);
    if(rec->addRecord((u_int8_t*)curr->text_,curr->len_)) DPRINT(DEB_WARN,"Records trashed.");
  }
  return rec;
}
