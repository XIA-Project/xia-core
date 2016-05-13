/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>
#include <map>
#include <algorithm>

#define MAX_XID_SIZE 100
#define VERSION "v1.0"
#define TITLE "XIA Multicast Endhost"
#define NAME "www_s.multicast.aaa.xia"

#define NUM_PROMPTS	2


// global configuration options
int verbose = 1;



struct addrinfo *ai;
sockaddr_x *sa;

void say(const char *fmt, ...);
void warn(const char *fmt, ...);
void die(int ecode, const char *fmt, ...);
void usage();
bool file_exists(const char * filename);



class MulticastEndhost{
  private:
   static void * InternalChunkThread(void * This) {((MulticastEndhost *)This)->ChunkLoop(); return NULL;}
   static void * InternalControlThread(void * This) {((MulticastEndhost *)This)->ControlLoop(); return NULL;}
   pthread_t _thread;
   pthread_t _thread2;
   std::string fname;
   
    Graph *DGramDAG;
    Graph *ChunkDAG;
    Graph *SourceServiceDAG;
    Graph *RPServiceDAG;
    std::string DGramSID;
    std::string ChunkSID; //for now we use the same for both
    
    std::string SourceName;
    std::string postname;
    std::map<std::string, std::string> *chunksLists;
    
    
    int ChunkSock, DGramSock;
    ChunkContext *ctx;
    
    int  BuildChunkDAGs(ChunkStatus *cs, std::string chunks, std::string ad, std::string hid);
    bool StartChunkLoop();
    bool StartControlLoop();
    int  SendCommand(std::string cmd);
    int  PullChunks( std::string &s, std::string chunks, Graph *g);
    int  PullChunksOneByOne( std::string &s, std::string chunks, Graph *g);
  
  
  
  protected:
    void ChunkLoop();
    void ControlLoop();

   
   
  public:  
    int  Join(std::string name);
    int  Leave();
    void InitializeClient(std::string mySID);
    virtual void ChunkReceived(char *buf, size_t len, ChunkInfo *info);

    

  
  
  MulticastEndhost( std::string s = ""){
  fname = "";
  postname = s;
  chunksLists = new std::map<std::string, std::string>;
  RPServiceDAG = NULL;
  

  }


 
};
//======================================================


void MulticastEndhost::ChunkLoop(){
  while (1) {
    

    char buf1[XIA_MAXBUF];
    ChunkInfo *info = (ChunkInfo*) malloc(sizeof(ChunkInfo));
    memset(buf1, 0, sizeof(buf1));

    warn("Will now listen for chunks\n");
    int received = -1;
    if((received = XrecvChunkfrom(ChunkSock, buf1, sizeof(buf1), 0, info)) < 0)
	    die(-5, "Receive error %d on socket %d\n", errno, ChunkSock);
    else{
	    ChunkReceived(buf1, received, info);
// 	    warn("Received Chunk CID: %s\n", info->cid);
    }

    free(info);
    
  }

  Xclose(ChunkSock);
  pthread_exit(NULL);
}

bool MulticastEndhost::StartChunkLoop()
{
  return (pthread_create(&_thread, NULL, InternalChunkThread, this) == 0);
}

bool MulticastEndhost::StartControlLoop()
{
  return (pthread_create(&_thread2, NULL, InternalControlThread, this) == 0);
}

int MulticastEndhost::BuildChunkDAGs(ChunkStatus *cs, std::string chunks, std::string ad, std::string hid)
{
	
	
	// build the list of chunks to retrieve
	std::size_t prev_location = 0;
	std::size_t location = chunks.find_first_of("|");
	int n = 0;
	
	while(location != std::string::npos) {
		
		std::string c = chunks.substr(prev_location, location-prev_location);

		char *dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", ad.c_str(), hid.c_str(), c.c_str());
//    		say("%s\n", dag);
// 		Graph *g = new Graph(dag);
// 		delete g;
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		
		
		n++;
		prev_location = location + 1;
// 		say("prev_loc %d\n", prev_location);
		location = chunks.find_first_of("|", prev_location);
// 		say("location %d\n", location);
	}

	return n;
}

int MulticastEndhost::SendCommand(std::string cmd){

  int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
  sockaddr_x dag;
  SourceServiceDAG->fill_sockaddr(&dag);


  int rc = -1;
  if( (rc = Xsendto(sock, cmd.c_str(), strlen(cmd.c_str()), 0, (struct sockaddr*)&dag, sizeof(dag))) >= 0) {
     say("\nSent message: %s \n", cmd.c_str());
  }else{
    say("\nFailed to send: %s \n", cmd.c_str());
  }
    
return rc;

}


