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

/** \file registration.h   Delivery registration.
*/

#ifndef DTN_REGISTRATION_H
#define DTN_REGISTRATION_H

/** Undefined failure action. */
#define FAIL_UNDEF 0
/** Defer delivery until requested. */
#define FAIL_DEFER 1
/** Abort delivery. */
#define FAIL_ABORT 2
/** Data sink. */
#define FAIL_SINK  3

/** Registration Entry.
 */
class RegEntry {
 public:
  RegEntry();
  ~RegEntry();

  int   token_;        /**< Registration token.                       */
  int   failAct_;      /**< Delivery failurer action                  */
  char* endpoint_;     /**< Destination endpoint id                   */
  int   passive_;      /**< Passive reception enabled.                */
  RegEntry* next_;     /**< List linkage.                             */
};

/** Registration handler.
 */
class Registration {
 public:
  Registration();
  ~Registration();
  int addReg(const char* failact, const char* endpoint);
  int  deReg(const char* ctoken);
  RegEntry* lookup(const char* endpoint);
  const char* lookup_id(const char* token);
  int start(const char* ctoken);
  int stop (const char* ctoken);
  int change(const char* ctoken, const char* failact);

 private:
  RegEntry* reg_;      /**< Registration list.                       */
};

#endif // DTN_REGISTRATION_H
