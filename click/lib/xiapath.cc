// -*- related-file-name: "../include/click/xiapath.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiapath.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

#define NDEBUG_XIA

XIAPath::XIAPath()
{
    reset();
}

XIAPath::XIAPath(const XIAPath& r)
{
    _nodes = r._nodes;
    _src = r._src;
    _dst = r._dst;
}

XIAPath&
XIAPath::operator=(const XIAPath& r)
{
    _nodes = r._nodes;
    _src = r._src;
    _dst = r._dst;
    return *this;
}

void
XIAPath::reset()
{
    _nodes.clear();
    _src = _npos;
    _dst = _npos;
}

bool
XIAPath::parse(const String& s, const Element* context)
{
    String str_copy = s;
    String type = cp_shift_spacevec(str_copy);
    if (type == "DAG")
        return parse_dag(str_copy, context);
    else if (type == "RE")
        return parse_re(str_copy, context);
    else
        return false;
}

bool
XIAPath::parse_dag(const String& s, const Element* context)
{
    reset();

    String str_copy = s;

    // parse pointers from the source node
    Node src_node;
    src_node.xid = XID();
    for (size_t i = 0; i < CLICK_XIA_XID_EDGE_NUM; i++) {
        String index_str = cp_shift_spacevec(str_copy);
        if (index_str == "-")
            break;
        else {
            int index;
            if (!cp_integer(index_str, &index)) {
                click_chatter("cannot parse index: %s", index_str.c_str());
                return false;
            }
            src_node.edges.push_back(index);
        }
    }

    // parse nodes with pointers (the destination node does not have pointers, though)
    bool end = false;
    while (!end) {
        struct click_xia_xid xid;
        Node node;

        String xid_str = cp_shift_spacevec(str_copy);
        if (xid_str.length() == 0)
            break;

        // parse XID
#ifndef CLICK_TOOL
        if (!cp_xid(xid_str, &xid, context)) {
#else
        (void)context;
        if (!cp_xid(xid_str, &xid)) {
#endif
            click_chatter("unrecognized XID format: %s", xid_str.c_str());
            return false;
        }
        node.xid = XID(xid);

        // parse indices
        for (size_t i = 0; i < CLICK_XIA_XID_EDGE_NUM; i++) {
            String index_str = cp_shift_spacevec(str_copy);
            if (index_str == "") {
                // no pointers means we hit the destination node
                end = true;
                break;
            }
            if (index_str == "-")
                break;
            else {
                int index;
                if (!cp_integer(index_str, &index)) {
                    click_chatter("cannot parse index: %s", index_str.c_str());
                    return false;
                }
                node.edges.push_back(index);
            }
        }

        _nodes.push_back(node);
    }

    _nodes.push_back(src_node);

    _dst = _nodes.size() - 2;
    _src = _nodes.size() - 1;

    //dump_state();

    return true;
}

bool
XIAPath::parse_re(const String& s, const Element* context)
{
    reset();

    String str_copy = s;

    click_xia_xid prev_xid;
    memset(&prev_xid, 0, sizeof(prev_xid));

    Node src_node;

    // parse iterations: ( ("(" fallback path ")")? main node )+
    int next_idx = -1;
    while (true) {
        String head = cp_shift_spacevec(str_copy);
        if (head.length() == 0)
            break;

        Vector<struct click_xia_xid> fallback;
        if (head == "(") {
            // parse fallback path for the next main node
            while (true)
            {
                String tail = cp_shift_spacevec(str_copy);
                if (tail == ")")
                    break;

                click_xia_xid xid;
#ifndef CLICK_TOOL
                if (!cp_xid(tail, &xid, context)) {
#else
                (void)context;
                if (!cp_xid(tail, &xid)) {
#endif
                    click_chatter("unrecognized XID format: %s", tail.c_str());
                    return false;
                }
                fallback.push_back(xid);
            }
            head = cp_shift_spacevec(str_copy);
        }

        // parse the next main node
        click_xia_xid next_xid;
#ifndef CLICK_TOOL
        if (!cp_xid(head, &next_xid, context)) {
#else
        (void)context;
        if (!cp_xid(head, &next_xid)) {
#endif
            click_chatter("unrecognized XID format: %s", head.c_str());
            return false;
        }

        // add the prev main node
        // 1 + 2*|fallback path| nodes before next main node
        int next_main_idx = next_idx + 1 + fallback.size();

        Node node;
        node.xid = prev_xid;
        // link to the next main node
        node.edges.push_back(next_main_idx);
        // link to the next fallback node
        if (fallback.size() > 0)
            node.edges.push_back(next_idx + 1);
        if (next_idx == -1)
            src_node = node;
        else
            _nodes.push_back(node);
        next_idx++;

        for (int i = 0; i < fallback.size(); i++) {
            Node node;
            node.xid = fallback[i];
            // link to the next main node (implicit link)
            node.edges.push_back(next_main_idx);
            // link to the next fallback node
            if (i < fallback.size() - 1)
                node.edges.push_back(next_idx + 1);
            _nodes.push_back(node);
            next_idx++;
        }

        prev_xid = next_xid;
    }
    if (next_idx == -1) {
        click_chatter("empty path");
        return false;
    }

    // add destination node
    Node node;
    node.xid = prev_xid;
    _nodes.push_back(node);

    // add source node
    _nodes.push_back(src_node);

    _dst = _nodes.size() - 2;
    _src = _nodes.size() - 1;

    //dump_state();

    return true;
}

template <typename InputIterator>
void
XIAPath::parse_node(InputIterator node_begin, InputIterator node_end)
{
    reset();

    if (node_begin == node_end)
        return;

    InputIterator current_node = node_begin;

    while (current_node != node_end) {
        Node graph_node;

        graph_node.xid = (*current_node).xid;

        for (size_t j = 0; j < CLICK_XIA_XID_EDGE_NUM; j++) {
            size_t idx = (*current_node).edge[j].idx;
            if (idx == CLICK_XIA_XID_EDGE_UNUSED)
                continue;
            graph_node.edges.push_back(idx);
        }

        _nodes.push_back(graph_node);
        ++current_node;
    }

    // duplicate the last node
    _nodes.push_back(_nodes[_nodes.size() - 1]);

    _dst = _nodes.size() - 2;
    _src = _nodes.size() - 1;

    // remove edges from the destination node
    _nodes[_dst].edges.clear();

    // remove XID from the source node
    _nodes[_src].xid = XID();

    //dump_state();
}

template void XIAPath::parse_node(const struct click_xia_xid_node*, const struct click_xia_xid_node*);
template void XIAPath::parse_node(struct click_xia_xid_node*, struct click_xia_xid_node*);

template <typename InputIterator>
void
XIAPath::parse_node(InputIterator node_begin, size_t n)
{
    parse_node(node_begin, node_begin + n);
}

template void XIAPath::parse_node(const struct click_xia_xid_node*, size_t);
template void XIAPath::parse_node(struct click_xia_xid_node*, size_t);

String
XIAPath::unparse(const Element* context)
{
    String s = unparse_re(context);
    if (s.length() != 0)
        s = String("RE ") + s;
    else
        s = String("DAG ") + unparse_dag(context);
    return s;
}

String
XIAPath::unparse_dag(const Element* context)
{
    if (!is_valid())
        return "(invalid)";

    // unparsing to DAG string representation requires unparsing to a node list.
    // the graph in XIAPath itself is incompatible to our own DAG because
    // the graph does not enforce a fixed location for the source and destination nodes.
    size_t n = unparse_node_size();
    struct click_xia_xid_node node[n];
    if (unparse_node(node, n) != n)
        assert(false);

    StringAccum sa;

    for (int i = -1; i < static_cast<int>(n); i++) {
        size_t wrapped_i;
        if (i < 0)
            wrapped_i = i + n;
        else
            wrapped_i = i;

        const struct click_xia_xid_node& current_node = node[wrapped_i];

        if (i >= 0) {   // not a source node
            sa << XID(current_node.xid).unparse_pretty(context);
            sa << ' ';
        }

        if (i < static_cast<int>(n) - 1) {  // not a destination node
            for (size_t j = 0; j < CLICK_XIA_XID_EDGE_NUM; j++) {
                size_t idx = current_node.edge[j].idx;
                if (idx != CLICK_XIA_XID_EDGE_UNUSED)
                    sa << (int)idx << ' ';
                else
                {
                    sa << "- ";
                    break;
                }
            }
        }
    }
    return String(sa.data(), sa.length() - 1 /* exclusing the last space character */);
}

String
XIAPath::unparse_re(const Element* context)
{
    if (!is_valid())
        return "(invalid)";

    // try to unparse to RE string representation directly from the graph

    StringAccum sa;
    size_t prev_node = _src;
    while (true) {
        if (_nodes[prev_node].edges.size() == 0)    // destination
            break;
        else if (_nodes[prev_node].edges.size() == 1 || _nodes[prev_node].edges.size() == 2) {
            size_t next_primary_node = _nodes[prev_node].edges[0];

            if (_nodes[prev_node].edges.size() == 2) {
                // output fallback nodes
                sa << "( ";
                size_t next_fallback_node = _nodes[prev_node].edges[1];
                while (true) {
                    sa << _nodes[next_fallback_node].xid.unparse_pretty(context);
                    sa << ' ';

                    if (_nodes[next_fallback_node].edges.size() == 1) {
                        if (_nodes[next_fallback_node].edges[0] == next_primary_node)
                            break;
                        else
                            return "";     // unable to represent in RE
                    }
                    else if (_nodes[next_fallback_node].edges.size() == 2) {
                        if (_nodes[next_fallback_node].edges[0] == next_primary_node)
                            next_fallback_node = _nodes[next_fallback_node].edges[1];
                        else
                            return "";     // unable to represent in RE
                    }
                    else
                        return "";     // unable to represent in RE
                }
                sa << ") ";
            }

            // output primary node
            sa << _nodes[next_primary_node].xid.unparse_pretty(context);
            sa << ' ';

            prev_node = next_primary_node;
        }
        else
            return "";     // unable to represent in RE
    }
    return String(sa.data(), sa.length() - 1 /* exclusing the last space character */);
}

size_t
XIAPath::unparse_node_size() const
{
    if (_nodes.size() == 0)
        return 0;
    else
        return _nodes.size() - 1;
}

size_t
XIAPath::unparse_node(struct click_xia_xid_node* node, size_t n) const
{
    if (!const_cast<XIAPath*>(this)->topological_ordering())
        return 0;

    size_t total_n = unparse_node_size();
    if (n > total_n)
        n = total_n;

    if (total_n == 0)
        return 0;

    for (int i = 0; i < _nodes.size(); i++) {
        size_t order = _nodes[i].order;
        size_t index;
        if (order == 0) {
            // poiters from source node are stored at the destination node
            index = total_n - 1;
        }
        else
            index = order - 1;

        if (index < n) {  // if the output buffer space is available
            if (order > 0) // not a source node
                node[index].xid = _nodes[i].xid.xid();

            if (order < total_n) { // not a destination node
                for (int j = 0; j < CLICK_XIA_XID_EDGE_NUM; j++) {
                    if (j < _nodes[i].edges.size()) {
                        size_t next_node_order = _nodes[_nodes[i].edges[j]].order - 1;
                        assert(next_node_order < total_n);
                        node[index].edge[j].idx = next_node_order;
                    }
                    else
                        node[index].edge[j].idx = CLICK_XIA_XID_EDGE_UNUSED;
                    node[index].edge[j].visited = 0;
                }
            }
            else
                assert(order < total_n + 1);
        }
    }

    return n;
}

bool
XIAPath::topological_ordering()
{
    int indegree[_nodes.size()];
    for (int i = 0; i < _nodes.size(); i++)
        indegree[i] = 0;

    for (int i = 0; i < _nodes.size(); i++)
        for (int j = 0; j < _nodes[i].edges.size(); j++)
            indegree[_nodes[i].edges[j]]++;

    Vector<int> q;
    for (int i = 0; i < _nodes.size(); i++)
        if (indegree[i] == 0)
            q.push_back(i);

    size_t next_order = 0;
    int num_processed = 0;
    while (!q.empty()) {
        int next_node = q.back();
        q.pop_back();
        _nodes[next_node].order = next_order++;

        for (int i = 0; i < _nodes[next_node].edges.size(); i++) {
            // update inorder of next nodes
            if (--indegree[_nodes[next_node].edges[i]] == 0)
                q.push_back(_nodes[next_node].edges[i]);
        }

        num_processed++;
    }

    if (num_processed != _nodes.size()) {
        // the graph could not be ordered (i.e. not a DAG)
        return false;
    }

    //dump_state();

    return true;
}


bool
XIAPath::is_valid() const
{
    // valid source/destination node?
    if (_src >= static_cast<size_t>(_nodes.size()))
        return false;

    if (_dst >= static_cast<size_t>(_nodes.size()))
        return false;

    // non-degenerative path?
    if (_src == _dst)
        return false;

    int indegree[_nodes.size()];
    for (int i = 0; i < _nodes.size(); i++)
        indegree[i] = 0;

    for (int i = 0; i < _nodes.size(); i++) {
        // incorrect destination node?
        if (_nodes[i].edges.size() == 0 && i != static_cast<int>(_dst))
            return false;
        // too high outdegree?
        if (_nodes[i].edges.size() > CLICK_XIA_XID_EDGE_NUM)
            return false;

        for (int j = 0; j < _nodes[i].edges.size(); j++)
            indegree[_nodes[i].edges[j]]++;
    }

    for (int i = 0; i < _nodes.size(); i++) {
        // incorrect source node?
        if (indegree[i] == 0 && i != static_cast<int>(_src))
            return false;
    }

    // valid DAG?
    if (!const_cast<XIAPath*>(this)->topological_ordering())
        return false;

    return true;
}

XIAPath::handle_t
XIAPath::source_node() const
{
    return _src;
}

XIAPath::handle_t
XIAPath::destination_node() const
{
    return _dst;
}

XIAPath::handle_t
XIAPath::hid_node_for_destination_node() const
{
    // Get the source and destination node handles
    handle_t src = source_node();
    handle_t dest = destination_node();
	/*
    // Verify that the destination is an SID or CID
    XID destXID(xid(dest).unparse());
    if(destXID.type() != htonl(CLICK_XIA_XID_TYPE_SID)) {
		if(destXID.type() != htonl(CLICK_XIA_XID_TYPE_CID)) {
            click_chatter("XIAPath:hid_node...() destination node not SID:%s: type:%d:", destXID.unparse().c_str(), destXID.type());
            return INVALID_NODE_HANDLE;
		}
    }
	*/
    // Walk from source to destination
    handle_t current_node = src;
    do {
        // Find the next nodes for current_node
        Vector<XIAPath::handle_t> child_nodes = next_nodes(current_node);
        Vector<XIAPath::handle_t>::iterator it;
        //click_chatter("XIAPath::hid_node...() next nodes:");
        //for(it = child_nodes.begin(); it!=child_nodes.end(); it++) {
            //click_chatter("%s", xid(*it).unparse().c_str());
        //}
        it = child_nodes.begin();
        // See if the first node points to the destination
        if(*it == dest) {
            XID currentNodeXID(xid(current_node).unparse());
            if(currentNodeXID.type() != htonl(CLICK_XIA_XID_TYPE_HID)) {
                click_chatter("XIAPath::hid_node...() skipping:%s:", currentNodeXID.unparse().c_str());
                return INVALID_NODE_HANDLE;
            } else {
                return current_node;
            }
        } else {
            // If not, that node becomes the next node to check
            if(current_node == *it) {
                click_chatter("XIAPath::hid_node...() ERROR detected infinite loop, breaking out");
                break;
            }
            current_node = *it;
        }
    } while(current_node != dest);


    return INVALID_NODE_HANDLE;

}

XIAPath::handle_t
XIAPath::first_ad_node() const
{
	handle_t src = source_node();
	handle_t dst = destination_node();
	handle_t current_node = src;
	while(current_node != dst) {
		XID currentNodeXID(xid(current_node).unparse());
		if(currentNodeXID.type() == htonl(CLICK_XIA_XID_TYPE_AD)) {
			return current_node;
		} else {
			Vector<XIAPath::handle_t> child_nodes = next_nodes(current_node);
			Vector<XIAPath::handle_t>::iterator it;
			it = child_nodes.begin();
			current_node = *it;
		}
	}
	click_chatter("XIAPath::first_ad_node: ERROR: No AD found");
	return INVALID_NODE_HANDLE;
}

XID
XIAPath::xid(handle_t node) const
{
    return XID(_nodes[node].xid);
}

bool
XIAPath::replace_node_xid(String oldXIDstr, String newXIDstr)
{
	for(int i=0; i<_nodes.size(); i++) {
		if(oldXIDstr.equals(_nodes[i].xid.unparse().c_str(), _nodes[i].xid.unparse().length())) {
			click_chatter("XIAPath: replacing %s with %s", oldXIDstr.c_str(), newXIDstr.c_str());
			_nodes[i].xid.parse(newXIDstr);
		}
	}
	return true;
}

Vector<XIAPath::handle_t>
XIAPath::next_nodes(handle_t node) const
{
    return _nodes[node].edges;
}

XIAPath::handle_t
XIAPath::add_node(const XID& xid)
{
    Node n;
    n.xid = xid;

    _nodes.push_back(n);

    return _nodes.size() - 1;
}

bool
XIAPath::add_edge(handle_t from_node, handle_t to_node, size_t priority)
{
    if (priority < static_cast<size_t>(_nodes[from_node].edges.size()))
        _nodes[from_node].edges.insert(&_nodes[from_node].edges[priority], to_node);
    else
        _nodes[from_node].edges.push_back(to_node);
    return true;
}

bool
XIAPath::remove_node(handle_t node)
{
    _nodes.erase(&_nodes[node]);
    for (int i = 0; i < _nodes.size(); i++) {
        int num_edges =_nodes[i].edges.size();
        for (int j = 0; j < num_edges; j++) {
            if (_nodes[i].edges[j] == node) {
                // remove all incoming edges
                _nodes[i].edges.erase(&_nodes[i].edges[j]);
                j--;
                num_edges--;
            }
            else if (_nodes[i].edges[j] > node) {
                // adjust edge handle
                _nodes[i].edges[j]--;
            }
        }
    }
    if (_src != _npos &&_src >= node)
        _src--;
    if (_dst != _npos && _dst >= node)
        _dst--;
    return true;
}

bool
XIAPath::remove_edge(size_t from_node, size_t to_node)
{
    for (int i = 0; i < _nodes[from_node].edges.size(); i++)
        if (_nodes[from_node].edges[i] == to_node) {
            _nodes[from_node].edges.erase(&_nodes[from_node].edges[i]);
            break;
        }
    return true;
}

void
XIAPath::incr(size_t order)
{
    int i=0;
    if (!const_cast<XIAPath*>(this)->topological_ordering())
		return;
    for (; i < _nodes.size(); i++) {
		if (_nodes[i].order ==order)
			break;
    }
    if (_nodes[i].order!= order)
        return;
    _nodes[i].xid.xid().id[0]=1 +_nodes[i].xid.xid().id[0];
}

void
XIAPath::set_source_node(handle_t node)
{
    _src = node;
}

void
XIAPath::set_destination_node(handle_t node)
{
    _dst = node;
}

int
XIAPath::compare_with_exception(XIAPath& other, XID& my_ad, XID& their_ad)
{
	String this_path_str = unparse();
	click_chatter("XIAPath: this path:%s", this_path_str.c_str());
	String other_path_str = other.unparse();
	click_chatter("XIAPath: other path:%s", other_path_str.c_str());

	int my_ad_c_str_len = strlen(my_ad.unparse().c_str())+1;
	int their_ad_c_str_len = strlen(their_ad.unparse().c_str())+1;
	int this_path_c_str_len = this_path_str.length()+1;

	char *my_ad_c_str = (char *) calloc(my_ad_c_str_len, 1);
	char *their_ad_c_str = (char *) calloc(their_ad_c_str_len, 1);
	char *this_path_c_str = (char *) calloc(this_path_c_str_len, 1);

	strcpy(my_ad_c_str, my_ad.unparse().c_str());
	strcpy(their_ad_c_str, their_ad.unparse().c_str());
	strcpy(this_path_c_str, this_path_str.c_str());

	// Find first occurence of my AD in the path
	char *offset = strstr(this_path_c_str, my_ad_c_str);
	// Replace it with their ad
	strncpy(offset, their_ad_c_str, their_ad_c_str_len-1);
	// The modified path must match the other XIAPath
	String modified_this_path(this_path_c_str);
	click_chatter("XIAPath: modified this path:%s", modified_this_path.c_str());
	if(modified_this_path == other_path_str) {
		return 0;
	}
	return 1;
}

int
XIAPath::compare(XIAPath& other)
{
	String this_path_str = unparse();
	String other_path_str = other.unparse();
	if(this_path_str.compare(other_path_str)) {
		return 1;
	}
	return 0;
}

bool XIAPath::operator== (XIAPath& other) {
	   return !compare(other);
}

bool XIAPath::operator!= (XIAPath& other) {
	if(compare(other)) {
		return true;
	}
	return false;
}

void
XIAPath::dump_state() const
{
#ifndef NDEBUG_XIA
    StringAccum sa;
    sa << "_nodes =\n";
    for (int i = 0; i < _nodes.size(); i++) {
        sa << i << ": ";
        sa << _nodes[i].xid << ' ';
        for (int j = 0; j < _nodes[i].edges.size(); j++)
            sa << _nodes[i].edges[j] << ' ';
        sa << '(' << _nodes[i].order << ") ";
        sa << '\n';
    }
    sa << "_src = " << _src << '\n';
    sa << "_dst = " << _dst << '\n';
    click_chatter("%s\n", String(sa.data(), sa.length()).c_str());
#endif
}

#ifndef NDEBUG_XIA
struct XIASelfTest {
    XIASelfTest()
    {
        // alignment/packing test
        {
            assert(sizeof(struct click_xia) == 8);
            assert(sizeof(struct click_xia_xid) == 24);
            assert(sizeof(struct click_xia_xid_edge) == 1);
            assert(sizeof(struct click_xia_xid_node) == 28);
            struct click_xia hdr;
            assert(reinterpret_cast<unsigned long>(&(hdr.node[0])) - reinterpret_cast<unsigned long>(&hdr) == 8);
            assert(reinterpret_cast<unsigned long>(&(hdr.node[1])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 28);
            assert(reinterpret_cast<unsigned long>(&(hdr.node[2])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 56);

            assert(sizeof(struct click_xia_ext) == 2);
            struct click_xia_ext ext_hdr;
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[0])) - reinterpret_cast<unsigned long>(&ext_hdr) == 2);
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[1])) - reinterpret_cast<unsigned long>(&ext_hdr) == 3);
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[2])) - reinterpret_cast<unsigned long>(&ext_hdr) == 4);
        }

        // RE test
        {
            const char* s[] = {
                "AD:0000000000000000000000000000000000000000",
                "AD:0000000000000000000000000000000000000000 AD:0000000000000000000000000000000000000001",
                "AD:0000000000000000000000000000000000000000 ( AD:0000000000000000000000000000000000000002 ) AD:0000000000000000000000000000000000000001",
                "AD:0000000000000000000000000000000000000000 ( AD:0000000000000000000000000000000000000002 AD:0000000000000000000000000000000000000003 ) AD:0000000000000000000000000000000000000001",
            };
            for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++)
            {
                XIAPath p;
                assert(p.parse_re(s[i]));
                //click_chatter("%s\n", p.unparse_re().c_str());
                assert(p.unparse_re() == s[i]);
            }
        }

        // DAG test
        {
            const char* s[] = {
                "0 - AD:0000000000000000000000000000000000000000",
                "0 - AD:0000000000000000000000000000000000000000 1 - AD:0000000000000000000000000000000000000001",
                "0 - AD:0000000000000000000000000000000000000000 1 - AD:0000000000000000000000000000000000000001 2 - AD:0000000000000000000000000000000000000002",
            };
            for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++)
            {
                XIAPath p;
                assert(p.parse_dag(s[i]));
                //click_chatter("%s\n", p.unparse_dag().c_str());
                assert(p.unparse_dag() == s[i]);
            }
        }

        // path manipulation test
        {
            XIAPath p;
            XIAPath::handle_t src_node = p.add_node(XID());
            p.set_source_node(src_node);
            XIAPath::handle_t dst_node = p.add_node(XID("AD:0000000000000000000000000000000000000000"));
            p.set_destination_node(dst_node);
            p.add_edge(src_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t prev_dst_node = dst_node;
            dst_node = p.add_node(XID("AD:0000000000000000000000000000000000000001"));
            p.set_destination_node(dst_node);
            p.add_edge(prev_dst_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t fallback_node = p.add_node(XID("AD:0000000000000000000000000000000000000002"));
            p.add_edge(prev_dst_node, fallback_node);
            p.add_edge(fallback_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t fallback_node2 = p.add_node(XID("AD:0000000000000000000000000000000000000003"));
            p.add_edge(fallback_node, fallback_node2);
            p.add_edge(fallback_node2, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.remove_node(fallback_node2);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.remove_node(fallback_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.add_edge(src_node, dst_node, 0);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());
        }

        click_chatter("test passed.\n");
    }
};

static XIASelfTest _self_test;
#endif

CLICK_ENDDECLS