int MulticastEndhost::PullChunks( std::string &s, std::string chunks, Graph *g)
{
  
  int num_chunks  = std::count(chunks.begin(), chunks.end(), '|');
  say("numchunks: %d\n", num_chunks); 
  ChunkStatus *cs = new ChunkStatus[num_chunks];
  char data[XIA_MAXCHUNK];
  int len;
  int status;
  int n = -1;
  int csock = -1;// ChunkSock;
  
  
  std::string ad = g->dag_string().substr(g->dag_string().find("AD:"),43);
  say(("sad " + ad + "\n").c_str());
  std::string hid = g->dag_string().substr(g->dag_string().find("HID:"),44);
  say(("shid " + hid + "\n").c_str());

  n = BuildChunkDAGs(cs, chunks, ad, hid);

  if(n != num_chunks)
    die(-1, "There was an error parsing the cids and number of chunks");
  
  if ((csock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
    die(-1, "unable to create chunk socket\n");
  
  
  // bring the list of chunks local
  say("requesting list of %d chunks\n", n);
  if (XrequestChunks(csock, cs, n) < 0) {
	  say("unable to request chunks\n");
	  return -1;
  }
  
  say("checking chunk status\n");
  int maxtries = 10;
  int tries = 0;
  
  while(tries<maxtries) {
	  status = XgetChunkStatuses(csock, cs, n);

	  if (status == READY_TO_READ)
		  break;

	  else if (status < 0) {
		  say("error getting chunk status\n");
		  return -1;

	  } else if (status & WAITING_FOR_CHUNK) {
		  // one or more chunks aren't ready.
		  say("waiting... one or more chunks aren't ready yet\n");
	  
	  } else if (status & INVALID_HASH) {
		  die(-1, "one or more chunks has an invalid hash");
	  
	  } else if (status & REQUEST_FAILED) {
		  die(-1, "no chunks found\n");

	  } else {
		  say("unexpected result\n");
	  }
	  tries++;
	  sleep(1);
	  
  }

  
  if (status != READY_TO_READ){
    say("Read Failed. \n");
    return -1;
    }
  
  say("all chunks ready\n");

  for (int i = 0; i < n; i++) {
	  char *cid = strrchr(cs[i].cid, ':');
	  cid++;
	  say("reading chunk %s\n", cid);
	  if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
		  say("error getting chunk\n");
		  return -1;
	  }
	  //FIXME: this whole chunkstatus and chunkinfo and chunkcontext thing is bad and should be fixed in the whole content module
	  //In chunk status cid is not really cid! it's the RE dag. 
	  std::string mcid = std::string(cs[i].cid);
	  mcid = mcid.substr(mcid.find("CID:")+4, mcid.npos);
/*	  say("CID: %s\n", cs[i].cid);
	  say("MCID: %s-\n", mcid.c_str());
	  say("strlen(info.cid): %d, strlen(mcid): %d\n", (CID_HASH_SIZE + 1), strlen(mcid.c_str()))*/;
	  ChunkInfo info;
	  strcpy(info.cid, mcid.c_str());//, strlen(info.cid));
// 	  say("INFOCID: %s\n", info.cid);
	  info.size = len;
// 	  info.ttl= 1000;
// 	  info.timestamp.tv_sec  = 0;
// 	  info.timestamp.tv_usec = 0;
	  
	  s = s + std::string(data,len);

	  free(cs[i].cid);
// 		  cs[i].cid = NULL;
// 		  cs[i].cidLen = 0;
  }
  delete[] cs;
  return n;
}


// I think there may be a bug in XRequestChunks. Very fast large files cause problems with missing chunks for some reason
int MulticastEndhost::PullChunksOneByOne( std::string &s, std::string chunks, Graph *g)
{
  
  int num_chunks  = std::count(chunks.begin(), chunks.end(), '|');
  say("numchunks: %d\n", num_chunks); 
  ChunkStatus *cs = new ChunkStatus[num_chunks];
  char data[XIA_MAXCHUNK];
  int len;
  int status;
  int n = -1;
  int csock = -1;// ChunkSock;
  
  
  std::string ad = g->dag_string().substr(g->dag_string().find("AD:"),43);
  say(("sad " + ad + "\n").c_str());
  std::string hid = g->dag_string().substr(g->dag_string().find("HID:"),44);
  say(("shid " + hid + "\n").c_str());

  n = BuildChunkDAGs(cs, chunks, ad, hid);

  if(n != num_chunks)
    die(-1, "There was an error parsing the cids and number of chunks");
  
  if ((csock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
    die(-1, "unable to create chunk socket\n");
  
  
  // bring the list of chunks local
  say("requesting list of %d chunks\n", n);

  
  
  for(int j = 0; j < n ; j ++){
    
      if (XrequestChunk(csock, cs[j].cid, 1) < 0) {
	  say("unable to request chunks\n");
	  return -1;
  }
  
  say("checking chunk status\n");
  int maxtries = 10;
  int tries = 0; 
    
  while(tries<maxtries) {
	  status = XgetChunkStatus(csock, cs[j].cid, 1);

	  if (status == READY_TO_READ)
		  break;

	  else if (status < 0) {
		  say("error getting chunk status\n");
		  return -1;

	  } else if (status & WAITING_FOR_CHUNK) {
		  // one or more chunks aren't ready.
		  say("waiting... one or more chunks aren't ready yet\n");
	  
	  } else if (status & INVALID_HASH) {
		  die(-1, "one or more chunks has an invalid hash");
	  
	  } else if (status & REQUEST_FAILED) {
		  die(-1, "no chunks found\n");

	  } else {
		  say("unexpected result\n");
	  }
	  tries++;
	  sleep(1);
	  
  }
    
  if (status != READY_TO_READ){
    say("Status Check Failed. dest dag: %s\n", cs[j].cid);
    return -1;
    }    
    
    
    
    char *cid = strrchr(cs[j].cid, ':');
    cid++;
    say("reading chunk %s\n", cid);
    if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[j].cid, cs[j].cidLen)) < 0) {
	    say("error getting chunk\n");
	    return -1;
    }
    //FIXME: this whole chunkstatus and chunkinfo and chunkcontext thing is bad and should be fixed in the whole content module
    //In chunk status cid is not really cid! it's the RE dag. 
    std::string mcid = std::string(cs[j].cid);
    mcid = mcid.substr(mcid.find("CID:")+4, mcid.npos);
//     say("CID: %s\n", cs[j].cid);
//     say("MCID: %s-\n", mcid.c_str());
//     say("strlen(info.cid): %d, strlen(mcid): %d\n", (CID_HASH_SIZE + 1), strlen(mcid.c_str()))*/;
    ChunkInfo info;
    strcpy(info.cid, mcid.c_str());//, strlen(info.cid));

    info.size = len;

    s = s + std::string(data,len);

    free(cs[j].cid);
    
  }
  
  delete[] cs;
  return n;
}



