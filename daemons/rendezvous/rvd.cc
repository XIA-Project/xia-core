#include <syslog.h>
#include <errno.h>
#include <map>
#include <string>
using namespace std;

#include <stdint.h>
#include "clicknetxia.h"
#include "Xsocket.h"
#include "Xsecurity.h"
#include "dagaddr.hpp"
#include "Xkeys.h"
#include "xia.h"

#define DEFAULT_NAME "host0"
#define APPNAME "xrendezvous"

#define RV_MAX_DATA_PACKET_SIZE 16384
#define RV_MAX_CONTROL_PACKET_SIZE 1024

map<std::string, std::string> name_to_dag_db_table; // map name to dag

char *hostname = NULL;
char *ident = NULL;
char data_plane_DAG[XIA_MAX_DAG_STR_SIZE];
char control_plane_DAG[XIA_MAX_DAG_STR_SIZE];
map<string, string> HIDtoAD;
map<string, double> HIDtoTimestamp;
map<string, double>::iterator HIDtoTimestampIterator;

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-d SID -c SID][-h hostname]\n", name);
	printf("where:\n");
	printf(" -c SID      : SID for rendezvous control plane.\n");
	printf(" -d SID      : SID for rendezvous data plane.\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=host0)\n");
	printf("\n");
	exit(0);
}

int buildDAGStringFromSID(char *sid, char *dag, int daglen)
{
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, sid, NULL, &ai)) {
		syslog(LOG_ALERT, "Unable to get local address");
		return -1;
	}
	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	if((int)g.dag_string().size() > daglen-1) {
		syslog(LOG_ERR, "DAG longer than provided buffer");
		return -1;
	}
	strcpy(dag, g.dag_string().c_str());
	return 0;
}

int buildDAGFromRandomSID(char *dag, int daglen)
{
	char sid[XIA_XID_STR_SIZE];
	if(XmakeNewSID(sid, XIA_XID_STR_SIZE)) {
		syslog(LOG_ERR, "ERROR generating random SID");
		return -1;
	}
	return buildDAGStringFromSID(sid, dag, daglen);
}


