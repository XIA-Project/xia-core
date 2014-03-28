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
#include <assert.h>
#include <map>
#include "dagaddr.hpp"
// #include "string.h"
#include <vector>
#include <sys/stat.h>

#include <iostream>
#include <functional>
#include <string>
#include <algorithm>
#include <queue>
#include <semaphore.h>

// #include <../../click/include/click/hashtable.hh>


#define VERSION "v1.0"
#define TITLE "XIA Multicast Source"

#define MAX_XID_SIZE 100
#define DGRAM_SID "SID:00000000dd41b924c1001cfa1e1117a812492434"
#define DGRAM_NAME "www_s.multicast.aaa.xia"
#define NUM_PROMPTS	1

#define CHUNKSIZE (XIA_MAXBUF - 1000)
#define NXIDS 80 //number of XIDS per masterchunk. Large numbers will cause problems


int verbose = 1;
bool quick = false;


int getFile(int sock, char *ad, char*hid, const char *fin, const char *fout);
void say(const char *fmt, ...);
void warn(const char *fmt, ...);
void die(int ecode, const char *fmt, ...);
void usage();


bool file_exists(const char * filename);
template<typename Container>
void delete_them(Container& c) { while(!c.empty()) delete c.back(), c.pop_back(); }


//======================================================
class Receiver{
  public:
  
  Graph *ControlDAG;
  Graph *ChunkDAG;
  Graph *RP;
  
 
  Receiver(Graph *control, Graph *chunk, Graph *rp=NULL){
    ControlDAG = control;
    ChunkDAG = chunk;
    RP = rp;

  }
};
class MulticastChunkData;

class MulticastSource{
  private:
    static void * InternalControlThread(void * This) {((MulticastSource *)This)->ControlLoop(); return NULL;}  
    static void * InternalChunkSendThread(void * This) {((MulticastSource *)This)->ChunkSendLoop(); return NULL;}
    
    pthread_t _thread;
    pthread_t _thread2;
    sem_t qsem;
    pthread_mutex_t  mtxlock;
    pthread_mutex_t  chunksendmtxlock;

    Graph *DGramDAG;
    std::map<std::string, Receiver *> *hosts; 
    std::map<std::string, Graph *> *RPs;
    
    std::string DGramName;
    std::string DGramSID;
    int ChunkSock, DGramSock;
    ChunkContext *ctx;
    
    int NumXIDs;
    unsigned int ChunkSize;
    std::queue<MulticastChunkData *> *MulticastChunks;
    

    bool StartControlLoop();
    bool StartChunkSendLoop();
    void EndhostJoin(std::string dags);
    void EndhostLeave(std::string dags);
    void RPJoin(std::string dags);
    void RPLeave(std::string dags);
    void SendToList(std::string cmd, std::vector<Graph*> *rcptList);
    int  PushFileto(const ChunkContext* xxct, const char* fname, int flags, std::vector< Graph* >* rcptList, ChunkInfo** info, unsigned int chunkSize, std::string& res);
    int  SendCommand(std::string cmd, Graph *g);
  
  protected: 
    void ControlLoop();
    void ChunkSendLoop();
  
  public:
    std::vector<Graph *> *BuildControlRecvList();
    std::vector<Graph *> *BuildChunkRecvList();
    std::vector<Graph *> *BuildEndHostControlRecvList();
    std::vector<Graph *> *BuildRPsControlRecvList();
    std::vector<Graph *> *BuildEndHostListForRP( std::string rp);
    
        //private methods
    template <typename Iter>  
    static void DeletePointers(Iter begin, Iter end);
    template <typename T>
    static void RemoveVec(std::vector<T *>* vec);
    