int MulticastEndhost::Join(std::string name){
    
      // lookup the xia service 
  SourceName = name;
  sockaddr_x dag;
  socklen_t daglen;
  daglen = sizeof(dag);
  if (XgetDAGbyName(SourceName.c_str(), &dag, &daglen) < 0)
	  die(-1, "unable to locate: %s\n", SourceName.c_str());
  
  //FIXME: Added for chunk push. Clean up the code here.
  if (Xgetaddrinfo(SourceName.c_str(), NULL, NULL, &ai) != 0)
		  die(-1, "unable to lookup name %s\n", SourceName.c_str());
  sa = (sockaddr_x*)ai->ai_addr;
  SourceServiceDAG = new Graph(sa);
  printf("Service DAG: %s\n", SourceServiceDAG->dag_string().c_str());
  RPServiceDAG = SourceServiceDAG;
	  //Send messages
  

  char joinmessage[512];
  sprintf(joinmessage, "joinc|%s", DGramDAG->dag_string().c_str());
  
  int rc = -1;
  if( (rc = SendCommand(std::string(joinmessage)) ) >= 0) {
//       say("sent message: %s\n", DGramDAG->dag_string().c_str());
  }
  else{
	say("Couldn't send join message\n");
	die(0, DGramDAG->dag_string().c_str());
  }

  //TODO: For now it's unreliable. This can be added to make it reliable.
//     char buf[XIA_MAXBUF];
//     sockaddr_x cdag;
//     socklen_t dlen;
//     dlen = sizeof(cdag);
//     memset(buf, 0, sizeof(buf));
//     if ((rc = Xrecvfrom(DGramSock, buf, sizeof(buf), 0, (struct sockaddr *)&cdag, &dlen)) < 0) {
// 	    warn("Recv error on socket %d, closing connection\n", DGramSock);
//     }
//     
//     if(strncmp("joinc ok|", buf,9) == 0){
// //       std::string st(buf);
// //       std::string dag = st.substr(9);
//       say("join successful %s\n", buf);
//     } 

  
  StartControlLoop();
  return rc;
  
}



