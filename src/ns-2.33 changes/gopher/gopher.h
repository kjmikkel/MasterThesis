#ifndef _GOPHER_h
#define _GOPHER_h

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
  $Id: gopher.h,v 1.1 2004/05/10 16:38:26 schenke Exp $
*/
using namespace std;

// GOPHER for ns2 w/wireless extensions

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

// GOPHER Defaults - Karp
#define GOPHER_ALIVE_DESYNC  0.5	/* desynchronizing term for alive beacons */
#define GOPHER_ALIVE_INT     0.5	/* interval between alive beacons */
#define GOPHER_ALIVE_EXP     (3*(GOPHER_ALIVE_INT+GOPHER_ALIVE_DESYNC*GOPHER_ALIVE_INT))
				/* timeout for expiring rx'd beacons */
#define GOPHER_PPROBE_INT    1.5	/* interval between perimeter probes */
#define GOPHER_PPROBE_DESYNC 0.5	/* desynchronizing term for perimeter probes */
#define GOPHER_PPROBE_EXP    8.0	/* how often must use a perimeter to keep probing them */
#define GOPHER_PPROBE_RTX    1

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
#define GOPHER_RBEACON_JITTER   0.015
#define GOPHER_RBEACON_RETRIES  1
#define GOPHER_BEACON_REQ_DELAY bexp_
#define GOPHER_BEACON_DELAY     bexp_

// Packet Types
#define GOPHER_PKT_TYPES    6     /* how many pkt types are defined */

#define GOPHERH_DATA_GREEDY 0	/* gopher mode data packet */
#define GOPHERH_DATA_PERI   1	/* perimeter mode data packet */
#define GOAFR_DATA_ADVANCE  2   /* advance to the node closest to the sink*/
#define GOPHERH_PPROBE      3	/* perimeter probe packet */
#define GOPHERH_BEACON      4     /* liveness beacon packet */
#define GOPHERH_BEACON_REQ  5     /* neighbor request */

#define GOPHER_ROUTE_VERBOSE 1   /* should shortest route be aquired and */


#ifndef CURRTIME
#define CURRTIME Scheduler::instance().clock()
#endif


// opaque type: returned by GOPHERNeighbTable iterator, holds place in table
typedef unsigned int GOPHERNeighbTableIter;


class GOPHER_Agent;
class GOPHERNeighbEnt;
class GOPHERNeighbTable;


/**************/
/* Structures */
/**************/

struct GOPHERSendBufEntry{
    double t;
    Packet *p;
};

struct hdr_gopher {
    
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
  


    enum port_t { GOPHER=0, LOCS=1 };
    int port_;

    int size() { return 0; }
    
    // NS-2 requirements
    static int offset_;
    inline static int& offset() { return offset_; } 
    inline static hdr_gopher* access(const Packet* p) {
	return (hdr_gopher*) p->access(offset_);
    }
  
