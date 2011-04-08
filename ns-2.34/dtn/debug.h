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

/** \file debug.h  Debug definitions.
*/


#ifndef DTN_DEBUG_H
#define DTN_DEBUG_H

#include <signal.h>
#include "common/scheduler.h"

/** Redefine abort() to actually cause core dump. */
#define abort() raise(SIGABRT)

//#define DEBUGLOG
//#define BDLTRACE
//#define PKTLOG
//#define QUEUELOG

#if defined(DEBUGLOG) || defined(BDLTRACE) || defined(PKTLOG) || defined(QUEUELOG)
#define CREATE_LOGDIR
#endif

/* Debug defines */

/** Error conditions                  */
#define DEB_ERR        0
/** Warning conditions                */
#define DEB_WARN       1
/** Normal but significant condition  */
#define DEB_NOTICE     2
/** Informational                     */
#define DEB_INFO       3
/** Debug-level messages              */
#define DEB_DEBUG      4

/** At which level it should start to be logged to file     */
#define FILELEVEL DEB_DEBUG
/** At which level it should start to be logged to console  */
#define CONSLEVEL DEB_WARN


/** Filename of the debug log. */
#define LOGFILE "dtn-logfile"
/** Filename of the trace log. */
#define TRACEFILE "dtn-tracefile"
/** Filename of the queue log. */
#define QUEUELOGFILE "dtn-queuelog"
/** Base directory for logs. */
#define LOGBASEDIR "log"

/** Trace: New bundle created. */
#define BDL_NEW    "NEW"
/** Trace: Forwarded bundle to TO. */
#define BDL_FWD    "FORWARD"
/** Trace: Received bundle from FROM. */
#define BDL_RECV   "RECEIVE"
/** Trace: Local delivery stored bundle for future delivery. */
#define BDL_LPEND  "LOCALPEND"
/** Trace: Local delivery performed ok. */
#define BDL_LDONE  "LOCALDONE"
/** Trace: Local delivery dropped. */
#define BDL_LDROP  "LOCALDROP"
/** Trace: Delivered to agent. */
#define BDL_AGENT  "AGENT"
/** Trace: Bundle expired. */
#define BDL_EXPIRE "EXPIRE"
/** Trace: Took custody from FROM. */
#define BDL_CUST   "CUSTODY"

/** Debug print function.\ Logs to console and file with filtering based on \a FILELEVEL and \a CONSLEVEL .
 *
 * \param level  Log level.
 * \param format Printf format string.
 * \param args   Printf arguments (variables to be printed).
*/
#ifdef DEBUGLOG
#define DPRINT(level, format, args...) do{ \
if(level<=CONSLEVEL)             printf(         "D%d(%-25s:%4d)%10.6f: " format "\n", \
                                        level, __FILE__, __LINE__, Scheduler::instance().clock(), ## args); \
if(logfile && level<=FILELEVEL) fprintf(logfile, "D%d(%-25s:%4d)%10.6f: " format "\n", \
                                        level, __FILE__, __LINE__, Scheduler::instance().clock(), ## args); \
  fflush(logfile);} while(0)
#else
#define DPRINT(level, format, args...) do{}while(0)
#endif //DEBUGLOG

/** Debug logfile. */
extern FILE* logfile;
extern FILE* tracefile;
extern FILE* queuelog;
extern char* logdir;

#endif // DTN_DEBUG_H