int MulticastEndhost::Leave(){
  char joinmessage[512];
  sprintf(joinmessage, "leave|%s", DGramDAG->dag_string().c_str());
  
  int rc = -1;
  if( (rc = SendCommand(std::string(joinmessage)) ) >= 0) {
//       say("sent message: %s\n", DGramDAG->dag_string().c_str());
  }
  else{
	say("Couldn't send leave message\n");
	die(0, DGramDAG->dag_string().c_str());
  }

  return rc;
  
}


void MulticastEndhost::ControlLoop(){

    while (1) {
// 	say("Dgram control waiting\n");

      char buf[XIA_MAXBUF];
      sockaddr_x cdag;
      socklen_t dlen;
      int n;
      pid_t pid = getpid();

      dlen = sizeof(cdag);
      memset(buf, 0, sizeof(buf));
      if ((n = Xrecvfrom(DGramSock, buf, sizeof(buf), 0, (struct sockaddr *)&cdag, &dlen)) < 0) {
	      warn("Recv error on socket %d, closing connection\n", pid);
	      break;
      }

      say("DGram received %d bytes, Text: %s\n", n, buf);
      
      
      if(strncmp("recvfile|", buf,9) == 0){
	std::string st(buf);
// 	say((st + "\n").c_str());
	std::string fs = st.substr(9, st.npos);
	int endoffname = fs.find_first_of("|");
	fname = "F" +postname + "-" + fs.substr(0, endoffname);
// 	say( (fname+ "\n").c_str());	  

	
	std::string chunkhashes = fs.substr(endoffname+1, fs.npos);
	say( (chunkhashes+ "\n").c_str());	
		  
	std::string c;
	// build the list of chunks to retrieve
	std::size_t prev_loc = 0;
	std::size_t location = chunkhashes.find_first_of("|");
	FILE *f = fopen(fname.c_str(), "w");
	
	while(location != std::string::npos) {
		c = chunkhashes.substr(prev_loc, location-prev_loc);
// 		say(("chunk key: " + c+"\n").c_str());
		std::string cc = chunksLists->find(c)->second; 
// 		say(("chunklist: "+cc + "\n").c_str() );

		prev_loc = location + 1;
		location = chunkhashes.find_first_of("|", prev_loc);

		
		std::string result;		 
// 		PullChunks( result, cc.c_str(), SourceServiceDAG);
		PullChunksOneByOne( result, cc.c_str(), SourceServiceDAG);
		
		fwrite(result.c_str(), sizeof(char), result.size(), f);
//  		  say((result + "\n").c_str() );
		
	}
	
	fclose(f);
	say("File done: %s\n", fname.c_str());
	fname = "";
      }
      else if(strncmp("chunks|", buf,7) == 0){
	std::string st(buf);
	std::string clean = st.substr(7, clean.npos);
// 	say((clean + "\n").c_str());
	int endofhash = clean.find_first_of("|");
	std::string hash = clean.substr(0, endofhash);
// 	say( ( hash+"\n" ).c_str());
// 	  say(clean.c_str());
// 	  say("\n");
// 	  std::string ccid = clean.substr(endofhash+1, 39);
// 	  say(ccid.c_str());	  
// 	say(clean.substr(endofhash+1, clean.npos).c_str());
	chunksLists->insert( std::pair<std::string, std::string>(hash, clean.substr(endofhash+1, clean.npos)) );

      }
      else if(strncmp("pullchunks|", buf, 11) == 0){
	std::string st(buf);
	std::string clean = st.substr(11, clean.npos);
	std::string result;
	PullChunksOneByOne( result, clean.c_str(), SourceServiceDAG);
      }
      else if(strncmp("pullchunksrp|", buf, 13) == 0){
	std::string st(buf);
	std::string clean = st.substr(13, clean.npos);
	std::string result;
	PullChunksOneByOne( result, clean.c_str(), RPServiceDAG);
      }
      else if(strncmp("setrp|", buf, 6) == 0){
	std::string st(buf);
	std::string clean = st.substr(6, clean.npos);
	Graph *g = new Graph(clean);
	RPServiceDAG = g;
      }
      else if(strncmp("removerp", buf, 8) == 0){
	if(RPServiceDAG == NULL)
	  return;
	if(RPServiceDAG->dag_string() != SourceServiceDAG->dag_string()){
	  delete RPServiceDAG;
	}
	RPServiceDAG = new Graph(SourceServiceDAG->dag_string());
      }

// 	say("Dgram Server waiting\n");
// 	say(">> ");

    }

    Xclose(DGramSock);
	
}



