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

extern int Xsendto(int sockfd,const void *buf, size_t len, int flags,char * dDAG, size_t dlen);
extern int Xrecvfrom(int sockfd,void *buf, size_t len, int flags,char * dDAG, size_t *dlen);
extern int Xsocket();
extern int Xconnect(int sockfd, char* dest_DAG);
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *rbuf, size_t len, int flags);
extern int Xsend(int sockfd, const void *wbuf, size_t len, int flags);
extern int XgetCID(int sockfd, char* dDAG, size_t dlen);
extern int XputCID(int sockfd, const void *wbuf, size_t len, int flags,char* sDAG, size_t dlen);
extern int Xaccept(int sockfd);
extern void set_conf(char *filename, char *sectioname);
extern void print_conf();

