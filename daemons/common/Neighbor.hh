#ifndef NEIGHBOR_HH
#define NEIGHBOR_HH

#include <string>
#include "dagaddr.hpp"

using namespace std;

class Neighbor
{
    private:
        Graph _addr;
        int _port;
        int _cost;

    public:
        Neighbor(Graph addr, int port, int cost);

        const Graph addr() const;
        int port() const;
        int cost() const;

        const std::string final_intent_str() const;
};

#endif /* NEIGHBOR_HH */
