/*ts=4 */
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
#include <vector>
#include <queue>
#include <semaphore.h>


#define MAX_XID_SIZE 100
#define VERSION "v1.0"
#define TITLE "XIA Multicast RP"
#define NAME "www_s.multicast.aaa.xia" // connects to this name. We can just replace it with the source service DAG address.


#define NUM_CHUNKS	10
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
class MulticastChunkData;


class Receiver{
  public:
  
  Graph *ControlDAG;
  Graph *ChunkDAG;
  int currentcounter;
 
  Receiver(Graph *control, Graph *chunk){
    ControlDAG = control;
    ChunkDAG = chunk;
  }
};

class MulticastRP{
  private:
   static void * InternalChunkThread(void * This) {((MulticastRP *)This)->ChunkLoop(); return NULL;}
   static void * InternalChunkSendThread(void * This) {((MulticastRP *)This)->ChunkSendLoop(); return NULL;}
   static void * InternalControlThread(void * This) {((MulticastRP *)This)->ControlLoop(); return NULL;}
   pthread_t _thread;
   pthread_t _thread2;
   pthread_t _thread3;
   pthread_mutex_t  mtxlock;
   pthread_mutex_t  chunksendmtxlock;
   std::string fname;
   //My info. Right now DGramDAG and ChunkDAG can be the same. Essentially you can bind the same SID for chunk and dgram and it will work.
   Graph *DGramDAG;
   Graph *ChunkDAG;
  //   Graph *SourceChunkDAG;
    Graph *SourceServiceDAG;

    std::string DGramSID;
    std::string ChunkSID; //for now we use the same for both
    
    std::string SourceName;
    std::string postname;
    std::map<std::string, std::string> *chunksLists;
    std::map<std::string, Receiver *> *hosts; 
    std::queue<MulticastChunkData *> *MulticastChunks;
    
    
    int ChunkSock, DGramSock;
    ChunkContext *ctx;
    sem_t qsem;
    
    int  BuildChunkDAGs(ChunkStatus *cs, std::string chunks, std::string ad, std::string hid);
    bool StartChunkLoop();
    bool StartChunkSendLoop();
    bool StartControlLoop();
    int  SendCommand(std::string cmd);
    int  PullChunks( std::string &s, std::string chunks, Graph *g);
    int  PullChunksOneByOne( std::string &s, std::string chunks, Graph *g);
    int  PullPushChunksOneByOne( std::string &s, std::string chunks, Graph *g, std::vector< Graph* >* rcptList);

    void EndhostJoin(std::string clean);
    void EndhostLeave(std::string dags);
  
  
  
  protected:
    void ChunkLoop();
    void ChunkSendLoop();
    void ControlLoop();

   
   
  public:  
    template <typename Iter>  
    static void DeletePointers(Iter begin, Iter end);
    template <typename T>
    static void RemoveVec(std::vector<T *>* vec);
    
    std::vector<Graph *> *BuildEndhostChunkRecvList();
    std::vector<Graph *> *BuildEndhostControlRecvList();
    void SendToList(std::string cmd, std::vector<Graph*> *rcptList);
    void MulticastToEndHosts(std::string cmd);
    int  Join(std::string name);
    int  Leave();
    void InitializeClient(std::string mySID);
    int  MulticastChunk(const ChunkContext* xxct, const char *buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info);
    void ParallelMulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info);
    virtual void ChunkReceived(char *buf, size_t len, ChunkInfo *info);

    

  
  
  MulticastRP( std::string s = ""){
    pthread_mutex_init(&mtxlock, NULL);
    pthread_mutex_init(&chunksendmtxlock, NULL);
    fname = "";
    postname = s;
    chunksLists = new std::map<std::string, std::string>;
    hosts = new std::map<std::string, Receiver *>;
    MulticastChunks = new std::queue<MulticastChunkData *>;
    sem_init(&qsem, 0, 0);
  

  }

  ~MulticastRP() {
	  pthread_mutex_destroy(&chunksendmtxlock);
	  pthread_mutex_destroy(&mtxlock);
  }

 
};


