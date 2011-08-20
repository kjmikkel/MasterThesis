#ifndef _GREEDY_h
#define _GREEDY_h

/*

  Copyright (C) 2000 President and Fellows of Harvard College

  All rights reserved.

  NOTICE: This software is provided "as is", without any warranty,
  including any implied warranty for merchantability or fitness for a
  particular purpose.  Under no circumstances shall Harvard University
  or its faculty, staff, students or agents be liable for any use of,
  misuse of, or inability to use this software, including incidental
  and consequential damages.

  License is hereby given to use, modify, and redistribute this
  software, in whole or in part, for any commercial or non-commercial
  purpose, provided that the user agrees to the terms of this
  copyright notice, including disclaimer of warranty, and provided
  that this copyright notice, including disclaimer of warranty, is
  preserved in the source code and documentation of anything derived
  from this software.  Any redistributor of this software or anything
  derived from this software assumes responsibility for ensuring that
  any parties to whom such a redistribution is made are fully aware of
  the terms of this license and disclaimer.

  Author: Brad Karp, Harvard University EECS, May, 1999
  $Id: greedy.h,v 1.1 2004/05/10 16:38:26 schenke Exp $
*/
using namespace std;

// GREEDY for ns2 w/wireless extensions

#include "agent.h"
#include "ip.h"
#include "delay.h"
#include "scheduler.h"
#include "queue.h"
#include "trace.h"
#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "mac-802_11.h"
#include "priqueue.h"
#include "node.h"
#include "timer-handler.h"
#include <random.h>
#include "god.h"
#include "../commen_routing.h"

#include "../locservices/ls_queued_timer.h"


/***********/
/* Defines */
/***********/

// Location Services
#define _OMNI_     0
#define _REACTIVE_ 1 
#define _GRID_     2 
#define _CELL_     3

// GREEDY Defaults - Karp
#define GREEDY_ALIVE_DESYNC  0.5	/* desynchronizing term for alive beacons */
#define GREEDY_ALIVE_INT     0.5	/* interval between alive beacons */
#define GREEDY_ALIVE_EXP     (3*(GREEDY_ALIVE_INT+GREEDY_ALIVE_DESYNC*GREEDY_ALIVE_INT))
				/* timeout for expiring rx'd beacons */
#define GREEDY_PPROBE_INT    1.5	/* interval between perimeter probes */
#define GREEDY_PPROBE_DESYNC 0.5	/* desynchronizing term for perimeter probes */
#define GREEDY_PPROBE_EXP    8.0	/* how often must use a perimeter to keep probing them */
#define GREEDY_PPROBE_RTX    1

#define PERI_DEFAULT_HOPS 32	/* default max number of hops in peri header */
#define MAX_PERI_HOPS_STATIC 128
#define PLANARIZE_RNG 0
#define PLANARIZE_GABRIEL 1

// SendBuffer
#define BUFFER_CHECK    0.05
#define SEND_BUF_SIZE   64   /* As in DSR */
#define SEND_TIMEOUT    30   /* As in DSR */
#define JITTER_VAR      0.3

// Reactive Beaconing
#define GREEDY_RBEACON_JITTER   0.015
#define GREEDY_RBEACON_RETRIES  1
#define GREEDY_BEACON_REQ_DELAY bexp_
#define GREEDY_BEACON_DELAY     bexp_

// Packet Types
#define GREEDY_PKT_TYPES    6     /* how many pkt types are defined */

#define GREEDYH_DATA_GREEDY 0	/* greedy mode data packet */
#define GREEDYH_DATA_PERI   1	/* perimeter mode data packet */
#define GREEDYH_PPROBE      2	/* perimeter probe packet */
#define GREEDYH_BEACON      3     /* liveness beacon packet */
#define GREEDYH_BEACON_REQ  4     /* neighbor request */

#define GREEDY_ROUTE_VERBOSE 1   /* should shortest route be aquired and */


#ifndef CURRTIME
#define CURRTIME Scheduler::instance().clock()
#endif


// opaque type: returned by GREEDYNeighbTable iterator, holds place in table
typedef unsigned int GREEDYNeighbTableIter;


class GREEDY_Agent;
class GREEDYNeighbEnt;
class GREEDYNeighbTable;


/**************/
/* Structures */
/**************/

struct GREEDYSendBufEntry{
    double t;
    Packet *p;
};

struct hdr_greedy {
    
    struct PeriEnt hops_[MAX_PERI_HOPS_STATIC];
    struct PeriEnt peript_; // starting point
    struct PeriEnt perips_; // intersection point
    nsaddr_t periptip_[3];
    int nhops_;
    int currhop_;
    int mode_;