    void Multicast(std::string);
    void MulticastFile(std::string fin);
    void PutFile(std::string fin);
    int  MulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info);
    void ParallelMulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info);
    void Initialize();
    void MulticastToEndHosts(std::string cmd);
    void MulticastToRPs(std::string cmd);
    void PullChunks(std::string cmd);
    void RPPullChunks(std::string cmd);
    void RPPushChunks(std::string cmd);
    void PullChunksRP(std::string cmd);
  
    MulticastSource(std::string sn, std::string sid, unsigned int cs = CHUNKSIZE, int NXIDs=NXIDS ){
      pthread_mutex_init(&mtxlock, NULL);
      pthread_mutex_init(&chunksendmtxlock, NULL);
      hosts = new std::map<std::string, Receiver *>;
      RPs = new std::map<std::string, Graph *>;
      MulticastChunks = new std::queue<MulticastChunkData *>;
      
      DGramName = sn;
      DGramSID = sid;
      NumXIDs = NXIDs;
      
      if(NXIDs > NXIDS)
	NumXIDs = NXIDS;

      ChunkSize= cs;
      if(cs > (XIA_MAXBUF-500))
	ChunkSize = XIA_MAXBUF - 500;
    }

	~MulticastSource() {
		pthread_mutex_destroy(&mtxlock);
		pthread_mutex_destroy(&chunksendmtxlock);
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
    MulticastSource::RemoveVec(rcptList);
  }
  
};
//======================================================


template <typename Iter>  
void MulticastSource::DeletePointers(Iter begin, Iter end)
{
  for (; begin != end; ++begin) delete *begin;
}

template <typename T>
void MulticastSource::RemoveVec(std::vector<T *>* vec)
{
  DeletePointers(vec->rbegin(), vec->rend());
  vec->clear();
  delete vec;
}

bool MulticastSource::StartControlLoop()
{
  return (pthread_create(&_thread, NULL, InternalControlThread, this) == 0);
}

bool MulticastSource::StartChunkSendLoop()
{
  return (pthread_create(&_thread2, NULL, InternalChunkSendThread, this) == 0);
}

