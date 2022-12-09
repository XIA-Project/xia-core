/***************************************************************************
 *            connections.c
 *
 *  Copyright 2005 Dimitur Kirov
 *  dkirov@gmail.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h> 
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
#include "defines.h"
#include "cmdparser.h"
#include "fileutils.h"

#include "Xsocket.h"

#define MAX_XID_SIZE 100
#define DAG  "RE %s %s %s"
#define STREAM_NAME "www_s.stream_tinyftp.aaa.xia"

#define SID_STREAM  "SID:0f00000000000000000000000000000000088888"


static char *createDAG(const char *, const char *, const char *);

int open_connections;
bool max_limit_notify;

int raiseerr(int err_code) {
	printf("Error %d\n",err_code);
	return -1;
}

/** 
 * This is neccessary for future use of glib and gettext based localization.
 */
const char * _(const char* message) {
	return message;
}

/**
 * Guess the transfer type, given the client requested type.
 * Actually in unix there is no difference between binary and 
 * ascii mode when we work with file descriptors.
 * If #type is not recognized as a valid client request, -1 is returned.
 */
int get_type(const char *type) {
	if(type==NULL)
		return -1;
	int len = strlen(type);
	if(len==0)
		return -1;
	switch(type[0]) {
		case 'I':
			return 1;
		case 'A':
			return 2;
		case 'L':
			if(len<3)
				return -1;
			if(type[2]=='7')
				return 3;
			if(type[2]=='8')
				return 4;
	}
	return -1;
}

/**
 * Create a new connection to a (address,port) tuple, retrieved from
 * the PORT command. This connection will be used for data transfers
 * in commands like "LIST","STOR","RETR"
 */
//int make_client_connection(int sock_fd,int client_port,const char* client_addr) {

int make_client_connection(int sock_fd,char *client_port) {

	if(client_port<1) {
		send_repl(sock_fd,REPL_425);
		return -1;
	}
	int sock=-1;
	char *dag;
	//int status = -1;	

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	//servaddr.sin_addr.s_addr = inet_addr(client_addr);
	//servaddr.sin_port = htons (client_port);
		//printf("before socket\n");
	if ((sock = Xsocket(XSOCK_STREAM)) < 0) {
	//if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		send_repl(sock,REPL_425);
		raiseerr(15);
		return -1;
	}
	
	int status = Xconnect(sock, client_port);
	//int status = connect (sock, (struct sockaddr *)&servaddr, sizeof (servaddr));
	
	if(status!=0) {
		send_repl(sock,REPL_425);
		return -1;
	}
		
	return sock;
}

/**
 * Close the connection to the client and exit the child proccess.
 * Although it is the same as close(sock_fd), in the future it can be used for 
 * logging some stats about active and closed sessions.
 */
void close_conn(int sock_fd) {
	if (close(sock_fd) < 0) { 
		raiseerr (5);
	}
	exit(0);
}

/**
 * Get the next command from the client socket.
 */
int get_command(int conn_fd,char *read_buff1,char *data_buff) {


	char read_buff[RCVBUFSIZE];
	memset((char *)&read_buff, 0, RCVBUFSIZE);
	read_buff[0]='\0';
	char *rcv=read_buff;
	int cmd_status = -1;
	
	int recvbuff = Xrecv(conn_fd,read_buff,RCVBUFSIZE,0);

	
	//int recvbuff = recv(conn_fd,read_buff,RCVBUFSIZE,0);
	if(recvbuff<1) {
		return CMD_CLOSE;
	}
	/*if(recvbuff==RCVBUFSIZE) {
		return CMD_UNKNOWN;
	}*/
	
	cmd_status = parse_input(rcv,data_buff);
	return cmd_status;
}

/**
 * A handler, which is called on child proccess exit.
 */
