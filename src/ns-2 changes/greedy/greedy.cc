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

#include <greedy/greedy.h>
#include <greedy/greedy_packet.h>
#include <random.h>
#include <cmu-trace.h>
#include <energy-model.h>

#define max(a,b)        ( (a) > (b) ? (a) : (b) )
#define CURRENT_TIME    Scheduler::instance().clock()

#define DEBUG

// ======================================================================
//  TCL Hooking Classes
// ======================================================================

int hdr_wfrp::offset_;
static class GREEDYHeaderClass : public PacketHeaderClass {
 public:
	GREEDYHeaderClass() : PacketHeaderClass("PacketHeader/GREEDY", sizeof(hdr_all_greedy)) {
	  bind_offset(&hdr_greedy::offset_);
	} 
} class_rtProtoGREEDY_hdr;

static class GREEDYclass : public TclClass {
 public:
	GREEDYclass() : TclClass("Agent/GREEDY") {}
	TclObject* create(int argc, const char*const* argv) {
		assert(argc == 5);
		return (new GREEDY((nsaddr_t) Address::instance().str2addr(argv[4])));
	}
} class_rtProtoGREEDY;


int
GREEDY::command(int argc, const char*const* argv) {
	if(argc == 2) {
	Tcl& tcl = Tcl::instance();
    
		if(strncasecmp(argv[1], "id", 2) == 0) {
			tcl.resultf("%d", index);
			return TCL_OK;
		}
 
		if(strncasecmp(argv[1], "start", 5) == 0) {
			rtcTimer.handle((Event*) 0);
			return TCL_OK;
		}

		// Start Beacon Timer (which sends beacon message)
		if(strncasecmp(argv[1], "sink", 4) == 0) {
			bcnTimer.handle((Event*) 0);
#ifdef DEBUG
		printf("N (%.6f): sink node is set to %d, start beaconing  \n", CURRENT_TIME, index);
#endif 
			return TCL_OK;
		}
	}
	else if(argc == 3) {
		if(strcmp(argv[1], "index") == 0) {
			index = atoi(argv[2]);
			return TCL_OK;
		}

		else if(strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
			logtarget = (Trace*) TclObject::lookup(argv[2]);
			if(logtarget == 0)
				return TCL_ERROR;
      			return TCL_OK;
		}
		
		else if(strcmp(argv[1], "drop-target") == 0) {
			/* int stat = rqueue.command(argc,argv);
			if (stat != TCL_OK)
				return stat;
			return Agent::command(argc, argv);*/
			return TCL_OK;
		}

		else if(strcmp(argv[1], "if-queue") == 0) {
			ifqueue = (PriQueue*) TclObject::lookup(argv[2]);
      
			if(ifqueue == 0)
				return TCL_ERROR;
			return TCL_OK;
		}

		else if (strcmp(argv[1], "port-dmux") == 0) {
			dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
			if (dmux_ == 0) {
				fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__,
				argv[1], argv[2]);
				return TCL_ERROR;
			}
			return TCL_OK;
		}
	}
	
	return Agent::command(argc, argv);
}

// ======================================================================
//  Agent Constructor
// ======================================================================

GREEDY::GREEDY(nsaddr_t id) : Agent(PT_GREEDY), bcnTimer(this), rtcTimer(this) {
             

#ifdef DEBUG
	printf("N (%.6f): Routing agent is initialized for node %d \n", CURRENT_TIME, id);
#endif 
	index = id;
	seqno = 1;

	LIST_INIT(&rthead);
	posx = 0;
	posy = 0;

	logtarget = 0;
	ifqueue = 0;

}

// ======================================================================
//  Send Beacon Routine
// ======================================================================
void
GREEDY::send_beacon() {
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_greedy_beacon *bcn = HDR_GREEDY_BEACON(p);

	// Write Channel Header
	ch->ptype() = PT_GREEDY;
	ch->size() = IP_HDR_LEN + bcn->size();
	ch->addr_type() = NS_AF_NONE;
	ch->prev_hop_ = index;

	// Write IP Header
	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = NETWORK_DIAMETER;

	// Write Beacon Header
	bcn->pkt_type = GREEDY_BEACON;
	bcn->beacon_hops = 1;
	bcn->beacon_id = seqno;
	bcn->beacon_src = index;
	
	// update the node position before putting it in the packet
	update_position();

	bcn->beacon_posx = posx;
	bcn->beacon_posy = posy;

	bcn->timestamp = CURRENT_TIME;

	// increase sequence number for next beacon
	seqno += 1;

#ifdef DEBUG
	printf("S (%.6f): send beacon by %d  \n", CURRENT_TIME, index);
#endif 
	Scheduler::instance().schedule(target_, p, 0.0);

}

