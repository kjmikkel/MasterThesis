/* this file contains the definition for the basic
   implementations of the components and also the 
   prototypes for these components
*/

#ifndef hls_basic_h
#define hls_basic_h

#include "timer-handler.h"
#include "cellbuilder.h"
//#include "packet.h"
#include "hls.h"


////////////////////////////////////////////////////////
// basic implementations ///////////////////////////////
////////////////////////////////////////////////////////

// the StopAndGoTimerHandler is a timer which counts how
// often he has been scheduled. It has an internal counter 
// which can be incremented and decremented, if the counter
// is 0, the timer will not expire, each time it expires, the
// counter will be decremented.
class StopAndGoTimerHandler : public TimerHandler
{
  public :
    StopAndGoTimerHandler(double intervall);
  void incrementCounter();
  void decrementCounter();
  
  protected :
    void expire(Event* e);
  // this is the expire method which should be used by the child
  // classes. With this construction, the whole counter management
  // can be kept in the parent class
  virtual void stopAndGoTimerExpired() = 0;//{printf("parent stopandgo\n");};
  private :
    int counter_;
  double intervall_;
};

class BUSTimer;

// the basic update sender sends an update each n
// milliseconds (where n is a parameter the user can
// determine) to every level.
class BasicUpdateSender : public UpdateSender
{
 public:
  BasicUpdateSender(Cellbuilder* cellbuilder, HLS* hls);
  ~BasicUpdateSender();
  void start();
  void timerExpired();
 
 private:
  double updateIntervall_;   // meassured in seconds
  BUSTimer* timer_;

  nodeposition sendUpdatePacketToCell(int cellid, int level);

  nodeposition* lastUpdatePosition_; // contains the position from which
  // we have send the last update (for each level)

  // Update Trace Support
  const char* upd_reason;
};

class BasicUpdateReceiver : public UpdateReceiver
{
  public :
    BasicUpdateReceiver(HLSLocationCache* activeEntries, 
			HLSLocationCache* passiveEntries, 
			HLSLocationCache* outOfCellEntries,
			HLS* hls);
    void recv(Packet* &p, bool forceActiveSave);
};

//declaration necessary for the BasicRequestProcessor
class CellcastRequestCache;
class CirclecastRequestCache;
class AnswerTimer;
class CleanTimerListTimer;

class BasicRequestProcessor : public RequestProcessor
{
  public :
    BasicRequestProcessor(HLSLocationCache* passiveEntries, HLS* hls);
  ~BasicRequestProcessor();
    void recv(Packet* &p); 
    void recvCellcastRequest(Packet* &p);
    void cellcastReplyOnMacReceived(const Packet* p);
    void recvCellcastReply(Packet* &p); 
    void processRequestUnreachableCell(Packet* &p_old);
    void recvCirclecastRequest(Packet* &p, bool fromDropCallback);
    void circlecastRequestOnMacReceived(const Packet* p);
    void cellcastTimerCallback(Packet* p);
    void dropAndTraceRequestPacket(Packet* p, char *reason);
    protected :      
      void cleanTimerList();

    private :

    AnswerTimer* answerTimerList_;
    CellcastRequestCache* cellcastRequestCache_;
    CirclecastRequestCache* circlecastRequestCache_;
    CleanTimerListTimer* cleanTimerListTimer_;
      
    void sendCellcastRequest(int nodeid, request_id reqid, struct_cell* cell);
    
    void sendCirclecastRequest(int nodeid, request_id, int* cells);
    // the cellcastBuffer stores the packets which caused the sending
    // of a cellcast request.If an answer arrives, the packet can be
    // forwarded to the target. If no answer arrives, the packet can
    // be forwarded to the RC on the next higher level
    cellcastEntry* cellcastBuffer_;


    friend class CellcastAnswerTimer;
    friend class CirclecastAnswerTimer;
};


// the answer timer triggers the sending of the answer to a
// cellcast request: it is quite likely that more than one
// node have the requested information. To avoid collisions
// in sending, every node which receives a request schedules
// a timer and sends an answer if he hasn't heared another 
// answer packet before

class AnswerTimer : public TimerHandler
{
  public :
    AnswerTimer(BasicRequestProcessor* brp, double timeout, 
		nodeposition* information);
  virtual ~AnswerTimer();
  AnswerTimer* next;
  // attributes
  request_id reqId_;
  virtual hdr_hls* getHlsHeaderOfPacket();

  protected :
    void expire(Event* e);
  BasicRequestProcessor* brp_;
  nodeposition info_; // the info which we want to sent

  private :
};

class CellcastAnswerTimer : public AnswerTimer
{   
  public :
  CellcastAnswerTimer(BasicRequestProcessor* brp, double timeout, 
		      Packet* p, nodeposition* information);
  ~CellcastAnswerTimer(); 
  hdr_hls* getHlsHeaderOfPacket();
  protected :
    void expire(Event* e);
  private :
    Packet* packet_;
};

