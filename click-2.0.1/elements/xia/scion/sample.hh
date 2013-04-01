/*****************************************
 * File Name : sample.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Last Modified : Wed 08 Feb 2012 02:59:52 PM EST

 * Purpose : 

******************************************/
#ifndef SAMPLE_HEADER_HH_
#define SAMPLE_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

/*include here*/
CLICK_DECLS

class Sample : public Element { 
    public :
        Sample(){};
        ~Sample(){};
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "Sample";}
        const char *port_count() const {return "-/-";}


        /*
            const char *processing():
                Configures the functionality of the ports. 
                returns input/output      

            Return values X/X
                Options for X
                h: push
                l: pull
                a: agnostic -> can be either push/pull but not both
            
            Available names for common processing
                
                AGNOSTIC        a/a
                PUSH            h/h
                PULL            l/l
                PUSH_TO_PULL    h/l
                PULL_TO_PUSH    l/h
                PROCESSING_A_AH a/ah 
                    : all input ports are agnostic/output[0] is agnostic and rest of
                    the output port is push

        */
        const char *processing() const {return PULL;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

    private:
    

};

CLICK_ENDDECLS


#endif
