/*
 * File: this header file contains the necessary information for the
 *       agent for the Hierarchical Location Service
 * Author: Wolfgang Kiess
 *
 */


#ifndef ns_hls_h
#define ns_hls_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "locservices/locservice.h"
#include "locservices/chc.h" // for the caches

// the STORE_CLEAN_INTERVALL is the intervall in which the
// content of the stores (active and passive) is checked 
#define STORE_CLEAN_INTERVALL 4


// the following is for the request table
#define HLS_REQTABLE_TIMEOUT 3.0
#define HLS_REQTABLE_SIZE    97

// the number of slots in the acitveEntries CHC; at the moment
// it should be a very small number because we have to do
// a linear search on it (the HandoverManager)
# define HLS_ACTIVE_ENTRIES_SIZE 37

#define hls_verbose true
#define hls_trace_updates true // must be set explicitly to trace
// the sending and receiving of updates
#define hls_trace_handovers true // tracing of sending and receiving
// the handover of activeEntries

#define HLS_HANDOVER_INTERVALL 2 // in seconds, the intervall in
// which the handover manager will check the entries
#define HLS_HANDOVER_JITTER 0.500 // the jitter in the timer to
// avoid collisions

class UpdateSender;
class Store;
class Cellbuilder;
class UpdateReceiver;
class RequestProcessor;
class CleanPosinfoTimer;
class HLS;
class HLSLocationCache;
class HandoverTimer;



#define HDR_HLS(p) (hdr_hls::access(p))

#define HLS_TTL 60
#define HLS_REQUEST_TTL_PER_LEVEL 30
#define HLS_UPDATE_TTL_PER_LEVEL 10
// if the above HLS_REQUEST_TTL_PER_LEVEL is for example = 30, a 
// request to a first level rc gets a ttl of 30, one to a 
// second level RC a ttl of 60 and so on, it is always multiplied
// by the level.

#define HLS_UPDATE             1
#define HLS_REQUEST            2
#define HLS_REPLY              3
#define HLS_CELLCAST_REQUEST   4
#define HLS_CELLCAST_REPLY     5
#define HLS_CIRCLECAST_REQUEST 6
#define HLS_HANDOVER           7
// handover packets are aktive entries which have
// left their cell and must be retransmitted to that cell
// (don't use update packets to distinguish them later 
// in the tracefiles)

// status_ definitions in the hls header
#define INITIAL_STATUS 0
//#define NO_INFORMATION_STATUS 1
//#define HAVE_INFORMATION_STATUS 2
#define ON_THE_FLY_UPDATE       3 // a poslookup has been executed 
// successfully for this packet, we have included our posinfo in
// the SRC to keep my counterpart in sync 
#define REQUEST_LOCATED         4 // when we have found the location
// of a node, we mark the packet with this flag.
// When the packet is dropped, we know that it's because there is no
// (greedy) route to the target.
#define REQUEST_LOCATED_AND_RETRIED 5 // before dropping a packet 
// because of a "located but no route" error, we try to send it
// directly to that node, it's quite possible that a beacon was lost
// and the node is still in reach. To avoid infinite recursion (e.g when
// the MAC-layer returns the packet), we change the state once again to 
// drop the packet afterwards)

// DROP Reasons
#define LOCATED_AND_NO_ROUTE     "LNR"
#define CIRCELCAST_ALREADY_SEND  "CAS"
#define CIRCLECAST_TIMEOUT       "CTO"



#define HLS_UPDATE_JITTER         1 /* 0-1000 ms Delay in update to 
					   avoid collisions */
#define HLS_UPDATE_CHECK_INTERVALL      2     // in this intervall, HLS will check if
                                              // updates are necessary
#define HLS_MAX_UPDATE_INTERVALL        9    // this defines the Maximum time which 
// should lie between updates for the top level cell (and the cell on the second highest
// level if  TIME_TRIGGERED_FOR_SECOND_HIGHEST_LEVEL is defined)
#define TIME_TRIGGERED_UPDATES_FOR_SECOND_HIGHEST_LEVEL