void sig_chld_handler(void) {
	open_connections--;
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * Send reply to the client socket, given the reply.
 */
int send_repl(int send_sock,char *msg) {
	if (Xsend(send_sock, msg, strlen(msg), 0) < 0) {
	//if (send(send_sock, msg, strlen(msg),0) < 0) { 
		raiseerr (4);
		Xclose(send_sock);
		exit(0);
	}
	return 0;
}

/**
 * Send single reply to the client socket, given the reply and its length.
 */
int send_repl_client_len(int send_sock,char *msg,int len) {
	if (Xsend(send_sock, msg, len,0) < 0) { 
	//if (send(send_sock, msg, len,0) < 0) { 
		raiseerr (4);
		Xclose(send_sock);
	}
	return 0;
}

/*
Izprashtane na edinishen otgovor do dopulnitelnia socket za transfer
*/
int send_repl_client(int send_sock,char *msg) {
	send_repl_client_len(send_sock,msg,strlen(msg));
	return 0;
}

/**
 * Send single reply to the additional transfer socket, given the reply and its length.
 */
int send_repl_len(int send_sock,char *msg,int len) {
	if (Xsend(send_sock, msg, len,0) < 0) { 
	//if (send(send_sock, msg, len,0) < 0) { 
		raiseerr (4);
		Xclose(send_sock);
		exit(0);
	}
	return 0;
}

/**
 * Parses the results from the PORT command, writes the
 * address in "client_addt" and returnes the port
 */
int parse_port_data(char *data_buff,char *client_addr) {
	client_addr[0]='\0';
	int len=0;
	int port=0;
	int _toint=0;
	char *result;
	result = strtok(data_buff, PORTDELIM);
	_toint=toint(result,FALSE);
	if(_toint<1 || _toint>254)
		return -1;
	len += strlen(result);
	strcpy(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	_toint=toint(result,FALSE);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	
	result = strtok(NULL, PORTDELIM);
	len = toint(result,FALSE);
	if(_toint<0 || _toint>255)
		return -1;
	port = 256*len;
	result = strtok(NULL, PORTDELIM);
	len = toint(result,FALSE);
	if(_toint<0 || _toint>255)
		return -1;
	port +=len;
	return port;
}
void print_help(int sock) {
	send_repl(sock,"    Some help message.\r\n    Probably nobody needs help from telnet.\r\n    See rfc959.\r\n");
}
/**
 * Main cycle for client<->server communication. 
 * This is done synchronously. On each client message, it is parsed and recognized,
 * certain action is performed. After that we wait for the next client message
 * 
 */
int interract(int conn_fd,cmd_opts *opts) {
	static int BANNER_LEN = strlen(REPL_220);
	int userid = opts->userid;
	int client_fd=-1;
	int len;
	int _type ;
	int type = 2; // ASCII TYPE by default
	if(userid>0) {
		int status = setreuid(userid,userid);
		if(status != 0) {
			switch(errno) {
				case EPERM:
					break;
				case EAGAIN:
					break;
				default:
					break;
			}
			close_conn(conn_fd);
		}
		
	}
	if(max_limit_notify) {
		send_repl(conn_fd,REPL_120);
		close_conn(conn_fd);
	}
	char current_dir[MAXPATHLEN];
	char parent_dir[MAXPATHLEN];
	char virtual_dir[MAXPATHLEN];
	char reply[SENDBUFSIZE];
	char data_buff[DATABUFSIZE];
	char read_buff[RCVBUFSIZE];
	char *str;
	bool is_loged = FALSE;
	bool state_user = FALSE;
	char rename_from[MAXPATHLEN];
	char newdag[DATABUFSIZE];
	
	int data_sock;
	
	memset((char *)&current_dir, 0, MAXPATHLEN);
	strcpy(current_dir,opts->chrootdir);
	strcpy(parent_dir,opts->chrootdir);
	free(opts);
	chdir(current_dir);
	if((getcwd(current_dir,MAXPATHLEN)==NULL)) {
		raiseerr(19);
		close_conn(conn_fd);
	}
	memset((char *)&data_buff, 0, DATABUFSIZE);
	memset((char *)&read_buff, 0, RCVBUFSIZE);
	
	reply[0]='\0';
	char client_addr[ADDRBUFSIZE];

	char *client_port;
	send_repl_len(conn_fd,REPL_220,BANNER_LEN);
	
	while(1) {
		data_buff[0]='\0';
	
		int result = get_command(conn_fd,read_buff,data_buff);
		if(result != CMD_RNFR && result != CMD_RNTO && result != CMD_NOOP)
			rename_from[0]='\0';
		switch(result) {
			case CMD_UNKNOWN:
			case -1:
				send_repl(conn_fd,REPL_500);
				break;
			case CMD_EMPTY:
			case CMD_CLOSE:
				close_conn(conn_fd);
				break;
			case CMD_USER:
				if(data_buff==NULL || strcmp(data_buff,ANON_USER)==0) {
					state_user = TRUE;
					send_repl(conn_fd,REPL_331_ANON);
				}
				else {
					send_repl(conn_fd,REPL_332);
				}
				break;
			case CMD_PASS:
				if(!state_user) {
					send_repl(conn_fd,REPL_503);
				}
				else {
					is_loged = TRUE;
					state_user = FALSE;
					send_repl(conn_fd,REPL_230);
				}
				break;
			case CMD_PORT:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {	
					strcpy(newdag, data_buff);  /* New dag created by client for the data connection */
					client_port = &newdag;
					
					if(client_port<0) {
						send_repl(conn_fd,REPL_501);
						client_port = 0;
					} else {
						send_repl(conn_fd,REPL_200);
					}
				}
				break;
			case CMD_PASV:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					send_repl(conn_fd,REPL_502);
				}
				break;
			case CMD_SYST:
				reply[0]='\0';
				len = sprintf(reply,REPL_215,"UNIX");
				reply[len] = '\0';
				send_repl(conn_fd,reply);
				break;
			case CMD_LIST:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					//send_repl(conn_fd, REPL_150);
					//client_fd = make_client_connection(conn_fd, client_port,client_addr);
					client_fd = make_client_connection(conn_fd, client_port);
					if(client_fd!=-1){
						write_list(conn_fd,client_fd,current_dir);
					}
					client_fd = -1;
				}
				break;
			case CMD_RETR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						//client_fd = make_client_connection(conn_fd, client_port,client_addr);
						client_fd = make_client_connection(conn_fd, client_port);
						if(client_fd!=-1){
							retrieve_file(conn_fd,client_fd, type,data_buff);
						}
						client_fd = -1;
					}
				}
				break;
			case CMD_STOU:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					int fd = mkstemp("XXXXX");
					//client_fd = make_client_connection(conn_fd, client_port,client_addr);
					client_fd = make_client_connection(conn_fd, client_port);
					if(client_fd!=-1){
						stou_file(conn_fd,client_fd, type,fd);
					}
					client_fd = -1;
				}
				break;
			case CMD_STOR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						//client_fd = make_client_connection(conn_fd, client_port,client_addr);
						
						client_fd = make_client_connection(conn_fd, client_port);
						if(client_fd!=-1){
							
							store_file(conn_fd,client_fd, type,data_buff);
							
						}
						client_fd = -1;
					}
				}
				break;
			case CMD_SITE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						send_repl(conn_fd,REPL_202);
					}
				}
				break;
			case CMD_PWD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					reply[0]='\0';
					len = sprintf(reply,REPL_257_PWD,current_dir);
					reply[len] = '\0';
					send_repl(conn_fd,reply);
				}
				break;
			case CMD_CDUP:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					change_dir(conn_fd,parent_dir,current_dir,virtual_dir,"..");
				}
				break;
			case CMD_CWD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						change_dir(conn_fd,parent_dir,current_dir,virtual_dir,data_buff);
					}
				}
				break;
			case CMD_QUIT:
				send_repl(conn_fd,REPL_221);
				if(client_fd!=-1){
					close_conn(client_fd);
				}
				close_conn(conn_fd);
				break;
			case CMD_TYPE:
				_type = get_type(data_buff);
				if(_type ==-1) {
					send_repl(conn_fd,REPL_500);
				}
				else {
					type=_type;
					send_repl(conn_fd,REPL_200);
				}
				break;
			case CMD_STAT:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					}
					else {
						stat_file(conn_fd,data_buff,reply);
					}
				}
				break;
			case CMD_ABOR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(client_fd!=-1){
						close_connection(client_fd);
					} 
					send_repl(conn_fd,REPL_226);
				}
				break;
			case CMD_MKD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						make_dir(conn_fd,data_buff,reply);
					}
				}
				break;
			case CMD_RMD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						remove_dir(conn_fd,data_buff);
					}
				}
				break;
			case CMD_DELE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						delete_file(conn_fd,data_buff);
					}
				}
				break;
			case CMD_RNFR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						strcpy(rename_from,data_buff);
						send_repl(conn_fd,REPL_350);
					}
				}
				break;
			case CMD_RNTO:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						if(rename_from==NULL || strlen(rename_from)==0 || rename_from[0]=='\0') {
							send_repl(conn_fd,REPL_501);
						} else {
							rename_fr(conn_fd,rename_from,data_buff);
						}
					}
					rename_from[0]='\0';
				}
				break;
			case CMD_NOOP:
				send_repl(conn_fd,REPL_200);
				break;
			case CMD_STRU:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						switch(data_buff[0]) {
							case 'F':
								send_repl(conn_fd,REPL_200);
								break;
							case 'P':
							case 'R':
								send_repl(conn_fd,REPL_504);
								break;
							default:
								send_repl(conn_fd,REPL_501);
							
						}
					}
				}
				break;
			case CMD_HELP:
			//	if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
					send_repl(conn_fd,REPL_214);
					print_help(conn_fd);
					send_repl(conn_fd,REPL_214_END);
			//	}
			// XXX separate HELP without arguments from HELP for a single command
				break;
			case CMD_MODE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						switch(data_buff[0]) {
							case 'S':
								send_repl(conn_fd,REPL_200);
								break;
							case 'B':
							case 'C':
								send_repl(conn_fd,REPL_504);
								break;
							default:
								send_repl(conn_fd,REPL_501);
							
						}
					}
				}
				break;
			default:
				send_repl(conn_fd,REPL_502);
		}
	}
	
	free(data_buff);
	free(read_buff);
	free(current_dir);
	free(parent_dir);
	free(virtual_dir);
	free(rename_from);
	close_conn(conn_fd);
}

