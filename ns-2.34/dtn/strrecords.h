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

/** \file strrecords.h  String records. 
*/

#ifndef DTN_STRRECORDS_H
#define DTN_STRRECORDS_H

#include <sys/types.h>

/** Define 8-bit type if not defined. */
#ifndef u_int8_t
typedef unsigned char u_int8_t;
#endif

/** A string record */
class StrRec{
public:
  StrRec() : next_(NULL),len_(0),text_(NULL) {} /**<  Constructor. */
  ~StrRec(){delete next_;}                      /**<  Destructor.\ Deletes any linked string records as well. */
  
  StrRec* next_;          /**< Next record in list. */
  u_int8_t len_;          /**< String length. */
  char*    text_;         /**< String text. */
};

/** Storage of information on all string records in a dictionary header.
 */
class DTNStrRecords {
 public:
  DTNStrRecords();
  ~DTNStrRecords();
  int addRecord(u_int8_t* text, int len);
  int addRecord(const char* text);

  int lookup(const char* text);

  StrRec* getRecord(int number);
  char* getRecords(u_int8_t number);

  int strcount(){return strcount_;}

  DTNStrRecords* clone();
  
 private:
  int strcount_;    /**< Total number of records. */
  StrRec* first_;   /**< Pointer to first record in list. */
  StrRec* last_;    /**< Pointer to last record in list. */
};

#endif // DTN_STRRECORDS_H
