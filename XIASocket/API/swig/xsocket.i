%module xsocket
%{
#include  "../Xsocket.h"
%}


%typemap (in) const void *
{
    $1 = (const void*)PyString_AsString($input);
}

%typemap (in) (void * rbuf, size_t len)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer");
        return NULL;
    }
    $2 = PyInt_AsLong($input);
    if ($2<0) {
        PyErr_SetString(PyExc_ValueError, "Positive integer expected");
        return NULL;
    }
    $1= (void*)malloc($2);

}
%typemap(argout) (void *rbuf, size_t len) 
{
    Py_XDECREF($result);
    if (result < 0) {
        free($1);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    $result = PyString_FromStringAndSize($1, result);
    free($1);
}

/* functions */
extern int Xsendto(int sockfd,const void *buf, size_t len, int flags,char * dDAG, size_t dlen);
extern int Xrecvfrom(int sockfd,void *rbuf, size_t len, int flags,char * dDAG, size_t *dlen);
extern int Xsocket(int transport_type); /* 0: Reliable transport (SID), 1: Unreliable transport (SID), 2: Content Chunk transport (CID) */
extern int Xconnect(int sockfd, char* dest_DAG);
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *rbuf, size_t len, int flags);
extern int Xsend(int sockfd,const void *buf, size_t len, int flags);

extern int XgetCID(int sockfd, char* cDAG, size_t dlen);
extern int XgetCIDList(int sockfd, const struct cDAGvec *cDAGv, int numCIDs);
extern int XgetCIDStatus(int sockfd, char* cDAG, size_t dlen);
extern int XgetCIDListStatus(int sockfd, struct cDAGvec *cDAGv, int numCIDs);
extern int XreadCID(int sockfd, void *rbuf, size_t len, int flags, char * cDAG, size_t dlen);

extern int XputCID(int sockfd, const void *buf, size_t len, int flags,char* sDAG, size_t dlen);
extern int Xaccept(int sockfd);
extern int Xgetsocketidlist(int sockfd, int *socket_list);
extern int Xgetsocketinfo(int sockfd1, int sockfd2, struct Netinfo *info);
extern void error(const char *msg);
extern void set_conf(const char *filename, const char *sectioname);
extern void print_conf();

/* constants */
#define ATTEMPTS 100 //Number of attempts at opening a socket 
#define MAXBUFLEN 2000 // Note that this limits the size of chunk we can receive

#define XSOCK_STREAM 0 // Reliable transport (SID)
#define XSOCK_DGRAM 1 // Unreliable transport (SID)
#define XSOCK_CHUNK 2 // Content Chunk transport (CID)

#define WAITING_FOR_CHUNK 0
#define READY_TO_READ 1
#define REQUEST_FAILED -1