// ======================================================================
//  Send Error Routine
// ======================================================================
void 
GREEDY::send_error(nsaddr_t unreachable_destination) {
	// TODO : code should be update;
}



// ======================================================================
//  Forward Routine
// ======================================================================

void 
GREEDY::forward(Packet *p, nsaddr_t nexthop, double delay) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);

	if (ih->ttl_ == 0) {
		drop(p, DROP_RTR_TTL);
	}
	
	if (nexthop != (nsaddr_t) IP_BROADCAST) {
		ch->next_hop_ = nexthop;
		ch->prev_hop_ = index;
		ch->addr_type() = NS_AF_INET;
		ch->direction() = hdr_cmn::DOWN;
	}
	else {
		assert(ih->daddr() == (nsaddr_t) IP_BROADCAST);
		ch->prev_hop_ = index;
		ch->addr_type() = NS_AF_NONE;
		ch->direction() = hdr_cmn::DOWN; 
	}
	
	Scheduler::instance().schedule(target_, p, delay);

}


// ======================================================================
//  Recv Packet
// ======================================================================

void
GREEDY::recv(Packet *p, Handler*) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);

	// if the packet is routing protocol control packet, give the packet to agent
	if(ch->ptype() == PT_GREEDY) {
		ih->ttl_ -= 1;
		recv_greedy(p);
		return;
	}

	//  Must be a packet I'm originating
	if((ih->saddr() == index) && (ch->num_forwards() == 0)) {
 	
		// Add the IP Header. TCP adds the IP header too, so to avoid setting it twice, 
		// we check if  this packet is not a TCP or ACK segment.

		if (ch->ptype() != PT_TCP && ch->ptype() != PT_ACK) {
			ch->size() += IP_HDR_LEN;
		}

	}

	// I received a packet that I sent.  Probably routing loop.
	else if(ih->saddr() == index) {
   		drop(p, DROP_RTR_ROUTE_LOOP);
		return;
	}

	//  Packet I'm forwarding...
	else {
		if(--ih->ttl_ == 0) {
			drop(p, DROP_RTR_TTL);
			return;
   		}
	}

	// This is data packet, find route and forward packet
	recv_data(p);
}


// ======================================================================
//  Recv Data Packet
// ======================================================================

void 
GREEDY::recv_data(Packet *p) {
	struct hdr_ip *ih = HDR_IP(p);
	RouteCache *rt;
	
	// if route fails at link layer, (link layer could not find next hop node) it will cal rt_failed_callback function
	//ch->xmit_failure_ = rt_failed_callback;
	//ch->xmit_failure_data_ = (void*) this;

#ifdef DEBUG
	printf("R (%.6f): recv data by %d  \n", CURRENT_TIME, index);
#endif 

	rt = rt_lookup(ih->daddr());

	// There is no route for the destination
	if (rt == NULL) {
	// TODO: queue the packet and wait for the route construction
		return ;
	}

	// if the route is not failed forward it;
	else if (rt->rt_state != ROUTE_FAILED) {
		forward(p, rt->rt_nexthop, 0.0);
	}
	
	// if the route has failed, wait to be updated;
	else {
		//TODO: queue the packet and wait for the route construction;
		return;
	}

}

// ======================================================================
//  Recv GREEDY Packet
// ======================================================================
void
GREEDY::recv_greedy(Packet *p) {
	struct hdr_greedy *wh = HDR_GREEDY(p);

	assert(HDR_IP (p)->sport() == RT_PORT);
	assert(HDR_IP (p)->dport() == RT_PORT);

	// What kind of packet is this
	switch(wh->pkt_type) {

		case GREEDY_BEACON:
			recv_beacon(p);
			break;

		case GREEDY_ERROR:
			recv_error(p);
			break;

		default:
			fprintf(stderr, "Invalid packet type (%x)\n", wh->pkt_type);
			exit(1);
	}
}


