/*****************************************
 * File Name : scionprint.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/
#ifndef SCIONPRINT_HEADER_HH_
#define SCIONPRINT_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
/*include here*/
#include "packetheader.hh"
#include "define.hh"
CLICK_DECLS
using namespace std;
enum LogType{
    EH=1,
    EM,
    EL,
    WH=101,
    WM,
    WL,
    IH=201,
    IM,
    IL,
};


class SCIONPrint{ 
    public :
        SCIONPrint(){
            m_iMsgLevel = 300; 
            m_iMaxLineNum = 1000000;
        };

        SCIONPrint(int msgLevel, const char* logFileName){
            m_iMsgLevel = msgLevel;
            time_t rawtime;
            time(&rawtime);
            tm* timeinfo;
            timeinfo = localtime(&rawtime);
            strcpy(m_csLogFileName, logFileName);
            //backup();         
            /*
            m_logFile = fopen(m_csLogFileName, "a");
            fprintf(m_logFile, "Start Time : %s\n", asctime(timeinfo));

            fclose(m_logFile);
            */
			//SL: get the line count of the current log file
			linecount(m_iLineNum);
			//SL: need to set this configurable...
            m_iMaxLineNum = 1000000;
            //m_iLineNum=1;
        };

        ~SCIONPrint()
        {
            //delete m_logFile;
        };
    
        void printLog(uint32_t ts, char* fmt, ...);
        void printLog(char* fmt, ...);
        void printLog(int logType, char* fmt, ...);
        void printLog(int logType, int msgType, char* fmt, ...);
        void printLog(int logType, int msgType, uint32_t ts, char* fmt, ...);
        void printLog(int logType, int msgType, uint32_t ts, uint64_t src,
            uint64_t dst, char* fmt, ...);
        
        void printLog(int logType, int msgType, uint32_t ts, HostAddr src,
        HostAddr dst, char* fmt, ...);
         
        void printSrcDst(uint64_t src, uint64_t dst);   
        void printTimestamp(uint32_t ts); 
        void backup();

		//SL: read the current log-file size (in # of lines).
		void linecount(int &i);
    private:
        int m_iLineNum;
        int m_iMaxLineNum;
        FILE* m_logFile; // Does not need to be deleted because fclose() is
                         // called in the source file.
        char m_csLogFileName[MAX_FILE_LEN];
        void printMsgType(int msgType);
        void printLogType(int logType);
        int m_iMsgLevel;
};

CLICK_ENDDECLS
#endif