class MulticastChunkData{
public:
  const ChunkContext* xxct;
  char buf[XIA_MAXBUF];
  size_t count;
  int flags;
  std::vector< Graph* >* rcptList;
  ChunkInfo* info;

  
  MulticastChunkData(const ChunkContext* ixxct, const char* ibuf, size_t icount, 
		     int iflags, std::vector< Graph* >* ircptList, ChunkInfo* iinfo){
    xxct = ixxct;
    memset(buf, 0, sizeof(buf));
//     strncpy(buf, ibuf, count);
    strcpy(buf, ibuf);
    count = icount;
    flags = iflags;
    rcptList = ircptList;
    info = iinfo;
  }
  
  ~MulticastChunkData(){
    MulticastRP::RemoveVec(rcptList);
  }
  
};


//======================================================

void MulticastRP::EndhostJoin(std::string clean){
  
  Graph *g = new Graph(clean);
  Receiver *r = new Receiver(g, g);
  pthread_mutex_lock( &mtxlock );
  hosts->insert(std::pair<std::string, Receiver *>(clean, r));
  pthread_mutex_unlock( &mtxlock );
  
}

void MulticastRP::EndhostLeave(std::string clean){
  
 std::map<std::string, Receiver*>::iterator i =  hosts->find(clean);

  if (i == hosts->end()){
    say("This DAG doesn't exists");
    return;
  }
  
  pthread_mutex_lock( &mtxlock );
  delete i->second;
  hosts->erase(clean);
  pthread_mutex_unlock( &mtxlock );
  say("successfully deleted Endhost: %s\n", clean.c_str() );
  
}

void MulticastRP::SendToList(std::string cmd, std::vector<Graph*> *rcptList){
  
  for(std::vector<Graph*>::iterator it = rcptList->begin(); it != rcptList->end(); ++it) {
    
      int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
      sockaddr_x dag;
      (*it)->fill_sockaddr(&dag);
// 	iter->second->ControlDAG->fill_sockaddr(&dag);


      int rc = -1;
      if( (rc = Xsendto(sock, cmd.c_str(), strlen(cmd.c_str()), 0, (struct sockaddr*)&dag, sizeof(dag))) >= 0) {
	say("\nSent command: %s \n", cmd.c_str());
	if ((unsigned)rc != strlen(cmd.c_str()))
	  say("\nThe size of command was too big, partially sent.");
      }
      else{
 	  say("Failed to send command: %s \n", DGramDAG->dag_string().c_str());
      }
  
  }
  
  
}


void MulticastRP::MulticastToEndHosts(std::string cmd){
  std::vector<Graph *> *veccontrol = BuildEndhostControlRecvList();
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  
}


void MulticastRP::ParallelMulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info) {
  
  MulticastChunkData *mcd = new MulticastChunkData(xxct, buf, count, flags, rcptList, info);
  
  pthread_mutex_lock(&chunksendmtxlock);
  MulticastChunks->push(mcd);
  pthread_mutex_unlock(&chunksendmtxlock);
  sem_post(&qsem);
  
}

int MulticastRP::MulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info) {
  int rc = -1;
  warn("RP Mutlicast Chunk %s\n", info->cid);
  for(std::vector<Graph*>::iterator it = rcptList->begin(); it != rcptList->end(); ++it) {
    sockaddr_x cdag;
    (*it)->fill_sockaddr(&cdag);
    if ((rc = XpushChunkto(xxct, buf, count, flags, (struct sockaddr*)&cdag, sizeof(cdag), info)) < 0)
      die(-1, "Could not send chunk");
	
  }
  RemoveVec(rcptList);
  return rc;
}


template <typename Iter>  
void MulticastRP::DeletePointers(Iter begin, Iter end)
{
  for (; begin != end; ++begin) delete *begin;
}

template <typename T>
void MulticastRP::RemoveVec(std::vector<T *>* vec)
{
  DeletePointers(vec->rbegin(), vec->rend());
  vec->clear();
  delete vec;
}


std::vector<Graph *> *MulticastRP::BuildEndhostChunkRecvList(){
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Receiver*>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter){
    Graph *g;
    g = new Graph(iter->second->ControlDAG->dag_string());
        //Make sure it's not duplicate, probably better to use a set
    bool dup = false;
    std::vector<Graph*>::iterator i;
    for (i = vec->begin(); i != vec->end(); ++i){
      if(g->dag_string() == (*i)->dag_string())
	dup = true;
    }
    if(!dup)
      vec->push_back(g);
    else
      delete g;
  }
  return vec;
}