class CirclecastAnswerTimer : public AnswerTimer
{
  public :
    CirclecastAnswerTimer(BasicRequestProcessor* brp, double timeout, 
			  const Packet* p, nodeposition* information);
  ~CirclecastAnswerTimer(); 
  hdr_hls* getHlsHeaderOfPacket();
  protected :
    void expire(Event* e);
  private :
    Packet* packet_;
};

class BUSTimer : public TimerHandler 
{
  public :
    BUSTimer(BasicUpdateSender* bus, double intervall);

  protected :
    void expire(Event* e);

  private :
    double intervall_; // meassured in seconds
  BasicUpdateSender* bus_;
};

class BasicHandoverManager : public HandoverManager
{  
  public :
    BasicHandoverManager(HLSLocationCache* activeEntries, 
			 HLSLocationCache* passiveEntries,
			 HLSLocationCache* outOfCellEntries,
			 HLS* hls, 
			 Cellbuilder* cellbuilder); 
  // will be called whenever HLS receives a handover packet.
  void recv(Packet* &p, bool forceActiveSave);
  void timerCallback(); // called when the timer expired
  
 private:
  void handoverInformation(nodeposition* info);
  void checkCacheAndTransmit(HLSLocationCache* cache, int actualCell);
  
}; // end handover manager

// the advanced hm sends all information to a cell in one
// packet in the data compartement
class AdvancedHandoverManager : public HandoverManager
{  
  public :
    AdvancedHandoverManager(HLSLocationCache* activeEntries, 
			 HLSLocationCache* passiveEntries,
			 HLSLocationCache* outOfCellEntries,
			 HLS* hls, 
			 Cellbuilder* cellbuilder); 
  // will be called whenever HLS receives a handover packet.
  void recv(Packet* &p, bool forceActiveSave);
  void timerCallback(); // called when the timer expired
  
 private:
  void handoverInformation(nodeposition* info,  int numberOfInfos);
  void checkCacheAndTransmit(HLSLocationCache* cache, int actualCell);
  void bubblesort(nodeposition* A, int size);
  void exchange(nodeposition* a, nodeposition* b);
  
}; // end handover manager

////////////////////////////////////////////////////////////
// CACHES //////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// replaces the old Store which couldn't grow
class HLSLocationCache : public ChainedHashCache {
 public:
     HLSLocationCache(class LocationService* parent, 
		     const unsigned int hash_size = CHC_BASE_SIZE, 
		     const double lifetime = 0.0 ) 
	: ChainedHashCache(parent,hash_size,lifetime) {}
    ~HLSLocationCache() {}
    
    // search and remove already defined in parent class, the 
    // code below is just for documenatation
    bool add(nodeposition*);
    bool update(nodeposition*);

 private:
    bool iupdate (void*, void*);
    bool itimeout (void*);
    void iprint (void*);
    inline void* inew() { return ((void*)(new nodeposition)); }
    inline void idelete (void* ventry) { delete (nodeposition*)ventry; }
    inline void icopy (void* ventry, void* vdata) 
      { memcpy(ventry,vdata,sizeof(nodeposition)); }
};

class RequestCacheTimer;

// has an internal cache which stores request pakets and timers
// for theses packets. When the timer expires, a callback will be called
// to inform that the request hasn't been successfull. There exists also
// a method to get the packets back
class RequestCache {
  public :
    RequestCache(int initialCacheSize, 
		 BasicRequestProcessor* requestProcessor);
  ~RequestCache();
  void timerCallback(int number);
  void store(Packet* p, double lifetime);
  Packet* getRequest(request_id reqid);
  virtual void timerExpired(Packet* p);
  
 protected :
   BasicRequestProcessor* brp_;

    private :
      int actualCacheSize;
    Packet** packetCache;
    RequestCacheTimer** timerCache;
    int getFreeSlot();
    
};

class RequestCacheTimer : public TimerHandler 
{
  public :
    RequestCacheTimer(RequestCache* cache, int number);

  protected :
    void expire(Event* e);

  private :
    RequestCache* cache_;
  int number;
};

class CellcastRequestCache : public RequestCache
{
  public : 
  CellcastRequestCache(int initialCacheSize,
		       BasicRequestProcessor* requestProcessor);
  void timerExpired(Packet* p);
};

class CirclecastRequestCache : public RequestCache
{
  public : 
    CirclecastRequestCache(int initialCacheSize,
			   BasicRequestProcessor* requestProcessor);
  void timerExpired(Packet* p);
};

// returns true if pos1 and pos2 are member of the same cell
extern bool sameCell(position* pos1, position* pos2, 
		     Cellbuilder* cellbuilder);
#endif