// ======================================================================
//  Recv Beacon Packet
// ======================================================================
void 
GREEDY::recv_beacon(Packet *p) {
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_greedy_beacon *bcn = HDR_GREEDY_BEACON(p);
	
	// I have originated the packet, just drop it
	if (bcn->beacon_src == index)  {
		Packet::free(p);
		return;
	}

#ifdef DEBUG
	printf("R (%.6f): recv beacon by %d, src:%d, seqno:%d, hop: %d \n", 
		CURRENT_TIME, index, bcn->beacon_src, bcn->beacon_id, bcn->beacon_hops);
#endif 
	
	// search for a route 
	RouteCache	*rt = rt_lookup(bcn->beacon_src);
	
	// if there is no route toward this destination, insert the route and forward
 	if (rt == NULL)  {
		rt_insert(bcn->beacon_src,bcn->beacon_id, ih->saddr(), bcn->beacon_posx, bcn->beacon_posy, bcn->beacon_hops);

		ih->saddr() = index;		
		bcn->beacon_hops +=1; // increase hop count

		double delay = 0.1 + Random::uniform();

#ifdef DEBUG
	printf("F (%.6f): NEW ROUTE, forward beacon by %d \n", CURRENT_TIME, index);
#endif 

		forward(p, IP_BROADCAST, delay);
	}
	// if the route is newer than I have (i.e. new beacon is received): update the route and forward
	else if (bcn->beacon_id > rt->rt_seqno) {
	
		rt->rt_seqno = bcn->beacon_id;
		rt->rt_nexthop = ih->saddr();
		rt->rt_xpos = bcn->beacon_posx;
		rt->rt_ypos = bcn->beacon_posy;
		rt->rt_state = ROUTE_FRESH;
		rt->rt_hopcount = bcn->beacon_hops;
		rt->rt_expire = CURRENT_TIME + DEFAULT_ROUTE_EXPIRE;
		
		ih->saddr() = index;
		bcn->beacon_hops +=1; // increase hop count

		double delay = 0.1 + Random::uniform();

#ifdef DEBUG
		printf("F (%.6f): UPDATE ROUTE, forward beacon by %d \n", CURRENT_TIME, index);
#endif 
		forward(p, IP_BROADCAST, delay);
	}
	// if the route is shorter than I have, update it
	else if ((bcn->beacon_id == rt->rt_seqno) && (bcn->beacon_hops < rt->rt_hopcount )) {

		rt->rt_seqno = bcn->beacon_id;
		rt->rt_nexthop = ih->saddr();
		rt->rt_xpos = bcn->beacon_posx;
		rt->rt_ypos = bcn->beacon_posy;
		rt->rt_state = ROUTE_FRESH;
		rt->rt_hopcount = bcn->beacon_hops;
		rt->rt_expire = CURRENT_TIME + DEFAULT_ROUTE_EXPIRE;
	}

	// TODO : initiate dequeue() routine to send queued packets;

}


// ======================================================================
//  Recv Error Packet
// ======================================================================

void
GREEDY::recv_error(Packet *p) {
	// TODO: code should be update;
}


// ======================================================================
//  Routing Management
// ======================================================================

/* static void
GREEDY::rt_failed_callback(Packet *p, void *arg) {

}*/

void
GREEDY::rt_insert(nsaddr_t src, u_int32_t id, nsaddr_t nexthop, u_int32_t xpos, u_int32_t ypos, u_int8_t hopcount) {
	RouteCache	*rt = new RouteCache(src, id);

	rt->rt_nexthop = nexthop;
	rt->rt_xpos = xpos;
	rt->rt_ypos = ypos;
	rt->rt_state = ROUTE_FRESH;
	rt->rt_hopcount = hopcount;
	rt->rt_expire = CURRENT_TIME + DEFAULT_ROUTE_EXPIRE;

	LIST_INSERT_HEAD(&rthead, rt, rt_link);
}



RouteCache*	
GREEDY::rt_lookup(nsaddr_t dst) {
	RouteCache *r = rthead.lh_first;

  	for( ; r; r = r->rt_link.le_next) {
		if (r->rt_dst == dst)
			return r;
 	}
	
	return NULL;
}

void
GREEDY::rt_purge() {
	RouteCache *rt= rthead.lh_first;
	double now = CURRENT_TIME;

	for(; rt; rt = rt->rt_link.le_next) {
		if(rt->rt_expire <= now)
			rt->rt_state = ROUTE_EXPIRED;
 	}
}

void
GREEDY::rt_remove(RouteCache *rt) {
	LIST_REMOVE(rt,rt_link);
}


void 
GREEDY::update_position() {
	//TODO: we have to update node position
}
