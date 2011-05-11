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

#ifndef __greedy_h__
#define __greedy_h__

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

class GREEDY;

// ======================================================================
//  Timers : Beacon Timer, Route Cache Timer
// ======================================================================

class greedyBeaconTimer : public Handler {
public:
        greedyBeaconTimer(GREEDY* a) : agent(a) {}
        void	handle(Event*);
private:
        GREEDY    *agent;
	Event	intr;
};

class greedyRouteCacheTimer : public Handler {
public:
        greedyRouteCacheTimer(GREEDY* a) : agent(a) {}
        void	handle(Event*);
private:
        GREEDY    *agent;
	Event	intr;
};

// ======================================================================
//  Route Cache Table
// ======================================================================
class Neighbor {
	friend class GREEDY;
 public:
	Neighbor(nsaddr_t neigh, u_int32_t bid) { nb_neigh = neigh; nb_seqno = bid;  }
 protected:
	LIST_ENTRY(Neighbor) nb_link;
	u_int32_t       nb_seqno;	// neighbor sequence number
       	nsaddr_t	nb_neigh;	// One of its neighbors
	u_int32_t	nb_xpos;	// x position of the neighbor;
	u_int32_t	nb_ypos;	// y position of the neighbor;
	u_int8_t	nb_state;	// state of the link: FRESH, EXPIRED, FAILED (BROKEN)
        double          nb_expire; 	// when route expires : Now + DEFAULT_ROUTE_EXPIRE

};
LIST_HEAD(greedy_nb, Neighbor);


// ======================================================================
//  GREEDY Routing Agent : the routing protocol
// ======================================================================

class GREEDY : public Agent {
	friend class RouteCacheTimer;

 public:
	GREEDY(nsaddr_t id);

	void		recv(Packet *p, Handler *);

        int             command(int, const char *const *);

	// Agent Attributes
	nsaddr_t	index;     // node address (identifier)
	nsaddr_t	seqno;     // beacon sequence number (used only when agent is sink)

	// Node Location
	uint32_t	posx;       // position x;
	uint32_t	posy;       // position y;
		

	// Routing Table Management
	void		nb_insert(nsaddr_t src, u_int32_t id, u_int32_t xpos, u_int32_t ypos);
	void		nb_remove(Neighbor *nb);
	void		nb_purge();
	Neighbor*	nb_lookup(nsaddr_t dst);

	// Timers
	greedyBeaconTimer		bcnTimer;
	greedyRouteCacheTimer	rtcTimer;
	
	// Neighbor Head
	greedy_nb	nbhead;
	
	// Send Routines
	void		send_beacon();
	void		send_error(nsaddr_t unreachable_destination);
	void		forward(Packet *p, nsaddr_t nexthop, double delay);
	
	// Recv Routines
	void		recv_data(Packet *p);
	void		recv_greedy(Packet *p);
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


#endif /* __greedy_h__ */