std::vector<Graph *> *MulticastRP::BuildEndhostControlRecvList(){
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Receiver*>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter){
    Graph *g;
    g = new Graph(iter->second->ControlDAG->dag_string());
        //Make sure it's not duplicate, probably better to use a set
    bool dup = false;
    std::vector<Graph*>::iterator i;
    for (i = vec->begin(); i != vec->end(); ++i){
      if(g->dag_string() == (*i)->dag_string())
	dup = true;
    }
    if(!dup)
      vec->push_back(g);
    else
      delete g;
  }
  return vec;
}



void MulticastRP::ChunkSendLoop(){
  while (1) {
    
    MulticastChunkData *mcd;
    sem_wait(&qsem);
    pthread_mutex_lock(&chunksendmtxlock);
    mcd = MulticastChunks->front();
    MulticastChunks->pop();
    //MulticastChunks.pop_front(someObjectReference);
    pthread_mutex_unlock(&chunksendmtxlock);
    
    int rc = -1;
    warn("RP Mutlicast Chunk %s\n", mcd->info->cid);
    for(std::vector<Graph*>::iterator it = mcd->rcptList->begin(); it != mcd->rcptList->end(); ++it) {
      sockaddr_x cdag;
      (*it)->fill_sockaddr(&cdag);
      if ((rc = XpushChunkto(mcd->xxct, mcd->buf, mcd->count, mcd->flags, (struct sockaddr*)&cdag, sizeof(cdag), mcd->info)) < 0)
	die(-1, "Could not send chunk");
	  
    }
      
    
    delete mcd;
    
  }

  Xclose(ChunkSock);
  pthread_exit(NULL);
}

void MulticastRP::ChunkLoop(){
  while (1) {

    char buf1[XIA_MAXBUF];
    ChunkInfo *info = (ChunkInfo*) malloc(sizeof(ChunkInfo));
    memset(buf1, 0, sizeof(buf1));

    warn("Will now listen for chunks\n");
    int received = -1;
    if((received = XrecvChunkfrom(ChunkSock, buf1, sizeof(buf1), 0, info)) < 0)
	    die(-5, "Receive error %d on socket %d\n", errno, ChunkSock);
    else{
	    
	    std::vector<Graph *> *recchunk = BuildEndhostChunkRecvList();
	    
	    
	    ParallelMulticastChunk(ctx, buf1, strlen(buf1), 0, recchunk, info);
// 	    int rc = -1;
// 	    if ((rc = MulticastChunk(ctx, buf1, strlen(buf1), 0, recchunk, info)) < 0)
// 	      warn("Could not send chunk");
	    
	    ChunkReceived(buf1, received, info);

	    // 	    RemoveVec(recchunk);    
// 	    warn("Received Chunk CID: %s\n", info->cid);
    }

    free(info);
    
  }

  Xclose(ChunkSock);
  pthread_exit(NULL);
}

bool MulticastRP::StartChunkLoop()
{
  return (pthread_create(&_thread, NULL, InternalChunkThread, this) == 0);
}

bool MulticastRP::StartControlLoop()
{
  return (pthread_create(&_thread2, NULL, InternalControlThread, this) == 0);
}

bool MulticastRP::StartChunkSendLoop()
{
  return (pthread_create(&_thread3, NULL, InternalChunkSendThread, this) == 0);
}