    void add_hop(nsaddr_t addip, double addx, double addy, double addz) {
	if (nhops_ == MAX_PERI_HOPS_STATIC) {
	    fprintf(stderr, "hdr_gopher::add_hop: out of slots!\n");
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

class GOPHERPacketDelayTimer : public QueuedTimer {
    
 public:
 GOPHERPacketDelayTimer(GOPHER_Agent *a_, int size) : QueuedTimer(size)
	{ a = a_; }
    void handle();
    void deleteInfo(void* info);

 private:
    GOPHER_Agent *a; 
    
};

class GOPHERSendBufferTimer : public TimerHandler {

 public:
    GOPHERSendBufferTimer(GOPHER_Agent *a): TimerHandler() { a_ = a; }
    void expire(Event *e);

 protected:
    GOPHER_Agent *a_;

};

class GOPHER_BeaconTimer : public TimerHandler {

 public:
    GOPHER_BeaconTimer(GOPHER_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GOPHER_Agent *a;
};

class GOPHER_LastPeriTimer : public TimerHandler {

 public:
    GOPHER_LastPeriTimer(GOPHER_Agent *a_) { a = a_; }
    virtual void expire(Event *);
    
 protected:
    GOPHER_Agent *a;
};

class GOPHER_PlanarTimer : public TimerHandler {

 public:
    GOPHER_PlanarTimer(GOPHER_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GOPHER_Agent *a;
};


class GOPHER_DeadNeighbTimer : public TimerHandler {

 public:
    GOPHER_DeadNeighbTimer(GOPHER_Agent *a_, GOPHERNeighbEnt *ne_) 
	{ a = a_; ne = ne_; }
    virtual void expire(Event *);
    
 protected:
    GOPHER_Agent *a;
    GOPHERNeighbEnt *ne;
};

class GOPHER_PeriProbeTimer : public TimerHandler {

 public:
    GOPHER_PeriProbeTimer(GOPHER_Agent *a_, GOPHERNeighbEnt *ne_)
	{ a = a_; ne = ne_; }
    virtual void expire(Event *);
    
 protected:
    GOPHER_Agent *a;
    GOPHERNeighbEnt *ne;
};

class GOPHERBeaconDelayTimer : public TimerHandler {

 public:
    GOPHERBeaconDelayTimer(GOPHER_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GOPHER_Agent *a;
};

class GOPHERBeaconReqDelayTimer : public TimerHandler {

 public:
    GOPHERBeaconReqDelayTimer(GOPHER_Agent *a_) { a = a_; }
    virtual void expire(Event *);

 protected:
    GOPHER_Agent *a;
};

/******************/
/* Neighbor Entry */
/******************/

class GOPHERNeighbEnt {

 public:
     GOPHERNeighbEnt(GOPHER_Agent *ina) :
	 peri(NULL), perilen(0), maxlen(0), dnt(ina, this), ppt(ina, this)
       {
       };
	
     void planarize(class GOPHERNeighbTable *, int, double, double, double); /** [HMF] Screen this edge */

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
     GOPHER_DeadNeighbTimer dnt; //**< [HMF] timer for expiration of neighbor
     GOPHER_PeriProbeTimer ppt;  //**< [HMF] Timer for generation of perimeter probe to neighbor
};

/******************/
/* Neighbor Table */
/******************/
/** Array that is ordered by destination addr and holds the GOPHERNeighbEnts */

class GOPHERNeighbTable {

 public:
     GOPHERNeighbTable(GOPHER_Agent *mya);
     ~GOPHERNeighbTable();
    
     void ent_delete(const GOPHERNeighbEnt *ent);          //** Delete an entry
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
	    
     GOPHERNeighbEnt *ent_add(const GOPHERNeighbEnt *ent); //** [HMF] Add an entry
     GOPHERNeighbEnt *ent_finddst(nsaddr_t dst);           //** [HMF] Find an entry by his destination address

     // Neighbor Functions
     class GOPHERNeighbEnt *ent_findshortest       //** Find Closest
	 (MobileNode *mn, double x, double y, double z);
     class GOPHERNeighbEnt *ent_findshortest_cc    //** Find Closest with congestion control 
	 (MobileNode *mn, double x, double y, double z, double alpha);
     class GOPHERNeighbEnt *ent_findshortestXcptLH //** Find Closest that is not the LastHop
	 (MobileNode *mn, nsaddr_t lastHopId, double x, double y, double z);

     //** [HMF] Iterating through every table on peri and return the first
     //*  hop on the perimeter that is close to the destination than
     //*  itself 
     class GOPHERNeighbEnt *ent_findcloser_onperi
	 (MobileNode *mn, double x, double y, double z, int *perihop);
     class GOPHERNeighbEnt *ent_findcloser_edgept
	 (MobileNode *, double, double, nsaddr_t, nsaddr_t, double, double, double *, double *);
     class GOPHERNeighbEnt *ent_next_ccw(MobileNode *, GOPHERNeighbEnt *, int);
     class GOPHERNeighbEnt *ent_next_ccw(double, double, double, int, GOPHERNeighbEnt * = 0);
     class GOPHERNeighbEnt *ent_findface(MobileNode *, double, double, double, int);
     GOPHERNeighbTableIter InitLoop(); 
     class GOPHERNeighbEnt *NextLoop(GOPHERNeighbTableIter *);
     class GOPHERNeighbEnt *ent_findnext_onperi(MobileNode *, int, double, double, double, int);
     class GOPHERNeighbEnt *ent_findnextcloser_onperi(MobileNode *mn, double dx, double dy, double dz);
     int meanLoad();         //**< calculates the mean load of all neighbors in this table
     inline int noEntries() {return nents;}
     
     /**set for planarization, entries are valid edges*/
     bool counter_clock;
     
     DHeapEntry *val_item;
     DHeap *valid;
     int itedge;
 protected:
  friend class GOPHERNeighbEnt;

 private:

     int nents;		     //** Entries currently in use
     int maxents;	     
     GOPHER_Agent *a;    
     GOPHERNeighbEnt **tab;
};

/**************/
/* GOPHER Agent */
/**************/

class GOPHER_Agent : public Tap, public Agent {

 public:
    GOPHER_Agent(void);
    
    // Timer called Functions
    void beacon_callback(void);	                   // generate a beacon (timer-triggered)
    void deadneighb_callback(class GOPHERNeighbEnt *ne); // neighbor gone (timer/MAC-trig)
    void periprobe_callback(class GOPHERNeighbEnt *ne);  // gen perimeter probe (timer-trig)
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

    // GOAFR
    int check_ellipse(Packet *p, int current_address, int to_address); // Check whether the next point is inside the ellipsis 
    // End GOAFR

    bool send_allowed[GOPHER_PKT_TYPES]; //**< array with permission value to send pkttype

    int off_gopher_;		 //**< offset of the GOPHER packet header in pkt 
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

    friend class GOPHERNeighbEnt;
    class MobileNode *mn_;	        //**< MobileNode 
    class PriQueue *ifq_;	        //**< InterfaceQueue [MK]
    class Mac *m;                       //**< MAC
    class Trace *tracetarget;		//**< Trace Target
    class LocationService *locservice_; //**< LocationService [MK]
    class GOPHERNeighbTable *ntab_;           //**< Neighbor Table

    class GOPHER_BeaconTimer   *beacon_timer_;   //**< Alive Beacon Timer
    class GOPHER_LastPeriTimer *lastperi_timer_; //**< Last Perimeter Used Timer
    class GOPHER_PlanarTimer   *planar_timer_;   //**< Inter-Planarization Timer
    class GOPHERPacketDelayTimer   *pd_timer;        //**< Packet Delay Timer [MK]

    GOPHERSendBufferTimer send_buf_timer;
    GOPHERSendBufEntry send_buf[SEND_BUF_SIZE];
    
    // Reactive Beaconing
    class GOPHERBeaconDelayTimer    *beacon_delay_;    //**< Min Delay between two Beacons
    class GOPHERBeaconReqDelayTimer *beaconreq_delay_; //**< Min Delay between two Beacon Reqs

    friend class GOPHERPacketDelayTimer;
    friend class GOPHERSendBufferTimer;

    friend class GOPHERNeighbTable;

    virtual void recv(Packet *, Handler *);
    void trace(char *fmt, ...);
    void tracepkt(Packet *, double, int, const char *);
    void init();

    void forwardPacket(Packet *, int = 0);      //**< Forwarding Packets (Way too big for one function :( )
    void periIn(Packet *, hdr_gopher *, int = 0);
    int hdr_size(Packet* p);                    //**< [MK] Handles everything size related  
    int crosses(class GOPHERNeighbEnt *, hdr_gopher *);
    int getLoad();				//**< [MT] recalculate my own load (using neighbors')
    
    // Beaconing Functions
    void beacon_proc(int, double, double, double, int = -1);
    void recvBeacon(Packet*);                   //**< [MK] receive and evaluate Beacon
    void recvBeaconReq(Packet*);                //**< [MK] receive Beacon Request
    void sendBeacon(double = 0.0);        //**< [MK] send Beacon
    void sendBeaconRequest();                   //**< [MK] send Beacon request
    void checkGopherCondition(const Packet*);   //**< [MK] check if node is a Gopher Neighbor to src

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

#endif //_GOPHER_h
