%module xsocket
%{
extern int Xsendto(int sockfd,const void *buf, size_t len, int flags,char * dDAG, size_t dlen);
extern int Xrecvfrom(int sockfd,void *buf, size_t len, int flags,char * dDAG, size_t *dlen);
extern int Xsocket();
extern int Xconnect(int sockfd, char* dest_DAG);
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *buf, size_t len, int flags);
extern int Xsend(int sockfd,void *buf, size_t len, int flags);
extern int XgetCID(int sockfd, char* dDAG, size_t dlen);
extern int XputCID(int sockfd,void *buf, size_t len, int flags,char* sDAG, size_t dlen);
extern int Xaccept(int sockfd);
%}


extern int Xsendto(int sockfd,const void *buf, size_t len, int flags,char * dDAG, size_t dlen);
extern int Xrecvfrom(int sockfd,void *buf, size_t len, int flags,char * dDAG, size_t *dlen);
extern int Xsocket();
extern int Xconnect(int sockfd, char* dest_DAG);
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *buf, size_t len, int flags);
extern int Xsend(int sockfd,void *buf, size_t len, int flags);
extern int XgetCID(int sockfd, char* dDAG, size_t dlen);
extern int XputCID(int sockfd,void *buf, size_t len, int flags,char* sDAG, size_t dlen);
extern int Xaccept(int sockfd);