void config(int argc, char** argv)
{
	int c;
	unsigned level = 3;
	int verbose = 0;
	char *datasid = NULL;
	char *controlsid = NULL;
	bool data_plane_dag_known = false;
	bool control_plane_dag_known = false;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:d:c:v")) != -1) {
		switch (c) {
			case 'h':
				hostname = strdup(optarg);
				break;
			case 'l':
				level = MIN(atoi(optarg), LOG_DEBUG);
				break;
			case 'd':
				datasid = strdup(optarg);
				break;
			case 'c':
				controlsid = strdup(optarg);
				break;
			case 'v':
				verbose = LOG_PERROR;
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname)
		hostname = strdup(DEFAULT_NAME);

	// Build data plane DAG if user provided SID for it
	if(datasid) {
		if(buildDAGStringFromSID(datasid, data_plane_DAG, XIA_MAX_DAG_STR_SIZE)) {
			syslog(LOG_ERR, "Failed creating data addr for %s", datasid);
			exit(1);
		}
		data_plane_dag_known = true;
	}

	// Build control plane DAG if user provided SID for it
	if(controlsid) {
		if(buildDAGStringFromSID(controlsid, control_plane_DAG, XIA_MAX_DAG_STR_SIZE)) {
			syslog(LOG_ERR, "Failed creating control addr for %s", controlsid);
			exit(1);
		}
		control_plane_dag_known = true;
	}

	if(!data_plane_dag_known) {
		// Read Data plane DAG from resolv.conf
		if(XreadRVServerAddr(data_plane_DAG, XIA_MAX_DAG_STR_SIZE) == 0) {
			syslog(LOG_INFO, "Data plane DAG: %s", data_plane_DAG);
		} else {
			// Build a DAG from scratch
			if(buildDAGFromRandomSID(data_plane_DAG, XIA_MAX_DAG_STR_SIZE)) {
				syslog(LOG_ERR, "Unable to build a dag from random SID");
				exit(1);
			}
		}
		data_plane_dag_known = true;
	}

	if(!control_plane_dag_known) {
		// Read Data plane DAG from resolv.conf
		if(XreadRVServerControlAddr(control_plane_DAG, XIA_MAX_DAG_STR_SIZE) == 0) {
			syslog(LOG_INFO, "Control plane DAG: %s", control_plane_DAG);
		} else {
			// Build a DAG from scratch
			if(buildDAGFromRandomSID(control_plane_DAG, XIA_MAX_DAG_STR_SIZE)) {
				syslog(LOG_ERR, "Unable to build a dag from random SID");
				exit(1);
			}
		}
		control_plane_dag_known = true;
	}

	// Terminate, if we don't have DAGs for control and data plane
	if(!data_plane_dag_known || !control_plane_dag_known) {
		syslog(LOG_ERR, "ERROR: Data and Control addresses not found.");
		exit(1);
	}

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s", APPNAME);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

int getServerSocket(char *dag, int type)
{
	int sockfd = Xsocket(AF_XIA, type, 0);
	if (sockfd < 0) {
		syslog(LOG_ALERT, "Unable to create a socket for %s", dag);
   		return -1;
	}

	sockaddr_x sa;
	Graph g(dag);
	g.fill_sockaddr(&sa);
	syslog(LOG_INFO, "binding to local DAG: %s", g.dag_string().c_str());

	// Data plane socket binding to the SID
	if (Xbind(sockfd, (struct sockaddr*)&sa, sizeof(sockaddr_x)) < 0) {
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
   		return -3;
	}
	return sockfd;
}

void print_packet_contents(char *packet, int len)
{
	int hex_string_len = (len*2) + 1;
	char hex_string[hex_string_len];
    int i;
    for(i=0;i<len;i++) {
        sprintf(&hex_string[2*i], "%02x", (unsigned int)packet[i]);
    }
    hex_string[hex_string_len-1] = '\0';
	syslog(LOG_INFO, "Packet contents|%s|", hex_string);
}

void print_packet_header(click_xia *xiah)
{
	syslog(LOG_INFO, "======= RAW PACKET HEADER ========");
	syslog(LOG_INFO, "ver:%d", xiah->ver);
	syslog(LOG_INFO, "nxt:%d", xiah->nxt);
	syslog(LOG_INFO, "plen:%d", htons(xiah->plen));
	syslog(LOG_INFO, "hlim:%d", xiah->hlim);
	syslog(LOG_INFO, "dnode:%d", xiah->dnode);
	syslog(LOG_INFO, "snode:%d", xiah->snode);
	syslog(LOG_INFO, "last:%d", xiah->last);
	int total_nodes = xiah->dnode + xiah->snode;
	for(int i=0;i<total_nodes;i++) {
		uint8_t id[20];
		char hex_string[41];
		bzero(hex_string, 41);
		memcpy(id, xiah->node[i].xid.id, 20);
		for(int j=0;j<20;j++) {
			sprintf(&hex_string[2*j], "%02x", (unsigned int)id[j]);
		}
		char type[10];
		bzero(type, 10);
		switch (htonl(xiah->node[i].xid.type)) {
			case CLICK_XIA_XID_TYPE_AD:
				strcpy(type, "AD");
				break;
			case CLICK_XIA_XID_TYPE_HID:
				strcpy(type, "HID");
				break;
			case CLICK_XIA_XID_TYPE_SID:
				strcpy(type, "SID");
				break;
			case CLICK_XIA_XID_TYPE_CID:
				strcpy(type, "CID");
				break;
			case CLICK_XIA_XID_TYPE_IP:
				strcpy(type, "4ID");
				break;
			default:
				sprintf(type, "%d", xiah->node[i].xid.type);
		};
		syslog(LOG_INFO, "%s:%s", type, hex_string);
	}
}

void process_data(int datasock)
{
	int packetlen;
	char packet[RV_MAX_DATA_PACKET_SIZE];
	sockaddr_x rdag;
	socklen_t rdaglen = sizeof(rdag);
	bzero(packet, RV_MAX_DATA_PACKET_SIZE);

	// Read in the data packet
	syslog(LOG_INFO, "Reading data packet");
	int retval = Xrecvfrom(datasock, packet, RV_MAX_DATA_PACKET_SIZE,
			0, (struct sockaddr *)&rdag, &rdaglen);
	if(retval < 0) {
		syslog(LOG_WARNING, "WARN: No data(%s)", strerror(errno));
		return;
	}
	packetlen = retval;
	Graph g(&rdag);
	syslog(LOG_INFO, "Packet of size:%d received from %s:",
			retval, g.dag_string().c_str());
	//print_packet_contents(packet, retval);
	click_xia *xiah = reinterpret_cast<struct click_xia *>(packet);
	print_packet_header(xiah);

	// Read in the destination DAG
	// NOTE: Conversion to Graph strips out 'visited' field in the dag
	Graph ddag;
	ddag.from_wire_format(xiah->dnode, xiah->node);

	// Find the intent AD and HID
	std::string intent_ad = ddag.intent_AD_str();
	std::string intent_hid = ddag.intent_HID_str();
	if (intent_ad.size() < CLICK_XIA_XID_ID_LEN ||
			intent_hid.size() < CLICK_XIA_XID_ID_LEN) {
		syslog(LOG_ERR, "intent_ad: %s.", intent_ad.c_str());
		syslog(LOG_ERR, "intent_hid: %s.", intent_hid.c_str());
		syslog(LOG_ERR, "intent AD or HID not found in dest dag");
		return;
	}

	// Find the current AD for intent hid
	// TODO: Add check ot ensure intent_hid is in HIDtoAD
	std::string new_ad(HIDtoAD[intent_hid]);

	// Now replace intent AD with the new AD, if changed
	if (new_ad.compare(intent_ad)) {
		ddag.replace_intent_AD(new_ad);

		// Convert DAG back to wire format and overwrite ddag in packet
		size_t num_nodes = ddag.fill_wire_buffer(xiah->node);

		// Confirm that the new DAG size matches the DAG being replaced
		if (num_nodes != xiah->dnode) {
			// drop this packet and abort
			syslog(LOG_ERR, "DAG modification resulted in size change");
			return;
		}
	}

	xiah->last = LAST_NODE_DEFAULT;
	syslog(LOG_INFO, "Updated AD and last pointer in header");
	print_packet_header(xiah);
	syslog(LOG_INFO, "Sending the packet out on the network");
	Xsend(datasock, packet, packetlen, 0);
}

#define MAX_XID_STR_SIZE 64
#define MAX_HID_DAG_STR_SIZE 256

void process_control_message(int controlsock)
{
	char packet[RV_MAX_CONTROL_PACKET_SIZE];
	sockaddr_x ddag;
	socklen_t ddaglen = sizeof(ddag);
	bzero(packet, RV_MAX_CONTROL_PACKET_SIZE);

	syslog(LOG_INFO, "Reading control packet");
	int retval = Xrecvfrom(controlsock, packet, RV_MAX_CONTROL_PACKET_SIZE, 0, (struct sockaddr *)&ddag, &ddaglen);
	if(retval < 0) {
		syslog(LOG_WARNING, "WARN: No control message(%s)", strerror(errno));
		return;
	}
	syslog(LOG_INFO, "Control packet of size:%d received", retval);

	// Extract control message, signature and pubkey of sender
	XIASecurityBuffer signedMsg(packet, retval);
	uint16_t controlMsgLength = signedMsg.peekUnpackLength();
	char controlMsgBuffer[controlMsgLength];
	signedMsg.unpack(controlMsgBuffer, &controlMsgLength);

	// The control message
	XIASecurityBuffer controlMsg(controlMsgBuffer, controlMsgLength);

	// The signature
	uint16_t signatureLength = signedMsg.peekUnpackLength();
	char signature[signatureLength];
	signedMsg.unpack(signature, &signatureLength);

	// The sender's public key
	uint16_t pubkeyLength = signedMsg.peekUnpackLength();
	char pubkey[pubkeyLength+1];
	bzero(pubkey, pubkeyLength+1);
	signedMsg.unpack(pubkey, &pubkeyLength);

	// Extract HID, DAG & timestamp from the control message
	uint16_t hidLength = controlMsg.peekUnpackLength();
	char hid[hidLength+1];
	bzero(hid, hidLength+1);
	controlMsg.unpack(hid, &hidLength);

	uint16_t dagLength = controlMsg.peekUnpackLength();
	char dag[dagLength+1];
	bzero(dag, dagLength+1);
	controlMsg.unpack(dag, &dagLength);

	uint16_t timestampstrLength = controlMsg.peekUnpackLength();
	char timestampstr[timestampstrLength+1];
	bzero(timestampstr, timestampstrLength+1);
	controlMsg.unpack(timestampstr, &timestampstrLength);
	double timestamp = strtod(timestampstr, NULL);

	// Verify hash(pubkey) matches HID
	if(!xs_pubkeyMatchesXID(pubkey, hid)) {
		syslog(LOG_ERR, "ERROR: Public key does not match HID of control message sender");
		return;
	}
	syslog(LOG_INFO, "Valid pubkey in control message");

	// Verify signature using pubkey
	if(!xs_isValidSignature((const unsigned char *)controlMsgBuffer, controlMsgLength, (unsigned char *)signature, signatureLength, pubkey, pubkeyLength)) {
		syslog(LOG_ERR, "ERROR: Invalid signature");
		return;
	}
	syslog(LOG_INFO, "Signature valid");

	// Verify that the timestamp is newer than seen before
	HIDtoTimestampIterator = HIDtoTimestamp.find(hid);
	if(HIDtoTimestampIterator == HIDtoTimestamp.end()) {
		// New host, create an entry and record initial timestamp
		HIDtoTimestamp[hid] = timestamp;
		syslog(LOG_INFO, "New timestamp %f for HID|%s|", timestamp, hid);
	} else {
		// Verify the last timestamp is older than the one is this message
		if(HIDtoTimestamp[hid] < timestamp) {
			HIDtoTimestamp[hid] = timestamp;
			syslog(LOG_INFO, "Updated timestamp %f for HID|%s|", timestamp, hid);
		} else {
			syslog(LOG_ERR, "ERROR: Timestamp previous:%f now:%f", HIDtoTimestamp[hid], timestamp);
			return;
		}
	}

	// Extract AD from DAG
	Graph hostaddr(dag);
	HIDtoAD[hid] = hostaddr.intent_AD_str();
	syslog(LOG_INFO, "Added %s:%s to table", hid, hostaddr.intent_AD_str().c_str());

	// Registration message
	// Extract HID, newAD, timestamp, Signature, Pubkey
	// Verify HID not already in table
	// Verify HID matches Pubkey
	// Verify Signature(HID, newAD, timestamp) using Pubkey
	// Set lastTimestamp for this HID
	//
	//
	// UpdateAD message
	// Extract HID, newAD, timestamp, Signature, Pubkey
	// Verify HID is in table
	// Verify HID matches Pubkey
	// Verify Signature(HID, newAD, timestamp) using Pubkey
	// Verify timestamp > lastTimestamp
	//
	// Heartbeat message?
}

int main(int argc, char *argv[]) {

	// Parse command-line arguments
	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// Data plane socket used to rendezvous clients with services
	int datasock = getServerSocket(data_plane_DAG, SOCK_RAW);
	int controlsock = getServerSocket(control_plane_DAG, SOCK_DGRAM);
	if(datasock < 0 || controlsock < 0) {
		syslog(LOG_ERR, "ERROR creating a server socket");
		return 1;
	}

	// Main loop checks data and control sockets for activity
	while(1) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(datasock, &read_fds);
		FD_SET(controlsock, &read_fds);
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;
		int retval = Xselect(controlsock+1, &read_fds, NULL, NULL, &timeout);
		if(retval == -1) {
			syslog(LOG_ERR, "ERROR waiting for data to arrive. Exiting");
			return 2;
		}
		if(retval == 0) {
			// No data on control/data sockets, loop again
			// This is the place to add any actions between loop iterations
			continue;
		}
		// Check for control messages first
		if(FD_ISSET(controlsock, &read_fds)) {
			process_control_message(controlsock);
		}
		// Handle data packets
		if(FD_ISSET(datasock, &read_fds)) {
			process_data(datasock);
		}
	}
	return 0;

}
