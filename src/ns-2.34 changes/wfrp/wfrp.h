/* 
 * Copyright (c) 2010, Elmurod A. Talipov, Yonsei University
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __wfrp_h__
#define __wfrp_h__

#include <cmu-trace.h>
#include <priqueue.h>
#include <classifier/classifier-port.h>

#define NETWORK_DIAMETER		64
#define DEFAULT_BEACON_INTERVAL		10 // seconds;
#define DEFAULT_ROUTE_EXPIRE 		2*DEFAULT_BEACON_INTERVAL // seconds;
#define ROUTE_PURGE_FREQUENCY		2 // seconds



#define ROUTE_FRESH		0x01
#define ROUTE_EXPIRED		0x02
#define ROUTE_FAILED		0x03

class WFRP;

// ======================================================================
//  Timers : Beacon Timer, Route Cache Timer
// ======================================================================

class wfrpBeaconTimer : public Handler {
public:
        wfrpBeaconTimer(WFRP* a) : agent(a) {}
        void	handle(Event*);
private:
        WFRP    *agent;
	Event	intr;
};

class wfrpRouteCacheTimer : public Handler {
public:
        wfrpRouteCacheTimer(WFRP* a) : agent(a) {}
        void	handle(Event*);
private:
        WFRP    *agent;
	Event	intr;
};

// ======================================================================
//  Route Cache Table
// ======================================================================
class RouteCache {
	friend class WFRP;
 public:
	RouteCache(nsaddr_t bsrc, u_int32_t bid) { rt_dst = bsrc; rt_seqno = bid;  }
 protected:
	LIST_ENTRY(RouteCache) rt_link;
	u_int32_t       rt_seqno;	// route sequence number
	nsaddr_t        rt_dst;		// route destination
       	nsaddr_t	rt_nexthop;	// next hop node towards the destionation
	u_int32_t	rt_xpos;	// x position of destination;
	u_int32_t	rt_ypos;	// y position of destination;
	u_int8_t	rt_state;	// state of the route: FRESH, EXPIRED, FAILED (BROKEN)
	u_int8_t	rt_hopcount;    // number of hops up to the destination (sink)
        double          rt_expire; 	// when route expires : Now + DEFAULT_ROUTE_EXPIRE

};
LIST_HEAD(wfrp_rtcache, RouteCache);


// ======================================================================
//  WFRP Routing Agent : the routing protocol
// ======================================================================

class WFRP : public Agent {
	friend class RouteCacheTimer;

 public:
	WFRP(nsaddr_t id);

	void		recv(Packet *p, Handler *);

        int             command(int, const char *const *);

	// Agent Attributes
	nsaddr_t	index;     // node address (identifier)
	nsaddr_t	seqno;     // beacon sequence number (used only when agent is sink)

	// Node Location
	uint32_t	posx;       // position x;
	uint32_t	posy;       // position y;
		

	// Routing Table Management
	void		rt_insert(nsaddr_t src, u_int32_t id, nsaddr_t nexthop, u_int32_t xpos, u_int32_t ypos, u_int8_t hopcount);
	void		rt_remove(RouteCache *rt);
	void		rt_purge();
	RouteCache*	rt_lookup(nsaddr_t dst);

	// Timers
	wfrpBeaconTimer		bcnTimer;
	wfrpRouteCacheTimer	rtcTimer;
	
	// Caching Head
	wfrp_rtcache	rthead;
	
	// Send Routines
	void		send_beacon();
	void		send_error(nsaddr_t unreachable_destination);
	void		forward(Packet *p, nsaddr_t nexthop, double delay);
	
	// Recv Routines
	void		recv_data(Packet *p);
	void		recv_wfrp(Packet *p);
	void 		recv_beacon(Packet *p);
	void		recv_error(Packet *p);
	
	// Position Management
	void		update_position();


        //  A mechanism for logging the contents of the routing table.
        Trace		*logtarget;

        // A pointer to the network interface queue that sits between the "classifier" and the "link layer"
        PriQueue	*ifqueue;

	// Port classifier for passing packets up to agents
	PortClassifier	*dmux_;

};


#endif /* __wfrp_h__ */