int MulticastRP::BuildChunkDAGs(ChunkStatus *cs, std::string chunks, std::string ad, std::string hid)
{
	
	
	// build the list of chunks to retrieve
	std::size_t prev_location = 0;
	std::size_t location = chunks.find_first_of("|");
	int n = 0;
	
	while(location != std::string::npos) {
		
		std::string c = chunks.substr(prev_location, location-prev_location);

		char *dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", ad.c_str(), hid.c_str(), c.c_str());
// 		say("%s\n", dag);
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

int MulticastRP::SendCommand(std::string cmd){

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


int MulticastRP::PullChunks( std::string &s, std::string chunks, Graph *g)
{
  
  int num_chunks  = std::count(chunks.begin(), chunks.end(), '|');
  say("numchunks: %d\n", num_chunks); 
  ChunkStatus *cs = new ChunkStatus[num_chunks];
  char data[XIA_MAXCHUNK];
  int len;
  int status;
  int n = -1;
  int csock = -1;// ChunkSock;
  
  
  std::string ad = g->dag_string().substr(SourceServiceDAG->dag_string().find("AD:"),43);
  say(("sad " + ad + "\n").c_str());
  std::string hid = g->dag_string().substr(SourceServiceDAG->dag_string().find("HID:"),44);
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

  
  if (status != READY_TO_READ)
    return -1;
  
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
// 	  say("CID: %s\n", cs[i].cid);
// 	  say("MCID: %s-\n", mcid.c_str());
// 	  say("strlen(info.cid): %d, strlen(mcid): %d\n", (CID_HASH_SIZE + 1), strlen(mcid.c_str()));
	  ChunkInfo info;
	  strcpy(info.cid, mcid.c_str());//, strlen(info.cid));
// 	  say("INFOCID: %s\n", info.cid);
	  info.size = len;
// 	  info.ttl= 1000;
// 	  info.timestamp.tv_sec  = 0;
// 	  info.timestamp.tv_usec = 0;
	  
	//TODO: so I get ChunkStatus and I want to pass Chunkinfo to the virtual ChunkReceived method. This is the problem. 
// 	  ChunkReceived(data, len, &info);
	  // write the chunk to disk
//		say("writing %d bytes of chunk %s to disk\n", len, cid);
// 		  fwrite(data, 1, len, fd);
	  s = s + std::string(data,len);

	  free(cs[i].cid);
// 		  cs[i].cid = NULL;
// 		  cs[i].cidLen = 0;
  }
  delete[] cs;
  return n;
}

// I think there may be a bug in XRequestChunks. Very fast large files cause problems with missing chunks for some reason
int MulticastRP::PullChunksOneByOne( std::string &s, std::string chunks, Graph *g)
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




int MulticastRP::PullPushChunksOneByOne( std::string &s, std::string chunks, Graph *g, std::vector< Graph* >* rcptList)
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
    
    ParallelMulticastChunk(ctx, data, len, 0, rcptList, &info);

    s = s + std::string(data,len);

    free(cs[j].cid);
    
  }
  
  delete[] cs;
  return n;
}


