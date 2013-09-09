#ifndef CONTROLMESSAGE_HH
#define CONTROLMESSAGE_HH

#include <cstdlib>
#include <sstream>
#include <string>

using namespace std;

#define CTL_UNDEFINED     -1
#define CTL_HELLO          0
#define CTL_LSA            1
#define CTL_HOST_REGISTER  2
#define CTL_ROUTING_TABLE  3

class ControlMessage
{
    private:
        std::string _msg;
        size_t _pos;

    public:
        ControlMessage(int type);
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
};

#endif /* CONTROLMESSAGE_HH */