// how long an entry will be kept in the active and passive
// store before it is deleted
#define ENTRY_LIFETIME 20
#define HLS_MAX_CACHE_LOOKUP_AGE       10  // determines the max age of a cache lookup
// (i.e. we return the position out of our cache if it is young enough to be still
// valid)
#define HLS_UPDATE_DISTANCE            250 // distance before a update gets necessary
#define HLS_UPDATE_STARTUP             2   // the second in which the earliest update will be
#define HLS_UPDATE_STARTUP_MAX_JITTER  11  // avoid all the collisions at startup
#define HLS_UPDATE_STARTUP_GROUPS      5  // defines the number of groups in which the 
// nodes schedule their first updates (the membership is determined by a modulo operation
// on the node's id)


#define NUMBER_OF_NEIGHBOR_CELLS 8 // the max number of cells which another cells
// can have as neighbors
#define NO_CELL                  -1 // to initialize the neighbors table in
// a circelcast packet and to indicate that the entry is empty (e.g if we
// have to transmit the circle cast packet to the neighbors of a cell which 
// is a cell on the border, this cell doesn't have 8 neighbors but only 5.
// Thus the neighbors fields from 5-7 will contain NO_CELL

// if aggressive caching is defined, nodes will grep all information they
// could an cache them
//#define AGGRESSIVE_CACHING

#define CELLCAST_BUFFER_SIZE 5  // the number of places in the cellcast buffer
// is the max number of cellcast which can be performed by a node in parallel
#define CELLCAST_BUFFER_ENTRY_TIMEOUT 0.2   // how long the sender of a 
// cellcast request will wait for answers. After the expiration of this 
// amount of time, the cellcast will be declared to have failed, the
// request will be forwarded to the RC on the next level
#define CIRCLECAST_ENTRY_TIMEOUT 0.6 // how long the sender of a 
// circlecast request will wait for answers. After the expiration of this 
// amount of time, the circlecast will be declared to have failed

#define CELLCAST_BUFFER_CHECK_INTERVALL 0.1 // how often the cellcast buffer
// will be checked for expired entries;
#define CELLCAST_ANSWER_JITTER          0.08 // the intervall in which we chose
// a random jitter before sending an answer (if we haven't heard an answer of
// somone else before)

////////////////////////////////////////////////////////
// packet format and other types ///////////////////////
////////////////////////////////////////////////////////
typedef struct nodeposition {
  nsaddr_t id;
  double ts;
  position pos;
  
  int targetcell; // the id of the cell to which this nodposition
  // belongs to (if it was send as a update)
  // identifies the cell where it should be stored as
  // activeEntry

  // copies the information in n in this struct
  void copyFrom(nodeposition *n)
  {
    id = n->id;
    ts = n->ts;
    pos.x = n->pos.x;
    pos.y = n->pos.y;
    pos.z = n->pos.z;
    targetcell = n->targetcell;
  }
};

// this is the unique request id which every request 
// will contain (for easier tracing)
struct request_id {
  int node;  // the node which sent the request
  int nr;    // the unique number

  request_id()
  {
     node = NO_NODE; 
     nr = -1;
  }

  request_id(int node, int nr)
  {
    this->node = node;
    this->nr   = nr;
  }

  inline bool equals(request_id* id2) { 
    if((node == id2->node) && 
       (nr   == id2->nr)) 
      return true;
  return false;
  }

  inline void set(request_id id2)
  {
    node = id2.node;
    nr   = id2.nr;
  }

  inline void init() { node = NO_NODE; nr = -1;}

};

// this identifies a cell in the packet
// used for updates and requests, therefore all geo-anycast
// packets
typedef struct struct_cell {
  int id;         // id of the cell
  int level;      // the level on which the cell is (perspective
                  // of the sender)
  position pos;   // position of the cell
  
  void init()
  {
    id = NO_NODE;
    level = -1;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
  }
};

struct hdr_hls {
  nodeposition src;
  nodeposition dst;

  int cellcastRequestSender; // only for cellcasts, in all other
  // cases, this is -2
  
  int type_; // the type of the packet, e.g. an update
  int status_; // the status, e.g. for marking the return packet
  // to a broadcast request in a cell (you can mark it as
  // "i had information about the node" or as "i don't know the
  // location of the node"; at the moment, no negative packets
  // are send
  