/**
 * Close a socket and return a status of the close operation.
 * Although it is equivalent to close(connection) in the future it can be used
 * for writing logs about opened and closed sessions.
 */
int close_connection(int connection) {
	return Xclose(connection);
}


char *createDAG(const char *ad, const char *host, const char *service)
{
        int len = snprintf(NULL, 0, DAG, ad, host, service) + 1;
        char * dag = (char*)malloc(len);
        sprintf(dag, DAG, ad, host, service);
        return dag;
}


/**
 * Creates new server listening socket and make the main loop , which waits
 * for new connections.
 */
int create_socket(struct cmd_opts *opts) {
	if(opts==NULL)
		return 10;
	int status = chdir(opts->chrootdir);
	if(status!=0) {
		raiseerr(15);
	}
	int servaddr_len =  0;
	int connection = 0;
	int sock = 0;
	int pid  = 0;
	open_connections=0;
	
	char *dag;
	char myAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE];

	
	struct sockaddr_in servaddr;
	pid = getuid();	
	if(pid != 0 && opts->port <= 1024)
	{
		printf((" Access denied:\n     Only superuser can listen to ports (1-1024).\n You can use \"-p\" option to specify port, greater than 1024.\n"));
		exit(1);
	}
	memset((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = PF_INET;
	if(opts->listen_any==TRUE) {
		servaddr.sin_addr.s_addr =  htonl(INADDR_ANY);
	}
	else if(opts->listen_addr==NULL) {
		return 9;
	} else {
		struct hostent *host = gethostbyname(opts->listen_addr);
		if(host==NULL) {
			printf(_("Cannot create socket on server address: %s\n"),opts->listen_addr);
			return 11;
		}
		bcopy(host->h_addr, &servaddr.sin_addr, host->h_length);
	}
	servaddr.sin_port = htons (opts->port);
	servaddr_len = sizeof(servaddr);

        if ((sock = Xsocket(XSOCK_STREAM)) < 0) {
	//if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		raiseerr(ERR_CONNECT);
		return 1;
	}
	int flag = 1;
	//setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,(char *) &flag, sizeof(int));
	
	// remove the Nagle algorhytm, which improves the speed of sending data.
	//setsockopt(sock, IPPROTO_TCP,TCP_NODELAY,(char *) &flag, sizeof(int));

	if ((XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID))) < 0) {
	printf("XreadLocalHostAddr error\n");	
	}

	if (!(dag = createDAG(myAD, myHID, SID_STREAM)))
		printf("unable to create DAG: %s\n", dag);
		

	if (XregisterName(STREAM_NAME, dag) < 0 )
    		printf("error registering name: %s\n", STREAM_NAME);

	if ((Xbind (sock, dag)) < 0) {
	//if(bind (sock, (struct sockaddr *)&servaddr, sizeof(servaddr))<0) {
		if(opts->listen_any==FALSE) {
			printf(("Cannot bind address: %s\n"),opts->listen_addr);
		}else {
			printf(("Cannot bind on default address\n"));
		}
		return raiseerr(8);
	}

	/*if(listen(sock,opts->max_conn) <0) {
		return raiseerr(2);
	} */
	#ifdef __USE_GNU
		signal(SIGCHLD, (sighandler_t )sig_chld_handler);
	#endif
	#ifdef __USE_BSD
		signal(SIGCHLD, (sig_t )sig_chld_handler);
	#endif

	printf(REPL_220);	
	
	for (;;) {
		max_limit_notify = FALSE;
		
		
	//	if ((connection = accept(sock, (struct sockaddr *) &servaddr, &servaddr_len)) < 0) {
		if ((connection = Xaccept(sock)) < 0) {
			printf("error connecting\n");
			raiseerr(3);
			return -1;
		}
		pid = fork();
		if(pid==0) {
			if(open_connections >= opts->max_conn)
				max_limit_notify=TRUE;
			interract(connection,opts);
		} else if(pid>0) {
			open_connections++;
			assert(close_connection(connection)>=0);
		}
		else {
			 
			Xclose(connection);
			Xclose(sock);
			assert(0);
		}
	}

free(dag);
Xclose(sock);
}