int MulticastRP::Join(std::string name){
    
      // lookup the xia service, can easily ignore this part and use a dag that is received through some other mechanism. 
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
    

  char joinmessage[512];
  sprintf(joinmessage, "joinrp|%s", DGramDAG->dag_string().c_str());
  
  int rc = -1;
  if( (rc = SendCommand(std::string(joinmessage)) ) >= 0) {
//       say("sent message: %s\n", DGramDAG->dag_string().c_str());
  }
  else{
	say("Couldn't send join message\n");
	die(0, DGramDAG->dag_string().c_str());
  }

  //TODO: For now it's unreliable. This can be added to make it reliable. Either use a stream socket. or manually send acks for commands. 
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



int MulticastRP::Leave(){
  char leavemessage[512];
  sprintf(leavemessage, "leaverp|%s", DGramDAG->dag_string().c_str());
  
  int rc = -1;
  if( (rc = SendCommand(std::string(leavemessage)) ) >= 0) {
//       say("sent message: %s\n", DGramDAG->dag_string().c_str());
  }
  else{
	say("Couldn't send leave message\n");
	die(0, DGramDAG->dag_string().c_str());
  }

  return rc;
  
}


void MulticastRP::ControlLoop(){

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
	//TODO: should get chunks first and then tell host to request?
	MulticastToEndHosts(st);
	
	say((st + "\n").c_str());
	std::string fs = st.substr(9, st.npos);
	int endoffname = fs.find_first_of("|");
	fname = "F" +postname + "-" + fs.substr(0, endoffname);
// 	say( (fname+ "\n").c_str());	  

	
	std::string chunkhashes = fs.substr(endoffname+1, fs.npos);
// 	say( (chunkhashes+ "\n").c_str());	
 
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
		//TODO: Maybe do something more complicated
// 		MulticastToEndHosts("pullchunks|"+cc);
		
		fwrite(result.c_str(), sizeof(char), result.size(), f);
//  		  say((result + "\n").c_str() );
		
	}
	
	fclose(f);
	say("File done: %s\n", fname.c_str());
	fname = "";
      }
      else if(strncmp("chunks|", buf,7) == 0){
	std::string st(buf);
	MulticastToEndHosts(st);
	
	std::string clean = st.substr(7, clean.npos);
// 	say((clean + "\n").c_str());
	int endofhash = clean.find_first_of("|");
	std::string hash = clean.substr(0, endofhash);
	chunksLists->insert( std::pair<std::string, std::string>(hash, clean.substr(endofhash+1, clean.npos)) );

      }
      else if(strncmp("pullchunks|", buf, 11) == 0){
	std::string st(buf);
	//End host pulls chunks
	MulticastToEndHosts(st);
	//TODO: decide what should RP do?
// 	std::string clean = st.substr(11, clean.npos);
// 	std::string result;
// 	PullChunks( result, clean.c_str(), SourceServiceDAG);
      }
      else if(strncmp("rppullchunks|", buf, 13) == 0){
	//RP pulls chunks
	std::string st(buf);
	std::string clean = st.substr(13, clean.npos);
	std::string result;
	PullChunksOneByOne( result, clean.c_str(), SourceServiceDAG);
      }
      else if(strncmp("pullchunksrp|", buf, 13) == 0){
	//Endhosts pull chunks from the RP
	std::string st(buf);
	MulticastToEndHosts(st);
      }
      else if(strncmp("setrp|", buf, 6) == 0){
	std::string st(buf);
	say("setrp, This should not happen unless RPs are used to set RPs in rpjoin\n");
	MulticastToEndHosts(st);
      }
      else if(strncmp("removerp|", buf, 9) == 0){
	std::string st(buf);
	say("removerp, This should not happen\n");
	MulticastToEndHosts(st);
	hosts->clear();

      }
      else if(strncmp("ehlist|", buf, 7) == 0){
	std::string st(buf);
	std::string clean = st.substr(7, clean.npos);
	
	
	std::size_t prev_location = 0;
	std::size_t location = clean.find_first_of("|");
	
	while(location != std::string::npos) {
		
		std::string c = clean.substr(prev_location, location-prev_location);
		EndhostJoin(c);
		prev_location = location + 1;
		location = clean.find_first_of("|", prev_location);
	}
	

      }
      else if(strncmp("endhostjoin|", buf, 12) == 0){
	std::string st(buf);
	std::string clean = st.substr(12, clean.npos);
	EndhostJoin(clean);
      }
      else if(strncmp("endhostleft|", buf, 12) == 0){
	std::string st(buf);
	std::string clean = st.substr(12, clean.npos);
	EndhostLeave(clean);
      }
      else if(strncmp("rppushchunks|", buf, 13) == 0){
	//RP pulls chunks
	std::string st(buf);
	std::string clean = st.substr(13, clean.npos);
	std::string result;
	
	PullPushChunksOneByOne(result, clean, SourceServiceDAG, BuildEndhostChunkRecvList());
	say("not comppleted. \n");
	//Buildchunkdags, readsthem, send them
	//PullChunks( result, clean.c_str(), SourceServiceDAG);
      }
      else{
	std::string st(buf);
	
	die(-1,"RP received invalid command: %s\n", st.c_str());
      }
      
// 	say("Dgram Server waiting\n");
// 	say(">> ");

    }

    Xclose(DGramSock);
	
}



void MulticastRP::InitializeClient(std::string mySID)
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
  ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 200000000);
  if (ctx == NULL)
      die(-2, "Unable to initilize the chunking system\n");
    
    
    
//       char my_csid[] = "00000000dd41b924c1001cfa1e1117a812492411";
  char CDAG[500];
  // Using same SID for chunk and DGram DAG. This should probably be different. But it works as the bind for chunk and DGram is different.
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
//   RPServiceDAG = SourceServiceDAG;
  
  StartChunkLoop();
  StartChunkSendLoop();

}



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++


//Abtract Methods


void MulticastRP::ChunkReceived(char* buf, size_t len, ChunkInfo* info)
{
  //Do something with the chunk
  if(len == 0 || buf == NULL)
    warn("There was an error receiving the chunk");
  say("Received Chunk %s\n", info->cid);
  
  std::string localfname = "T" +postname + ".tmp";
  say( (fname+ "\n").c_str());	  
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
  MulticastRP *mrp = new MulticastRP(std::string(name)); //:-)
  //TODO make this random
  mrp->InitializeClient(std::string("00000000dd41b924c1001cfa1e1117a812492444"));
  mrp->Join(std::string(NAME));
  say("AFTER JOIN\n");
//   sleep(25);
//   mrp->Leave();
  
  while(1){
  }
  
}

