#include "Neighbor.hh"

Neighbor::Neighbor(Graph addr, int port, int cost)
{
    _addr = addr;
    _port = port;
    _cost = cost;
}

const Graph Neighbor::addr() const
{
    return _addr;
}

int Neighbor::port() const
{
    return _port;
}

int Neighbor::cost() const
{
    return _cost;
}

const std::string Neighbor::final_intent_str() const
{
    Node n = _addr.get_node(_addr.num_nodes() - 1);

    return n.type_string() + ":" + n.id_string();
}
