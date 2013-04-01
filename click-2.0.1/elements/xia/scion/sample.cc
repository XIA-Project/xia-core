/*****************************************
 * File Name : sample.cc

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Last Modified : Tue 01 May 2012 09:13:19 PM PDT

 * Purpose : 

******************************************/

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
#include<string.h>
#include<stdio.h>
#include<stdlib.h>


/*change this to corresponding header*/
#include"sample.hh"


CLICK_DECLS
int Sample::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        cpEnd) <0){

    }

    return 0;
}

int Sample::initialize(ErrorHandler* errh){
    click_chatter("Hello world\n");  
    return 0;
}



CLICK_ENDDECLS
EXPORT_ELEMENT(Sample)


