#ifndef CONTROLMESSAGE_HH
#define CONTROLMESSAGE_HH

#include <cstdlib>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include "Xsocket.h"

using namespace std;

#define CTL_HELLO          0
#define CTL_LSA            1
#define CTL_HOST_REGISTER  2
#define CTL_ROUTING_TABLE  3 // TODO: rename to RTU
#define CTL_XBGP           4
#define CTL_SID_DISCOVERY  5
#define CTL_SID_ROUTING_TABLE  6
#define CTL_SID_MANAGE_KA  7 // keep alive msg to service controller
#define CTL_SID_RE_DISCOVERY  8 // send to service controller
#define CTL_AD_PATH_STATE_PING 9
#define CTL_AD_PATH_STATE_PONG 10
#define CTL_SID_DECISION_QUERY 11
#define CTL_SID_DECISION_ANSWER 12

class ControlMessage
{
    private:
        std::string _msg;
        size_t _pos;

    public:
        ControlMessage(int type, std::string ad, std::string hid);
        ControlMessage(std::string s);

        void clear();

        size_t length() const;
        size_t size() const;

        const std::string str() const;
        const char *c_str() const;

        void append(std::string s);
        void append(int n);

        void seek(int pos);
        int read(std::string &s);
        int read(int &n);

        int send(int socket, const sockaddr_x *dest) const;
};

#endif /* CONTROLMESSAGE_HH */
