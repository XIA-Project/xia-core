// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaheader.cc" -*-
#ifndef CLICK_XIAHEADER_HH
#define CLICK_XIAHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <vector>

CLICK_DECLS
class StringAccum;
class XIDGraphNode;
class XIAHeader;

#define MAX_OUTDEGREE 4

/** @class XIDGraphNode
 *  @brief This represents a node of a XID graph. 
 */
class XIDGraphNode {
    friend class XIAHeader;
    public:
    XIDGraphNode(const struct click_xid_v1 xid, int id) :xid_(xid), current_edge(0), id_(id), primary_path(false)
    { 
        for (int i=0;i<MAX_OUTDEGREE;i++) edge[i]= NULL;
    }

    void connectOutgoing(int pos, XIDGraphNode* to) 
    {
        edge[pos] = to; 
    }

    inline int id() { return id_; }
    inline struct click_xid_v1* xid() { return &xid_; }

    private:
        struct click_xid_v1 xid_; 
        int current_edge;
        int id_;
        bool primary_path;
        XIDGraphNode * edge[MAX_OUTDEGREE];
};


/** @class XIAHeader
 *  @brief A helper class to manipulate XIA header
 *
 *  The XIA header class type stores and represents a graph of XIDs 
 *  as a representation of a XIA header. 
 *  This class also translates the graph structure to actual packet format(s).
 *  You may extend this class to support alternative packet formats. 
 *  (Currently, click_xia is the only header format supported.)
 *  However, the underlying graph representation is very generic,
 *  and it is intended so as to preserve the expressibility.
 *  Not all packet formats support all graphs of XIDs. 
 *  Note that this class does limited sanity check and may return a malformed header
 *  if the underlying graph is not well specified or can't be encoded to the
 *  requested packet format.
 *
 *  <pre>
 *    Eaxmple:
 *      SRC_STACK = XID1:XID2:XID3
 *      DEST= XID4
 *    
 *    Graph should look like: 
 *     XID1->XID2->XID3->XID4
 *     src_stack should poing to XID3.  (user sets this by calling setSourceStack)
 *     root points to XID1. Root is the first XID added to the graph.
 *     intent is set to be XID4. (Intent is automatically set by traversing the first egde until the end.
 *
 *  </pre>
 */

class XIAHeader { public:

    /** @brief Construct an XIAHeader */
    XIAHeader(): root(0), src_stack(0), version(XIA_V1), hlim(128), node_counter(0)
    {
    }

    ~XIAHeader();

    void setSourceStack(int k)
    {
        src_stack = lookup(k);
    }

    void connectXID(int priority, int fromnode, int tonode)  {
        // fromnode [priority] -> tonode
        XIDGraphNode* from = lookup(fromnode);
        XIDGraphNode* to = lookup(tonode);
        from->connectOutgoing(priority, to);
    }

    int addXID(struct click_xid_v1& xid)
    { 
        XIDGraphNode* n = new XIDGraphNode(xid, node_counter++);
        if (root==NULL) root = n; // first node becomes the root
        nodelist.push_back(n);
        return n->id();
    }

    struct click_xid_v1* getXID(int id)
    {
        XIDGraphNode* n= lookup(id);
        if (n==NULL) return NULL;
        return n->xid();
    }

    /* @brief returns the number of iterations. If the packet is malformed, -1 is returned */
    int niter() 
    { 
        int cnt=0;
        if (!src_stack) return -1;
        XIDGraphNode* curr = src_stack;
        while (curr->edge[0]) {
            curr = curr->edge[0];
            curr->primary_path= true;
            cnt++;
        }
        return cnt;
    }

    /* @brief Returns the length of the xia header */
    int click_xia_len() 
    {
         if (niter()<0) return -1;
         return sizeof(click_xia) + niter() + sizeof(click_xid_v1)*nodelist.size(); 
    }

    /* @brief Constucts the XIA (click_xia) header 
     *
     * It allocates memory and returns click_xia header
     * click_xia supports only one fallback.
    */
    click_xia* header() 
    {
        int len= click_xia_len();
        if (len<=0) return NULL;  /* malformed graph */
        if (src_stack_len()<0) return NULL; /* malformed graph */

        char * buf = new char[len];
        return dump(buf, len);
    }

    int src_stack_len()
    {
        int cnt=1;
        if (!src_stack) return -1;
        XIDGraphNode* curr = root;
        while (curr->edge[0] && src_stack!=curr) {
            curr = curr->edge[0];
            cnt++;
        }
        if (src_stack!=curr) return -1;
        return cnt;
    }

    struct click_xia* dump(char *b, int len) 
    {
        int pktlen= click_xia_len();
        if (pktlen<=0) return NULL;  /* malformed graph */
        if (src_stack_len()<0) return NULL; /* malformed graph */

        if (pktlen> len) return NULL;

        click_xia *header= reinterpret_cast<click_xia*>(b); 

        header->niter = niter();
        //click_chatter("niter %d\n", header->niter);
        header->nxid = nodelist.size();
        header->ver= version;
        header->intent = niter();
        header->next = 1;
        header->next_fb = NEXT_FB_UNDEFINED; // adjust later if not the case
        header->hlim= hlim;
        //header->nexthdr;
       
        int cnt =0;  /* iteration count */
        int next_fb_pos = header->niter+1 + src_stack_len();
        XIDGraphNode* curr = src_stack;
        while (curr->edge[0]) {
            /* handle the primary path from src to destinations */
            header->xid()[cnt] = *curr->xid();

            /* handle fallbacks */
            XIDGraphNode* fb = curr->edge[1]; /* only one fallback */
            if (fb) {
                header->fb_offset()[cnt]=next_fb_pos;
                // for each list of XIDs in this fallback
                // copy the XID to header
                while (fb->edge[0] && !fb->edge[0]->primary_path){
                    header->xid()[next_fb_pos] = *fb->xid();
                    next_fb_pos++;
                }
            } else {
                header->fb_offset()[cnt]= FALLBACK_INVALID;
            }
            curr = curr->edge[0];
            cnt++;
        }

        /* handle source stack */ 
        int next_pos = header->niter+1;

        XIDGraphNode* src = root;
        while (src->edge[0] && src_stack!=src) {
            header->xid()[next_pos] = *src->xid();
            src = src->edge[0];
            cnt++;
        }

        return header;
    }


    String toString();
   

    private:
    XIDGraphNode* lookup(int id) { return nodelist.at(id); }
    XIDGraphNode* root;       // root
    XIDGraphNode* intent;     // pointer to the last XID of dest
    XIDGraphNode* src_stack;  // points to the last XID of source. From root, first k xids are source (e.g. SRC= AD1:AD2:HID )
    std::vector<XIDGraphNode*> nodelist; // nodes are ordered by its id 
    uint8_t version; 
    uint8_t hlim;
    int node_counter;

};



CLICK_ENDDECLS
#endif
