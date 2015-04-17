/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>


/*change this to corresponding header*/
#include"scionprint.hh"


CLICK_DECLS

SCIONPrint::SCIONPrint(int msgLevel, const char* logFileName, const char* 
    entityName): m_iMsgLevel(msgLevel), m_iMaxLineNum(1000000), m_Enabled(1){
    m_iMsgLevel = msgLevel;
    time_t rawtime;
    time(&rawtime);
    tm* timeinfo;
    timeinfo = localtime(&rawtime);
    strcpy(m_csLogFileName, logFileName);
    strcpy(m_csLogEntity, entityName);
    m_logFile = fopen(m_csLogFileName, "a");
    if(m_logFile==NULL){
        // create file if possible
        int fd = open(m_csLogFileName, O_RDWR|O_CREAT, 777);
        if(fd==-1) m_Enabled = 0;
    }
    if(m_Enabled){
        fprintf(m_logFile, "Entity %s starts at %s\n", 
            m_csLogEntity, asctime(timeinfo));
    }
    fclose(m_logFile);
    linecount(m_iLineNum);
}

void SCIONPrint::printLog(int logType, int msgType, uint32_t ts, HostAddr src,
        HostAddr dst, char* fmt, ...) {
    return;
}

void SCIONPrint::printLog(uint32_t ts, char* fmt, ...){
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    printTimestamp(ts);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}


void SCIONPrint::printLog(char* fmt, ...){
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}

void SCIONPrint::printLog(int logType, char* fmt, ...){
    if(logType > m_iMsgLevel){
        return;
    }
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    printLogType(logType);
    fprintf(m_logFile, "%s:", m_csLogEntity);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}

void SCIONPrint::printLog(int logType, int msgType, char* fmt, ...){
    if(logType > m_iMsgLevel){
        return;
    }
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    printLogType(logType);
    fprintf(m_logFile, "%s:", m_csLogEntity);
    printMsgType(msgType);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}

void SCIONPrint::printLog(int logType, int msgType, uint32_t ts, char* fmt, ...){
    if(logType > m_iMsgLevel){
        return;
    }
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    printLogType(logType);
    fprintf(m_logFile, "%s:", m_csLogEntity);
    printTimestamp(ts);
    printMsgType(msgType);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}

void SCIONPrint::printLog(int logType, int msgType, uint32_t ts, uint64_t src,
uint64_t dst, char* fmt, ...){
    if(logType > m_iMsgLevel){
        return;
    }
    linecount(m_iLineNum);
    if(m_iLineNum >= m_iMaxLineNum){
        backup();
    }
    m_logFile = fopen(m_csLogFileName, "a");
    va_list args;
    va_start(args, fmt);
    printLogType(logType);
    fprintf(m_logFile, "%s:", m_csLogEntity);
    printTimestamp(ts);
    printSrcDst(src, dst);
    printMsgType(msgType);
    vfprintf(m_logFile, fmt, args);
    va_end(args);
    fclose(m_logFile);
}

void SCIONPrint::printTimestamp(uint32_t ts){
    time_t time = ts;
    tm* timeinfo = localtime(&time); 
    char* t = asctime(timeinfo);
    char day[4];
    char month[9];
    char date[3];
    char curtime[10];
    char year[5];
    sscanf(t,"%s %s %s %s %s",day,month,date,curtime,year);
    strcat(curtime,",");
    fprintf(m_logFile, curtime);
}

void SCIONPrint::printMsgType(int msgType){
    switch(msgType){
        case BEACON :  fprintf(m_logFile,"BEACON,"); break;
        case DATA :  fprintf(m_logFile,"DATA,"); break;
        case CERT_REQ :  fprintf(m_logFile,"CERT_REQ,"); break;
        case CERT_REP :  fprintf(m_logFile,"CERT_REP,"); break;
        case CERT_REQ_LOCAL : fprintf(m_logFile,"CERT_REQ_LOCAL,"); break;
        case CERT_REP_LOCAL : fprintf(m_logFile,"CERT_REP_LOCAL,"); break;
        case PATH_REQ :  fprintf(m_logFile,"PATH_REQ,"); break;
        case PATH_REP :  fprintf(m_logFile,"PATH_REP,"); break;
        case PATH_REG :  fprintf(m_logFile,"PATH_REG,"); break;
        case PATH_REQ_LOCAL :  fprintf(m_logFile,"PATH_REQ_LOCAL,"); break;
        case PATH_REP_LOCAL :  fprintf(m_logFile,"PATH_REP_LOCAL,"); break;
        case AID_REQ :  fprintf(m_logFile,"AID_REQ,"); break;
        case AID_REP :  fprintf(m_logFile,"AID_REP,"); break;
        case IFID_REQ :  fprintf(m_logFile,"IFID_REQ,"); break;
        case IFID_REP:  fprintf(m_logFile,"IFID_REP,"); break;
        case UP_PATH :  fprintf(m_logFile,"UP_PATH,"); break;
        case ROT_REQ : fprintf(m_logFile,"ROT_REQ,"); break;
        case ROT_REP : fprintf(m_logFile,"ROT_REP,"); break;
        case ROT_REQ_LOCAL : fprintf(m_logFile, "ROT_REQ_LOCAL,"); break;
        case ROT_REP_LOCAL : fprintf(m_logFile, "ROT_REP_LOCAL,"); break;
        default: break;
    }
}

void SCIONPrint::printLogType(int logType){
    switch(logType){
        case EH: fprintf(m_logFile,"[E] "); break;
        case WH: fprintf(m_logFile,"[W] "); break;
        case IH: fprintf(m_logFile,"[I] "); break;
        default: break;
    }
}


void SCIONPrint::printSrcDst(uint64_t src, uint64_t dst){
    fprintf(m_logFile, "(%llu, %llu),",src,dst);
}

void SCIONPrint::backup(){
    time_t rawtime;
    time(&rawtime);
    tm* timeinfo;
    timeinfo = localtime(&rawtime);
    char* t = asctime(timeinfo);
    char day[4];
    char month[9];
    char date[3];
    char curtime[10]={};
    char year[5];
    sscanf(t,"%s %s %s %s %s",day,month,date,curtime,year);
    for(int i=0;i<10;i++){
        if(curtime[i]==':'){
            curtime[i]='-';
        }
    }
    char newFileName[MAX_FILE_LEN];
    strcpy(newFileName, m_csLogFileName);
    strcat(newFileName,".");
    strcat(newFileName,curtime);
    rename(m_csLogFileName, newFileName); 
    m_iLineNum=0; 
}

void SCIONPrint::linecount(int &lnum) {
	char buf[MAX_FILE_LEN + 10];
	FILE * fp;

	sprintf(buf, "wc -l %s", m_csLogFileName);
	fp = popen(buf, "r");
	if(fp) {
		fscanf(fp, "%d", &lnum);
	} else {
		lnum = 1;
	}
	pclose(fp);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONPrint)