  request_id reqid; // only for request packets (all packets 
  // associatet with a request will have the same reqid, that
  // means the request, the cellcast packets and also the reply)
  // if it isn't a request packet, the reqid will contain -1
  // in both fields

  struct_cell cell; // the target cell of a geo-anycast
  // that means the target of a update or a request

  // only for tracing updates, contains the reason of the update
  const char* updreason_;

  // the following fields are necessary for the backup of the 
  // cell on the highest level. They are used in the cellcastCircle
  // mode
  int neighbors[NUMBER_OF_NEIGHBOR_CELLS]; // contains the list of 
  // neighbors to which the packet should be sent (first to the 
  // neighbor[0] cell, than to the neighbor[1] cell,..
  // the initialized entries contain -1
  int neighborIndex; // the index in the array neighbors indicastes 
  // to which cell the packet is actually send
  bool circleCastAlreadySend; // this field is a marker for a packet;
  // it doesn't have to be send over the "network", it is a workaround
  // to avoid keeping a list of all packets for which we have sent a
  // circlecast. Instead we mark the packet and drop it if we want to
  // send a second circlecast for it.
  int numberOfNodeinfosToHandover; // this parameter is only used in the
  // advanced handover mangaer: it indicates how much nodeposition entries
  // we transmit with this packet

   void init() {
     src.id = NO_NODE;
     src.ts = -1.0;
     src.pos.x = -1.0;
     src.pos.y = -1.0;
     src.pos.z = -1.0;

     dst.id = NO_NODE;
     dst.ts = -1.0;
     dst.pos.x = -1.0;
     dst.pos.y = -1.0;
     dst.pos.z = -1.0;

     cellcastRequestSender = -2;

     type_ = 0;
     status_ = INITIAL_STATUS;

     reqid.init();

     cell.init();

     updreason_ = "";

     for(int i=0;i<NUMBER_OF_NEIGHBOR_CELLS;i++)
       {
	 neighbors[i] = NO_CELL;       
       }
     neighborIndex = -1;

     circleCastAlreadySend = false;
     numberOfNodeinfosToHandover = -1;
   }
 
  static int offset_;
  inline static int& offset() { return offset_;}
  inline static hdr_hls* access(const Packet* p) {
    return (hdr_hls*) p->access(offset_);
  }
};

// stores the information necessary to go on after a node
// receives a response to a cellcast request
struct cellcastEntry {
  double timeout; // the timestamp when the cellcast has a timeout
  request_id* id;  // the id of the request (consiting of the
  // node and a unique number)
  bool circlecast;
  Packet* p; // the request packet which contains the info
};

////////////////////////////////////////////////////////
// prototypes //////////////////////////////////////////
////////////////////////////////////////////////////////

// is responsible for sending position information 
// updates
class UpdateSender
{
  public :
    UpdateSender(){};
    UpdateSender(Cellbuilder* cellbuilder, HLS* hls);
  virtual void start(); // responisble for starting the
                        // the update sender

  protected : 
    Cellbuilder* cellbuilder_;
  HLS * hls_;
    
};

// the methods here are called whenever an update packet
// arrives at the node
class UpdateReceiver
{
  public :
    UpdateReceiver(){};
    UpdateReceiver(HLSLocationCache* activeEntries, 
		   HLSLocationCache* passiveEntries,
		   HLSLocationCache* outOfCellEntries,
		   HLS* hls);
    virtual void recv(Packet* &p, bool forceActiveSave);
  protected :
    HLSLocationCache* activeEntries_;
    HLSLocationCache* passiveEntries_;
    HLSLocationCache* outOfCellEntries_;
    HLS*   hls_;
};

// the methods here are called whenever a position request
// arrives at a node
class RequestProcessor
{ 
  public :
    RequestProcessor(){};
    RequestProcessor(HLSLocationCache* passiveEntries, HLS* hls);
    virtual void recv(Packet* &p);
    virtual void recvCellcastRequest(Packet* &p);
    virtual void cellcastReplyOnMacReceived(const Packet* p);
    virtual void recvCellcastReply(Packet* &p);
    virtual void processRequestUnreachableCell(Packet* &p);
    virtual void recvCirclecastRequest(Packet* &p, bool fromDropCallback);    
    virtual void circlecastRequestOnMacReceived(const Packet* p);