    // Additions
    int load;
    int retry;

    // for geo-anycast - wk
     bool geoanycast;
  


    enum port_t { GREEDY=0, LOCS=1 };
    int port_;

    int size() { return 0; }
    
    // NS-2 requirements
    static int offset_;
    inline static int& offset() { return offset_; } 
    inline static hdr_greedy* access(const Packet* p) {
	return (hdr_greedy*) p->access(offset_);
    }
  
    void add_hop(nsaddr_t addip, double addx, double addy, double addz) {
	if (nhops_ == MAX_PERI_HOPS_STATIC) {
	    fprintf(stderr, "hdr_greedy::add_hop: out of slots!\n");
	    abort();
	}
	hops_[nhops_].x = addx; hops_[nhops_].y = addy; hops_[nhops_].z = addz;
	hops_[nhops_].ip = addip;
	nhops_++;
    }
};  


/*****************/
/* Timer Classes */
/*****************/

class GREEDYPacketDelayTimer : public QueuedTimer {
    
 public:
 GREEDYPacketDelayTimer(GREEDY_Agent *a_, int size) : QueuedTimer(size)
	{ a = a_; }
    void handle();
    void deleteInfo(void* info);

 private:
    GREEDY_Agent *a; 
    
};

class GREEDYSendBufferTimer : public TimerHandler {

 public:
    GREEDYSendBufferTimer(GREEDY_Agent *a): TimerHandler() { a_ = a; }
    void expire(Event *e);

 protected:
    GREEDY_Agent *a_;

};

class GREEDY_BeaconTimer : public TimerHandler {

 public:
    GREEDY_BeaconTimer(GREEDY_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GREEDY_Agent *a;
};

class GREEDY_LastPeriTimer : public TimerHandler {

 public:
    GREEDY_LastPeriTimer(GREEDY_Agent *a_) { a = a_; }
    virtual void expire(Event *);
    
 protected:
    GREEDY_Agent *a;
};

class GREEDY_PlanarTimer : public TimerHandler {

 public:
    GREEDY_PlanarTimer(GREEDY_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GREEDY_Agent *a;
};


class GREEDY_DeadNeighbTimer : public TimerHandler {

 public:
    GREEDY_DeadNeighbTimer(GREEDY_Agent *a_, GREEDYNeighbEnt *ne_) 
	{ a = a_; ne = ne_; }
    virtual void expire(Event *);
    
 protected:
    GREEDY_Agent *a;
    GREEDYNeighbEnt *ne;
};

class GREEDY_PeriProbeTimer : public TimerHandler {

 public:
    GREEDY_PeriProbeTimer(GREEDY_Agent *a_, GREEDYNeighbEnt *ne_)
	{ a = a_; ne = ne_; }
    virtual void expire(Event *);
    
 protected:
    GREEDY_Agent *a;
    GREEDYNeighbEnt *ne;
};

class GREEDYBeaconDelayTimer : public TimerHandler {

 public:
    GREEDYBeaconDelayTimer(GREEDY_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GREEDY_Agent *a;
};

class GREEDYBeaconReqDelayTimer : public TimerHandler {

 public:
    GREEDYBeaconReqDelayTimer(GREEDY_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GREEDY_Agent *a;
};

/******************/
/* Neighbor Entry */
/******************/

class GREEDYNeighbEnt {

 public:
     GREEDYNeighbEnt(GREEDY_Agent *ina) :
	 peri(NULL), perilen(0), maxlen(0), dnt(ina, this), ppt(ina, this)
       {
       };
	
     void planarize(class GREEDYNeighbTable *, int, double, double, double); /** [HMF] Screen this edge */

     int closer_pt(nsaddr_t myip, double myx, double myy, double myz,
		   double ptx, double pty, nsaddr_t ptipa, nsaddr_t ptipb,
		   double dstx, double dsty, double *closerx, double *closery);
     
     
     nsaddr_t dst;	   //**< [HMF] IP of neighbor
     double x, y, z;       //**< [HMF] location of neighbor last heard
     double ts;            //**< [HMF] timestamp of location information
     struct PeriEnt *peri; //**< [HMF] Perimeter via this neighbor
     int perilen;	   //**< [HMF] length of perimeter
     int maxlen;	   //**< [HMF] allocated slots in peri
     int live;	           //**< [HMF] when planarizing, whether edge should be used
     int load;	           //**< [MT] Load on MAC layer (802.11) at this neighbor (= 0..100)
     GREEDY_DeadNeighbTimer dnt; //**< [HMF] timer for expiration of neighbor
     GREEDY_PeriProbeTimer ppt;  //**< [HMF] Timer for generation of perimeter probe to neighbor
};

/******************/
/* Neighbor Table */
/******************/
/** Array that is ordered by destination addr and holds the GREEDYNeighbEnts */

class GREEDYNeighbTable {