std::vector<Graph *> *MulticastSource::BuildControlRecvList(){
  
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Receiver*>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter){
// 	say("send to: %s", iter->second->ControlDAG->dag_string().c_str());
    Graph *g;
    if(iter->second->RP == NULL)
	g = new Graph(iter->second->ControlDAG->dag_string());
    else{
	g = new Graph(iter->second->RP->dag_string());
// 	say("RP is : %s", g->dag_string().substr(g->dag_string().find("HID:"), 44).c_str());
    }
    
    
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


std::vector<Graph *> *MulticastSource::BuildEndHostListForRP( std::string rp){
  
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Receiver *>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter)
    if(iter->second->RP != NULL)
      if(iter->second->RP->dag_string() == rp)
      {
	Graph *g = new Graph(iter->second->ControlDAG->dag_string());
	
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

std::vector<Graph *> *MulticastSource::BuildEndHostControlRecvList(){
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

std::vector<Graph *> *MulticastSource::BuildRPsControlRecvList(){
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Graph*>::iterator iter;

  for (iter = RPs->begin(); iter != RPs->end(); ++iter){
    Graph *g;
    g = new Graph(iter->second->dag_string());
    
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

std::vector<Graph *> *MulticastSource::BuildChunkRecvList(){
  
  std::vector<Graph *> *vec = new std::vector<Graph*>();
  std::map<std::string, Receiver*>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter){
// 	say("send to: %s", iter->second->ControlDAG->dag_string().c_str());
    Graph *g;
    if(iter->second->RP == NULL)
      g = new Graph(iter->second->ChunkDAG->dag_string());
    else
      g = new Graph(iter->second->RP->dag_string());
    
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
  
  std::map<std::string, Graph*>::iterator ip;
  for (ip = RPs->begin(); ip != RPs->end(); ++ip){
// 	say("send to: %s", iter->second->ControlDAG->dag_string().c_str());
    Graph *g;
    if(ip->second != NULL)
      g = new Graph(ip->second->dag_string());

    
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


int MulticastSource::SendCommand(std::string cmd, Graph* g){

  int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
  sockaddr_x dag;
  g->fill_sockaddr(&dag);


  int rc = -1;
  if( (rc = Xsendto(sock, cmd.c_str(), strlen(cmd.c_str()), 0, (struct sockaddr*)&dag, sizeof(dag))) >= 0) {
     say("\nSent message: %s \n", cmd.c_str());
  }else{
    say("\nFailed to send: %s \n", cmd.c_str());
  }
  
    
return rc;

}


void MulticastSource::SendToList(std::string cmd, std::vector<Graph*> *rcptList){
  
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


void MulticastSource::EndhostJoin(std::string dags){
  
  say("Join DAG: %s\n", dags.c_str());
  Graph *g = new Graph(dags);
  Receiver *r = new Receiver(g, g);
  
  std::map<std::string, Graph*>::iterator iter;

  for (iter = RPs->begin(); iter != RPs->end(); ++iter){
    std::string ad1 = dags.substr(dags.find("AD:"),43);
    
    if(iter->second != NULL){
      std::string ad2 = iter->second->dag_string().substr(iter->second->dag_string().find("AD:"),43);
      if(ad1 == ad2){
	r->RP = iter->second;
	say("RP for this new endhost is: %s\n", iter->second->dag_string().c_str());
	break;
      }
    } 
  
  }
  
  
  if(r->RP != NULL){
    std::string cmd = "endhostjoin|"+dags;
    say((cmd+"\n").c_str());
    SendCommand(cmd, r->RP);
    
    cmd = "setrp|" + r->RP->dag_string();
    say((cmd+"\n").c_str());
    SendCommand(cmd, g);
  }
  
  
  
  pthread_mutex_lock( &mtxlock );
  hosts->insert(std::pair<std::string, Receiver *>(dags, r));
  pthread_mutex_unlock( &mtxlock );
  
  
//     std::map<std::string, Receiver*>::iterator i =  hosts->find(dags);
//     say("inserted %s", i->second->ControlDAG->dag_string().c_str());

  //TODO: for now it's unreliable with no confirmation. This can be added later.
//     sockaddr_x dag;
//     g->fill_sockaddr(&dag);
//     
//     char joinmessage[512];
//     sprintf(joinmessage, "joinc ok|%s", DGramDAG->dag_string().c_str());
//     int rc = -1;
//     if( (rc = Xsendto(DGramSock, joinmessage, strlen(joinmessage), 0, (struct sockaddr*)&dag, sizeof(dag))) >= 0) {
//       say("sent message: %s", DGramDAG->dag_string().c_str());
//     }
  
  

}


void MulticastSource::RPLeave(std::string dags){
  
  say("Leave RPDAG: %s\n", dags.c_str());
  
  std::map<std::string, Graph*>::iterator i =  RPs->find(dags);

  if (i == RPs->end()){
    say("This DAG doesn't exists\n");
    return;
  }
  
  std::vector<Graph *> *veccontrol = BuildEndHostListForRP(dags);
  std::map<std::string, Receiver*>::iterator iter;

  for (iter = hosts->begin(); iter != hosts->end(); ++iter){
    std::string ad1 = dags.substr(dags.find("AD:"),43);
//     say("checking for host  %s\n", iter->second->ControlDAG->dag_string().c_str());
    
    if(iter->second->RP != NULL){
      std::string ad2 = iter->second->RP->dag_string().substr(iter->second->RP->dag_string().find("AD:"),43);
//       say("ad1: %s, ad2: %s\n",ad1.c_str(), ad2.c_str());
      if(ad1 == ad2){
//  	say("deleting RP now for %s\n", iter->second->ControlDAG->dag_string().c_str());
	iter->second->RP = NULL;

      }
    } 
    
  }
  
//   say("deleted RP now deleting from map\n");
  
  delete i->second;
  pthread_mutex_lock( &mtxlock );
  RPs->erase(dags);
  pthread_mutex_unlock( &mtxlock );
  
  std::string cmd = "removerp";  
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  

  
  say("successfully deleted Endhost: %s\n", dags.c_str() );
  
}

void MulticastSource::RPJoin(std::string dags){
  
  say("RPJoin DAG: %s\n", dags.c_str());
  Graph *g = new Graph(dags);
//   Receiver *r = new Receiver(g, g);
  
  
  pthread_mutex_lock( &mtxlock );
  RPs->insert(std::pair<std::string, Graph *>(dags, g));
  pthread_mutex_unlock( &mtxlock );
  
  std::map<std::string, Receiver*>::iterator iter;
  std::string cmd = "ehlist|";
  for(iter = hosts->begin(); iter != hosts->end(); ++iter){
    std::string ad1 = dags.substr(dags.find("AD:"),43);
    std::string ad2 = iter->second->ControlDAG->dag_string().substr(iter->second->ControlDAG->dag_string().find("AD:"),43);
    
    if(iter->second->RP == NULL){
      if(ad1 == ad2){
	iter->second->RP = g;
	cmd = cmd + iter->second->ControlDAG->dag_string() + "|";
	say("became RP for: %s\n" , iter->second->ControlDAG->dag_string().c_str() );
      }
    } else{
      say("In the same AD as another RP. Existing: %s\n" , iter->second->RP->dag_string().c_str() );
      //TODO handle this case
    }
    
  }
  SendCommand(cmd, g);
  
  //This can be done through the RP as well
  cmd = "setrp|" + dags;
  std::vector<Graph *> *veccontrol = BuildEndHostListForRP(dags);
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  
  
}


void MulticastSource::EndhostLeave(std::string dags){
  
  say("Leave DAG: %s\n", dags.c_str());
//   Graph *g = new Graph(dags);
  
  std::map<std::string, Receiver*>::iterator i =  hosts->find(dags);
//   say("inserted %s", i->second->ControlDAG->dag_string().c_str());
  if (i == hosts->end()){
    say("This DAG doesn't exists");
    
//     std::map<std::string, Receiver*>::iterator iter;
//     for (iter = hosts->begin(); iter != hosts->end(); ++iter)
//       say((iter->first).c_str());
    return;
  }
  
  pthread_mutex_lock( &mtxlock );
  if(i->second->RP != NULL){
    std::string cmd = "endhostleft|"+dags;
    say((cmd+"\n").c_str());
    SendCommand(cmd, i->second->RP);
  }
   delete i->second;
  hosts->erase(dags);
  pthread_mutex_unlock( &mtxlock );
  
  
  say("successfully deleted Endhost: %s\n", dags.c_str() );
  
}

void MulticastSource::ParallelMulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info) {
  
  MulticastChunkData *mcd = new MulticastChunkData(xxct, buf, count, flags, rcptList, info);
  
  pthread_mutex_lock(&chunksendmtxlock);
  MulticastChunks->push(mcd);
  pthread_mutex_unlock(&chunksendmtxlock);
  sem_post(&qsem);
  
}


int MulticastSource::MulticastChunk(const ChunkContext* xxct, const char* buf, size_t count, int flags, std::vector< Graph* >* rcptList, ChunkInfo* info) {
  int rc = -1;
  for(std::vector<Graph*>::iterator it = rcptList->begin(); it != rcptList->end(); ++it) {
    sockaddr_x cdag;
    (*it)->fill_sockaddr(&cdag);
    if ((rc = XpushChunkto(xxct, buf, count, flags, (struct sockaddr*)&cdag, sizeof(cdag), info)) < 0)
      die(-1, "Could not send chunk");
	
  }
  return rc;
}

//essentially the same as API XpushFileto but slightly modified to add latency, etc.
int MulticastSource::PushFileto(const ChunkContext* xxct, const char* fname, int flags, std::vector< Graph* >* rcptList, ChunkInfo** info, unsigned int chunkSize, std::string& res)
{
    FILE *fp;
    struct stat fs;
    ChunkInfo *infoList;
    unsigned numChunks;
    unsigned i;
    int rc;
    int count;
    char *buf;

    if (xxct == NULL) {
	    errno = EFAULT;
	    return -1;
    }

    if (fname == NULL) {
	    errno = EFAULT;
	    return -1;
    }

    if (chunkSize == 0)
	    chunkSize =  DEFAULT_CHUNK_SIZE;
    else if (chunkSize > XIA_MAXBUF)
	    chunkSize = XIA_MAXBUF;

    if (stat(fname, &fs) != 0)
	    return -1;

    if (!(fp= fopen(fname, "rb")))
	    return -1;

    numChunks = fs.st_size / chunkSize;
    if (fs.st_size % chunkSize)
	    numChunks ++;
    //FIXME: this should be numChunks, sizeof(ChunkInfo)
    if (!(infoList = (ChunkInfo*)calloc(numChunks, sizeof(ChunkInfo)))) {
	    fclose(fp);
	    return -1;
    }

    if (!(buf = (char*)malloc(chunkSize))) {
	    free(infoList);
	    fclose(fp);
	    return -1;
    }

    i = 0;
// 	std::string st = "";
    while (!feof(fp)) {
    
	    if ((count = fread(buf, sizeof(char), chunkSize, fp)) > 0) {
// 	      for(std::vector<Graph*>::iterator it = rcptList->begin(); it != rcptList->end(); ++it) {
// 		sockaddr_x cdag;
// 		(*it)->fill_sockaddr(&cdag);
// 	      sleep(1);
	      if ((rc = MulticastChunk(xxct, buf, count, flags, rcptList, &infoList[i])) < 0)
		      break;
	      res = res + infoList[i].cid +"|";
	      say("Chunk: %d\n", i);
// 	      }
	      i++;
	    }
    }

    if (i != numChunks) {
	    // FIXME: something happened, what do we want to do in this case?
	    rc = -1;
    }
    else
	    rc = i;

    *info = infoList;
    fclose(fp);
    free(buf);

    return rc;
}

void MulticastSource::ChunkSendLoop(){
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

void MulticastSource::ControlLoop()
{
  say("Dgram Server waiting");
  while (1) {

    char buf[XIA_MAXBUF];
    sockaddr_x cdag;
    socklen_t dlen;
    int n;


    dlen = sizeof(cdag);
    memset(buf, 0, sizeof(buf));
    if ((n = Xrecvfrom(DGramSock, buf, sizeof(buf), 0, (struct sockaddr *)&cdag, &dlen)) < 0) {
	    warn("Recv error on socket %d, closing connection\n", DGramSock);
	    break;
    }

    say("DGram received %d bytes, Text: %s\n", n, buf);
    
    if(strncmp("joinc|", buf,6) == 0){
      std::string st(buf);
      std::string dag = st.substr(6);
      EndhostJoin(dag);
      
    }else if(strncmp("leave|", buf, 6) == 0){
      std::string st(buf);
      std::string dag = st.substr(6);
      EndhostLeave(dag);
    }
    else if(strncmp("leaverp|", buf, 8) == 0){
      std::string st(buf);
      std::string dag = st.substr(8);
      RPLeave(dag);
    }
    else if(strncmp("joinrp|", buf, 7) == 0){
      std::string st(buf);
      std::string dag = st.substr(7);
      RPJoin(dag);
    }
// 	else if(strncmp("joinc|", buf,6) == 0){
// 	}
    say("Dgram Server waiting");

  }

  Xclose(DGramSock);
  pthread_exit(NULL);
      
}


void MulticastSource::Multicast(std::string cmd){
  std::vector<Graph *> *veccontrol = BuildControlRecvList();
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  
}


void MulticastSource::MulticastToEndHosts(std::string cmd){
  std::vector<Graph *> *veccontrol = BuildEndHostControlRecvList();
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  
}

void MulticastSource::MulticastToRPs(std::string cmd){
  std::vector<Graph *> *veccontrol = BuildRPsControlRecvList();
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  
}


// chunkslist in format cid|cid|cid|...|cid|
void MulticastSource::RPPullChunks(std::string chunkslist){
  std::vector<Graph *> *veccontrol = BuildRPsControlRecvList();
  std::string cmd = "rppullchunks|" + chunkslist;
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol); 
}

void MulticastSource::RPPushChunks(std::string chunkslist){
  std::vector<Graph *> *veccontrol = BuildRPsControlRecvList();
  std::string cmd = "rppushchunks|" + chunkslist;
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol); 
}


// chunkslist in format cid|cid|cid|...|cid|
void MulticastSource::PullChunks(std::string chunkslist){
  std::vector<Graph *> *veccontrol = BuildControlRecvList();
  std::string cmd = "pullchunks|" + chunkslist;
  
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol); 
}

void MulticastSource::PullChunksRP(std::string chunkslist){
  std::vector<Graph *> *veccontrol = BuildControlRecvList();
  std::string cmd = "pullchunksrp|" + chunkslist;
  
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
}


void MulticastSource::PutFile(std::string fin){
  
  if(!file_exists(fin.c_str())){
	  warn("Source file: %s doesn't exist\n", fin.c_str());
	  return;
  }

  ChunkInfo *info = NULL;
  std::string res;
  XputFile(ctx, fin.c_str(),  ChunkSize, &info);
}

void MulticastSource::MulticastFile(std::string fin){
  
  if(!file_exists(fin.c_str())){
	  warn("Source file: %s doesn't exist\n", fin.c_str());
	  return;
  }


  ChunkInfo *info = NULL;
  std::string res;
  
  std::vector<Graph *> *recchunk = BuildChunkRecvList();
  //TODO: maybe this buffer is not enough?!
  //ChunkContext contains size, ttl, policy, and contextID which for now is PID
//   ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
  
  PushFileto(ctx, fin.c_str(), 0, recchunk, &info, ChunkSize, res);
  
  
//   die(-1, "die");
  
//   sleep(1);
//   say(("PUSHFILE: " + res + "\n" ).c_str());
  
  // TODO:  should use SHA1 instead.
  std::size_t h1 = std::hash<std::string>()(res);
  char hash[100];//sizeof(size_t)+1];
  sprintf(hash, "%ld", h1);
  std::string mhash = std::string(hash);
  say(( "HASH:" + mhash + "\n").c_str());
  

  std::size_t size = res.size();
  std::size_t read = 0;
  std::size_t xid_size = std::string("00000000dd41b924c1001cfa1e1117a812492434|").size();
  
  std::string masterchunk;
  //This is used instead of multicast method to avoid having different recptlists when someone joins in the middle of doing this stuff
  std::vector<Graph *> *veccontrol = BuildControlRecvList();

  
  
  while(read < size ){
//       XIA_MAXBUF/xid_size -200
//     int NumXIDs = 2;
    std::string s = res.substr( read, xid_size*NumXIDs );
    read = read + xid_size*NumXIDs;
    
    
    
    std::size_t hres = std::hash<std::string>()(s);
    char hashres[100];//sizeof(size_t)+1];
    sprintf(hashres, "%ld", hres);
    std::string chunklisthash = std::string(hashres);
      
    
    std::string chunks = "chunks|" + chunklisthash + "|" + s;
    masterchunk = masterchunk + chunklisthash + "|";
    say(chunks.c_str());
    
    
    SendToList(chunks, veccontrol);
    
  }
  
  //assuming it's not bigger than XIA_MAXBUF size
  std::string cmd = "recvfile|" + fin + "|" + masterchunk;
//   say(cmd.c_str());
//     std::vector<Graph *> *veccontrol = BuildControlRecvList();
  SendToList(cmd, veccontrol);
  RemoveVec(veccontrol);
  RemoveVec(recchunk);

  
}



void MulticastSource::Initialize(){
    
    say ("\n%s (%s): started\n", TITLE, VERSION);
    say("Datagram service started\n");

    // create a socket, and listen for incoming connections
    if ((DGramSock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
	    die(-1, "Unable to create the listening socket\n");

    struct addrinfo *ai;
    if (Xgetaddrinfo(NULL, DGramSID.c_str(), NULL, &ai) != 0) 
	    die(-1, "getaddrinfo failure!\n");
    
    sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
    DGramDAG =  new Graph(dag);
    printf("\nDatagram DAG\n%s\n", DGramDAG->dag_string().c_str());
	
	
    if (XregisterName(DGramName.c_str(), dag) < 0 )
	die(-1, "error registering name: %s\n", DGramName.c_str());

    if (Xbind(DGramSock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
	    Xclose(DGramSock);
	    die(-1, "Unable to bind to the dag: %s\n", dag);
    }
    
    //-------------Chunk Stuff
    if ((ChunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
	    die(-1, "unable to create chunk socket\n");
    
    ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 200000000);
    if (ctx == NULL)
	    die(-2, "Unable to initilize the chunking system\n");
    
    // IF receiving but we don't receive for now
//       if (XbindPush(chunkSock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
// 	      Xclose(chunkSock);
// 		die(-1, "Unable to bind to the dag: %s\n", dag);
//       }	
    
    StartControlLoop();

}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++



void usage(){
	say("usage: put <source file> \n");
}

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		vprintf("\n>>", args);
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


bool file_exists(const char * filename)
{
    if (FILE * file = fopen(filename, "r")){
	fclose(file);
	return true;
	}
    return false;
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

int main()
{	
    MulticastSource *m;
    m = new MulticastSource(DGRAM_NAME, DGRAM_SID);
    m->Initialize();


//	This is for quick testing with a couple of commands
    int i = 0;

    char fin[512];//, fout[512];
    char cmd[512], reply[512];
    int params = -1;
    
    sleep(1);
    while(i < NUM_PROMPTS){
	    cmd[0] = '\n';
	    fin[0] = '\n';
// 	    fout[0] = '\n';
	    params = -1;
	    
	    if(quick){
		    if( i==0 )
			    strcpy(cmd, "put s.txt");
// 		    else if( i==1 )
// 			    strcpy(cmd, "get r.txt sr.txt\n");
		    i++;
	    }else{
		    if( fgets(cmd, 511, stdin) == NULL)
		      die(0, "No input, exisitng");
	    }

//		enable this if you want to limit how many times this is done
// 		i++;
	    

	    if (strncmp(cmd, "put", 3) == 0){
		    params = sscanf(cmd,"put %s", fin);


		    if(params !=1 ){
			    sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
			    warn(reply);
			    usage();
			    continue;
		    }

// 		    m->PutFile(std::string(fin));
// 		    m->PullChunks("f9979c33a2a74020d854fbc138696c5c249edc16|4715a6373ced400b37a63b07c609313e39d488d3|335c7b4ab714a29280ad47fc3d9060161dfefb73");
//  		    m->RPPullChunks("f9979c33a2a74020d854fbc138696c5c249edc16|4715a6373ced400b37a63b07c609313e39d488d3|335c7b4ab714a29280ad47fc3d9060161dfefb73");
// 		    sleep(10);
//  		    m->PullChunksRP("f9979c33a2a74020d854fbc138696c5c249edc16|4715a6373ced400b37a63b07c609313e39d488d3|335c7b4ab714a29280ad47fc3d9060161dfefb73");
		    
  		    m->MulticastFile(std::string(fin) );
		  
		    
	    }
	    else{
		    sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
		    warn(reply);
		    usage();
	    }
	    
    }	
	

	
    delete m;
    return 0;
}