void MulticastEndhost::InitializeClient(std::string mySID)
{
  int rc;
// 	char sdag[1024];
  char IP[MAX_XID_SIZE];
  char localdag[XIA_MAX_DAG_STR_SIZE];

  // create a socket, and listen for incoming connections
  if ((DGramSock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
	    die(-1, "Unable to create the listening socket\n");
  
  char my_ad[MAX_XID_SIZE];
  char my_hid[MAX_XID_SIZE];
  
  rc = XreadLocalHostAddr(DGramSock, localdag, XIA_MAX_DAG_STR_SIZE, IP, MAX_XID_SIZE);

  if (rc < 0) {
	  Xclose(DGramSock);
	    die(-1, "Unable to read local address.\n");
  } else{
	  Graph g_localhost(localdag);
	  strcpy(my_ad, g_localhost.intent_AD_str().c_str());
	  strcpy(my_hid, g_localhost.intent_HID_str().c_str());
	  warn("My AD: %s, My HID: %s\n", my_ad, my_hid);
  }
    


//       char my_sid[] = "00000000dd41b924c1001cfa1e1117a812492444";
  char DAG[500];
  sprintf(DAG,"DAG 0 - \n%s 1 - \n%s 2 - \nSID:%s", my_ad, my_hid,  mySID.c_str());
  std::string myDAG = DAG;
  say(DAG);
  DGramDAG = new Graph(myDAG);

  sockaddr_x saddr;
  DGramDAG->fill_sockaddr(&saddr);
  
  if (Xbind(DGramSock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
	  Xclose(DGramSock);
	  die(-1, "Unable to bind DGRAM to the dag: %s\n", saddr);
  }


    
    
//      ------------Chunk Stuff
  if ((ChunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
	  die(-1, "unable to create chunk socket\n");
  ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
  if (ctx == NULL)
      die(-2, "Unable to initilize the chunking system\n");
    
    
    
//       char my_csid[] = "00000000dd41b924c1001cfa1e1117a812492411";
  char CDAG[500];
  // Using same SID for chunk and DGram DAG. This should probably be different. But it fworks as the bind for chunk and DGram is different.
  // Can be easily changed to something else. Probably should. This was just easier.
  sprintf(CDAG,"DAG 0 - \n%s 1 - \n%s 2 - \nSID:%s", my_ad, my_hid,  mySID.c_str());
  std::string chunkDAG = CDAG;
  say(CDAG);
  ChunkDAG = new Graph(chunkDAG);

  sockaddr_x caddr;
  ChunkDAG->fill_sockaddr(&caddr);

//       IF receiving but we don't receive for now
  if (XbindPush(ChunkSock, (struct sockaddr*)&caddr, sizeof(caddr)) < 0) {
    die(-1, "Unable to bind CHUNK to the caddr: %s\n", caddr);
    Xclose(ChunkSock);
  }
  
  
  StartChunkLoop();

}



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++


//Abtract Methods


void MulticastEndhost::ChunkReceived(char* buf, size_t len, ChunkInfo* info)
{
  //Do something with the chunk
  if(len == 0 || buf == NULL)
    warn("There was an error receiving the chunk");
  say("Received Chunk %s\n", info->cid);
  
  std::string localfname = "T" +postname + ".tmp";
//   say( (fname+ "\n").c_str());	  
  FILE *f = fopen(localfname.c_str(), "a");
  fwrite(buf, sizeof(char), strlen(buf), f);
  fclose(f);
}






//=======================================================

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

}

/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}



void usage(){
	say("usage: get|put <source file> <dest name>\n");
}

bool file_exists(const char * filename)
{
    if (FILE * file = fopen(filename, "r")){
	fclose(file);
	return true;
	}
    return false;
}

int main(int argc, char **argv)
{	
  if(argc > 1)
    say("For now it doesn't accept source name");
  char *name = &argv[0][2];
  printf("name: %s", name);
  MulticastEndhost *meh = new MulticastEndhost(std::string(name)); //:-)
  meh->InitializeClient(std::string("00000000dd41b924c1001cfa1e1117a812492444"));
  meh->Join(std::string(NAME));
//   say("AFTER JOIN\n");
//   sleep(5);
//   meh->Leave();
  
  while(1){
  }
  
}