 public:
     GREEDYNeighbTable(GREEDY_Agent *mya);
     ~GREEDYNeighbTable();
    
     void ent_delete(const GREEDYNeighbEnt *ent);          //** Delete an entry
     void planarize(int, int, double, double, double);          //** Remove all crossing edges

     inline double norm(double tmp_bear){
       double to_norm = tmp_bear;
       while(to_norm <= 0)
	 to_norm += 2*M_PI;
       return to_norm;
     }

     inline double norm_rev(double tmp_bear){
       double to_norm = tmp_bear;
       while(to_norm >= 0)
	 to_norm -= 2*M_PI;
       return to_norm;
     }
	    
     GREEDYNeighbEnt *ent_add(const GREEDYNeighbEnt *ent); //** [HMF] Add an entry
     GREEDYNeighbEnt *ent_finddst(nsaddr_t dst);           //** [HMF] Find an entry by his destination address

     // Neighbor Functions
     class GREEDYNeighbEnt *ent_findshortest       //** Find Closest
	 (MobileNode *mn, double x, double y, double z);
     class GREEDYNeighbEnt *ent_findshortest_cc    //** Find Closest with congestion control 
	 (MobileNode *mn, double x, double y, double z, double alpha);
     class GREEDYNeighbEnt *ent_findshortestXcptLH //** Find Closest that is not the LastHop
	 (MobileNode *mn, nsaddr_t lastHopId, double x, double y, double z);

     //** [HMF] Iterating through every table on peri and return the first
     //*  hop on the perimeter that is close to the destination than
     //*  itself 
     class GREEDYNeighbEnt *ent_findcloser_onperi
	 (MobileNode *mn, double x, double y, double z, int *perihop);
     class GREEDYNeighbEnt *ent_findcloser_edgept
	 (MobileNode *, double, double, nsaddr_t, nsaddr_t, double, double, double *, double *);
     class GREEDYNeighbEnt *ent_next_ccw(MobileNode *, GREEDYNeighbEnt *, int);
     class GREEDYNeighbEnt *ent_next_ccw(double, double, double, int, GREEDYNeighbEnt * = 0);
     class GREEDYNeighbEnt *ent_findface(MobileNode *, double, double, double, int);
     GREEDYNeighbTableIter InitLoop(); 
     class GREEDYNeighbEnt *NextLoop(GREEDYNeighbTableIter *);
     class GREEDYNeighbEnt *ent_findnext_onperi(MobileNode *, int, double, double, double, int);
     class GREEDYNeighbEnt *ent_findnextcloser_onperi(MobileNode *mn, double dx, double dy, double dz);
     int meanLoad();         //**< calculates the mean load of all neighbors in this table
     inline int noEntries() {return nents;}
     
     /**set for planarization, entries are valid edges*/
     bool counter_clock;
     
     DHeapEntry *val_item;
     DHeap *valid;
     int itedge;
 protected:
  friend class GREEDYNeighbEnt;

 private:

     int nents;		     //** Entries currently in use
     int maxents;	     
     GREEDY_Agent *a;    
     GREEDYNeighbEnt **tab;
};

/**************/
/* GREEDY Agent */
/**************/

class GREEDY_Agent : public Tap, public Agent {

 public:
    GREEDY_Agent(void);
    
    // Timer called Functions
    void beacon_callback(void);	                   // generate a beacon (timer-triggered)
    void deadneighb_callback(class GREEDYNeighbEnt *ne); // neighbor gone (timer/MAC-trig)
    void periprobe_callback(class GREEDYNeighbEnt *ne);  // gen perimeter probe (timer-trig)
    void lastperi_callback(void);	           // turn off peri probes when unused for timeout
    void planar_callback(void);	                   // planarization callback
#ifdef SPAN
    void span_callback(void);                      // SPAN callback
#endif

    virtual int command(int argc, const char * const * argv);
    void lost_link(Packet *p);
    void tap(const Packet *p);

    // Additions
    const inline int isActive() { return(active_); } 
    void sleep();  /** [HMF] This functions lays the Agent to rest */
    void wake();   /** [HMF] This functions wakes the Agent up and reinits its times  */

    void allow(unsigned int pktType) { send_allowed[pktType] = true; }
    void block(unsigned int pktType) { send_allowed[pktType] = false; }
    bool allowedToSend(unsigned int pktType) { return (send_allowed[pktType]); }

