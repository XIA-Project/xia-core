/*****************************************
 * File Name : sample.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/
#ifndef SCIONPATHSERVER_HEADER_HH_
#define SCIONPATHSERVER_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <vector>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

#include <tr1/unordered_map>

/*include here*/
#include "define.hh"
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionprint.hh"
//#include "uppath.hh"
//#include "upqueue.hh"

struct upPath
{
    uint16_t hops;
    uint16_t numPeers;
    std::vector<uint64_t> path;
    std::multimap<uint64_t, uint64_t> peers;
    uint8_t* msg;
};


struct downPath{
    uint16_t hops;
    uint16_t numPeers;
    std::vector<uint64_t>path;
    std::multimap<uint64_t, uint64_t> peers;
    uint64_t destAID;
    uint8_t* msg;
};

struct end2endPath{
    bool isShortcut;
    uint16_t hops;
    uint16_t numPeers;
    std::vector<uint64_t>path;
    std::multimap<uint64_t, uint64_t> peers;
    uint64_t destAID;
    uint8_t* msg;
};

class UPQueue
{

    // Forward declaration of Element struct.
    //struct Element;

public:

    /**
     * PathQueue (constructor)
     * -------------------------------------------------------------------------
     * Creates an empty PathQueue object with size limit maxSize.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Creates a new PathQueue object with the specified size limit.
     */
    UPQueue(size_t maxSize);

    // Copy constructor.
    //UPQueue(const UPQueue &orig);

    // Assignment operator.
	//SLP: blocked...
    //UPQueue& operator=(const UPQueue &rhs);

    // Destructor.
    ~UPQueue();

    /**
     * isEmpty (constant member function)
     * -------------------------------------------------------------------------
     * Checks whether the queue is empty or not.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns true is the queue is empty and false otherwise.
     */
    bool isEmpty() const;

    /**
     * size (constant member function)
     * -------------------------------------------------------------------------
     * Determines the current size of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the size of the queue.
     */
    size_t getSize() const;

    /**
     * enqueue (member function)
     * -------------------------------------------------------------------------
     * Adds a path to the end of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   The up-path to add to the queue.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   The new path is added to the end of the queue.
     */
    void enqueue(upPath* path);

    /**
     * dequeue (member function)
     * -------------------------------------------------------------------------
     * Deletes the path at the front of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   The up-path at the front of the queue is dequeued.
     */
    void dequeue();

    /**
     * headPath (member function)
     * -------------------------------------------------------------------------
     * Returns the path at the front of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the first path in the queue. The path remains on the queue.
     */
    upPath* headPath();

    /**
     * tailPath (member function)
     * -------------------------------------------------------------------------
     * Returns the path at the end of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the last path in the queue. The path remains on the queue.
     */
    upPath* tailPath();

private:

    size_t head;
    size_t tail;
    size_t size;
    size_t maxSize;

    upPath** paths;
};

struct PathHash
{
    size_t operator()(const scionHash &h) const;
};

enum pathCriteria {DISTANCE,
                   FRESHNESS,
                   UNASSIGNED_3,
                   UNASSIGNED_4,
                   UNASSIGNED_5,
                   UNASSIGNED_6,
                   UNASSIGNED_7,
                   UNASSIGNED_8};

CLICK_DECLS

class SCIONPathServer : public Element { 
    public :
        SCIONPathServer():_timer(this), _task(this){};
        ~SCIONPathServer(){clearPaths(); delete scionPrinter;};
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "SCIONPathServer";}
        
        const char *port_count() const {return "-/-";}
        
        const char *processing() const {return PULL_TO_PUSH;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        void run_timer(Timer *);


     //   void push(int, Packet*); 

        bool run_task(Task* );

        void parseDownPath(uint8_t* pkt);

        void parseUpPath(uint8_t* pkt);

        void printDownPath();

        int buildPath(uint8_t* pkt, uint8_t* output);
        
        int sendRequest(uint64_t, HostAddr);

        int sendUpPath(uint32_t pref=0);
		//SL:
		//Added to include recipient address
        int sendUpPath(const HostAddr &requestId, uint32_t pref=0);

        scionHash createHash(uint8_t* pkt);

        void initVariables();

        void sendPacket(uint8_t* packet, uint16_t packetLength, int port);


        void parseTopology();
        
        void constructIfid2AddrMap();

		//SLP:
		void clearPaths();
    private:
        char m_sLogFile[MAX_FILE_LEN];
        int m_iLogLevel;
        int m_iQueueSize;
        int m_iNumRetUP;
        uint64_t m_uAid;
        uint64_t m_uAdAid;
        std::tr1::unordered_map<scionHash, UPQueue*, PathHash> upPaths;
        std::multimap<uint16_t, downPath> downPaths; 
        std::map<int,ClientElem> m_clients;
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         
        std::map<uint16_t, HostAddr> ifid2addr;

		//SL:
		std::multimap<uint64_t, HostAddr> pendingDownpathReq;
		std::map<uint64_t, time_t> pendingDownpathReqTime;
       
        Timer _timer;
        Task _task;
        int m_iTval;
        String m_sConfigFile;
        String m_sTopologyFile;
        SCIONPrint* scionPrinter;
};

CLICK_ENDDECLS


#endif