  protected :
    HLSLocationCache* passiveEntries_;
    HLS*   hls_;
    virtual Packet* newReply(Packet* req, struct nodeposition* infosrc);
    virtual void sendCellcastReply(const Packet* req, struct nodeposition* info);
};

// this class is responsible for managing the handover of active position
// information. That means it tests if we are still in the correct cell
// and sends a handover packet to the RC if we have left it.
class HandoverManager
{  
  public :
  HandoverManager(HLSLocationCache* activeEntries, 
		  HLSLocationCache* passiveEntries,
		  HLSLocationCache* outOfCellEntries,
		  HLS* hls,
		  Cellbuilder* cellbuilder); 
  virtual ~HandoverManager();
  // will be called whenever HLS receives a handover packet.
  virtual void recv(Packet* &p, bool forceActiveSave)
    {printf("HandoverManager.recv() called\n");};
  virtual void timerCallback(){printf("HandoverManager.callback() called\n");}; 
  // called when the timer expired

  protected :
    // called when we discoverd in the timerCallback that there is
    // information which should be in another cell
    virtual void handoverInformation(nodeposition* info){};

    HLSLocationCache* activeEntries_;
    HLSLocationCache* passiveEntries_;
    HLSLocationCache* outOfCellEntries_;
    HLS*   hls_;
    Cellbuilder* cellbuilder_;
    HandoverTimer* timer_;      // responsible for periodically calling 
                               // the callback
    int lastCell;         // remembers the id of the last cell in which
    // the node has been (thus enables me to determine when we changed
    // the cell)
}; // end handover manager


class HandoverTimer : public TimerHandler
{
  public :
    HandoverTimer(HandoverManager* manager, double intervall);

  protected :
    void expire(Event* e);

  private :
    double intervall_; // meassured in seconds
  HandoverManager* manager_;  
};// end HandoverTimer

class HLS : public LocationService {

 public:
    HLS(Agent* p);
    ~HLS();
 
    void recv(Packet* &p);
    bool poslookup(Packet *p);
    void init(); 
    void evaluatePacket(const Packet *p);
    int hdr_size(Packet* p); // returns the size the LS part of the header
    void dropPacketCallback(Packet* &p);
    int getCell(); // returns the id of the cell in which we are
    // at the moment
    

    protected :
      Packet* allocpkt() { return parent->allocpkt(); }
      Cellbuilder* cellbuilder_;     
      nodeposition* findEntry(int nodeid);
      nodeposition* findActiveEntry(int nodeid);
      nodeposition getNodeInfo();
      
    private :
      UpdateSender* updateSender_;
      HLSLocationCache* activeEntries_;  // contains the active entries
      // (these are the entries which belong to the cell
      // in which we are, no cached entries here)
      HLSLocationCache* passiveEntries_; // contains the passive entries
      // (cached entries which are filtered out of the
      // network traffic, posinfo extracted from beacons and replies to
      // location query packets)
      HLSLocationCache* outOfCellEntries_; // contains the entries
      // which must be stored (the target cell couldn't be reached, so
      // we have to store them and retry a transmit to that cell later).
      UpdateReceiver* updateReceiver_;
      RequestProcessor* requestProcessor_;
      HandoverManager* handoverManager_;

      LSRequestCache* reqtable_; // stores the requests we have sent
      int lastReqNr;             // stores the number of the last req

      void sendGeocast();
      bool sendRequest(int nodeid, int level);
      void processReply(Packet* &p);

    // declare the update senders as friends to enable their
    // access to the parent, ...
    friend class BasicUpdateSender;
    friend class BasicUpdateReceiver;
    friend class RequestProcessor;
    friend class BasicRequestProcessor;
    friend class CellcastAnswerTimer; // for access to parent_
    friend class BasicHandoverManager;
    friend class AdvancedHandoverManager;
    
};

// returns the distance between the actual location of the node nodeid
// and the given position
extern double distance(int nodeid, position pos);

void printUsageStatistics();

#endif // ns_hls_h