 protected:

    bool send_allowed[GREEDY_PKT_TYPES]; //**< array with permission value to send pkttype

    int off_greedy_;		 //**< offset of the GREEDY packet header in pkt 
    int use_mac_;		 //**< whether or not to simulate full MAC level 
    int use_peri_;		 //**< whether or not to use perimeters 
    int verbose_;		 //**< verbosity (binary) 
    int active_;                 //**< specifies if node is active [HMF] 
    int drop_debug_;		 //**< whether or not to be verbose on NRTE events 
    int peri_proact_;		 //**< whether or not to pro-actively send pprobes 
    int use_implicit_beacon_;	 //**< whether or not all data packetsare beacons 
    int use_planar_;		 //**< whether or not to planarize graph 
    int use_loop_detect_;	 //**< whether or not to fix loops in peridata pkts 
    int use_timed_plnrz_;	 //**< whether or not to replanarize w/timer 
    int use_beacon_;             //**< whether or not to do beacons at all [MK]
    int use_congestion_control_; //**< whether or not to ship load information with beacons [MT] 
    int use_reactive_beacon_;    //**< whether or not to use reactive beaconing [MK]
    int locservice_type_;        //**< which Location Service should be used [MK]
    double bint_;		 //**< beacon interval 
    double bdesync_;		 //**< beacon desync random component range 
    double bexp_;		 //**< beacon expiration interval 
    double pint_;		 //**< perimeter probe interval 
    double pdesync_;		 //**< perimeter probe desync random cpt. range 
    double lpexp_;		 //**< perimeter probe generation timeout
    double cc_alpha_;		 //**< parameter for congestion control [MT]
    int use_span_;               //**< whether or not to use span services [CL]

    friend class GREEDYNeighbEnt;
    class MobileNode *mn_;	        //**< MobileNode 
    class PriQueue *ifq_;	        //**< InterfaceQueue [MK]
    class Mac *m;                       //**< MAC
    class Trace *tracetarget;		//**< Trace Target
    class LocationService *locservice_; //**< LocationService [MK]
    class GREEDYNeighbTable *ntab_;           //**< Neighbor Table

    class GREEDY_BeaconTimer   *beacon_timer_;   //**< Alive Beacon Timer
    class GREEDY_LastPeriTimer *lastperi_timer_; //**< Last Perimeter Used Timer
    class GREEDY_PlanarTimer   *planar_timer_;   //**< Inter-Planarization Timer
    class GREEDYPacketDelayTimer   *pd_timer;        //**< Packet Delay Timer [MK]

    GREEDYSendBufferTimer send_buf_timer;
    GREEDYSendBufEntry send_buf[SEND_BUF_SIZE];
    
    // Reactive Beaconing
    class GREEDYBeaconDelayTimer    *beacon_delay_;    //**< Min Delay between two Beacons
    class GREEDYBeaconReqDelayTimer *beaconreq_delay_; //**< Min Delay between two Beacon Reqs

    friend class GREEDYPacketDelayTimer;
    friend class GREEDYSendBufferTimer;

    friend class GREEDYNeighbTable;

    virtual void recv(Packet *, Handler *);
    void trace(char *fmt, ...);
    void tracepkt(Packet *, double, int, const char *);
    void init();

    void forwardPacket(Packet *, int = 0);      //**< Forwarding Packets (Way too big for one function :( )
    void periIn(Packet *, hdr_greedy *, int = 0);
    int hdr_size(Packet* p);                    //**< [MK] Handles everything size related  
    int crosses(class GREEDYNeighbEnt *, hdr_greedy *);
    int getLoad();				//**< [MT] recalculate my own load (using neighbors')
    
    // Beaconing Functions
    void beacon_proc(int, double, double, double, int = -1);
    void recvBeacon(Packet*);                   //**< [MK] receive and evaluate Beacon
    void recvBeaconReq(Packet*);                //**< [MK] receive Beacon Request
    void sendBeacon(double = 0.0);        //**< [MK] send Beacon
    void sendBeaconRequest();                   //**< [MK] send Beacon request
    void checkGreedyCondition(const Packet*);   //**< [MK] check if node is a Greedy Neighbor to src

    // SendBuffer Functions
    void notifyPos(nsaddr_t);
    void stickPacketInSendBuffer(Packet *p);
    void dropSendBuff(Packet *& p, const char*);
    void sendBufferCheck();
    void Terminate();
    double sendbuf_interval() {
      return BUFFER_CHECK;
    }

};

#endif //_GREEDY_h
