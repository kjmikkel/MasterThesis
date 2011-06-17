/*

  Contacts: 
  Michael Kaesemann (mikael@uni-mannheim.de)

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms is hereby granted, provided 
  that the above copyright notice appears in all copies and the author is 
  acknowledged in all documentation pertaining to any such copy or
  derivative work.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

#ifndef _GridLocService_cc
#define _GridLocService_cc

#include "gridlocservice.h"
#include "../gpsr/gpsr.h"
#include "../greedy/greedy.h"
#include <math.h>
#include <random.h>
#include <cmu-trace.h>

#include "chc.h"

/*
  Notes/ToDo:
  - callback() can handle Pkt's that the node wanted to send, but couldn't.
    maybe we'll use it to clean caches or forward pkts
  - loccache has a static timeout for every entry.
  - loctable saves updates for a fixed time, that is specified in the update packet
  - no next locserver and missing targets, should send feedback packets to the sender.
    otherwise we use the same dead route every time :(
  - order handling is a bit hacked (too many patches :( ) should be cleaned
  - loccache is used for updates and everything else, which might destroy the GLS properties.
    i probably need to test a seperate loccache/poscache approach.
  - query forwarding should check tables first, because GLS is based on the hierachy distributed in the tables not the caches.

  - location notifications need some knowledge about data traffic to know what a live connection
    is. Unfortunately this can not be done with GPSR atm. So the mechanism is implemented, but
    can not be used. :(
*/

/********************/
/* Expire Functions */
/********************/

void GLSRequestScheduler::handle() {
    ls_->unansweredRequest(local_key);
}

void GLSUpdateScheduler::handle() {
    ls_->nackedUpdate((updcacheentry*)local_info);
}

void GLSUpdateScheduler::deleteInfo(void* info) {
    delete (updcacheentry*)info;
}

/*******/
/* GLS */
/*******/

GridLocService::GridLocService(Agent* p)
    : LocationService(p)
{
    parent = p;
    loccache  = new LSLocationCache(this,GLS_LOCCACHE_SIZE,GLS_LOCCACHE_TIMEOUT);
    loctable  = new LSLocationCache(this,GLS_LOCTABLE_SIZE);
    reqtable  = new LSRequestCache(this,GLS_REQTABLE_SIZE);

    liveconns = new ConnectionCache(this,GLS_LIVECONN_TIMEOUT);

    updtimer = new UpdateTimer(this);
    reqtimer = new GLSRequestScheduler(this,GLS_HEAP_SIZE);   
#ifdef ALLOW_ACKED_UPDATES
    updcache = new GLSUpdateScheduler(this,GLS_HEAP_SIZE);
#endif
}

GridLocService::~GridLocService() {
    delete loccache;
    delete loctable;
    delete reqtable;
    delete liveconns;
    delete reqtimer;
#ifdef ALLOW_ACKED_UPDATES
    delete updcache;
#endif
}

void GridLocService::init() {

    // Initialize Topology Info
    {
	// Get the max x, y coordinates of the entire terrain
	double maxx = mn_->T_->upperX();
	double maxy = mn_->T_->upperY();
	
	double len;
	if (maxx>maxy) len = maxx;
	else           len = maxy;

	if (len > (sizeof(int)*8*GLS_SMALLEST_GRID)) {
	    printf("Scenario is too big for this Grid Size on a %d-bit Machine\n",sizeof(int)*8); 
	    exit(1);
	}

	double smallest_grids = len / GLS_SMALLEST_GRID;
	max_order = (int)smallest_grids; // primitive ceiling
	while (smallest_grids <= (1 << max_order)) { max_order--; }
	max_order++; // correct offset
	if (max_order < 0) { max_order=0; }// primitive floor

	max_len = ((1 << max_order) * GLS_SMALLEST_GRID);
    }

    // Initialize Fields for LocUpdate 
    prevloc = getNodeInfo();
    predto = 0;
    prevlevel = max_order;

    // Schedule first UpdTimer Event
    updtimer->sched(0.0);
}

void GridLocService::recv(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    if ((iph->saddr() == parent->addr()) && (cmnh->num_forwards() == 0)) {
	// New Paket by me
	piggybackLocation(p);
    }else{
	// Other packets
	updateLocation(p);
    }

    if (locsh->valid_) {
      
	// Location Service Packet that needs to be processed
	switch (locsh->type_) {
	    case LOCS_UPDATE:     { recvUpdate(p); break; }
	    case LOCS_REQUEST:    { recvRequest(p); break; }
	    case LOCS_REPLY:      { recvReply(p); break; }
	    case LOCS_NOTIFY:     { recvNotify(p); break; }
	    case LOCS_UPDATE_ACK: { recvUpdAck(p); break; }
	    case LOCS_DATA:       { break; }
	}	
    }
}

void GridLocService::callback(Packet* &p) {

    // Do we need to evaluate callbacks for 
    // Cache consistency or forwarding ?

    // For Tracing we mark the pkt as callback
    struct hdr_locs *locsh = HDR_LOCS(p);
    if (locsh->valid_) { locsh->callback_ = 1; }
    
}

void GridLocService::updatePosition(struct nodelocation &entry, struct hdr_ip* iph /* = NULL */) {
  struct nodelocation* tmp = NULL;

  tmp = (nodelocation*)loccache->search(entry.id);

  if ((tmp!=NULL) && (tmp->ts > entry.ts)) {
    if (GLS_DEBUG>=2) {
      trace("LSUPOS: %.12f _%d_ [%d %.4f %.2f %.2f] to [%d %.4f %.2f %.2f]",
		    Scheduler::instance().clock(),
		    parent->addr(),
		    entry.id, entry.ts, entry.loc.x, entry.loc.y,
		    tmp->id, tmp->ts, tmp->loc.x, tmp->loc.y);
    }
    // Update
    entry.ts = tmp->ts;
    entry.loc = tmp->loc;
    entry.sqr = tmp->sqr;
    if (iph != NULL) {
      // Update GPSR Routing Info
      iph->dx() = tmp->loc.x;
      iph->dy() = tmp->loc.y;
      iph->dz() = tmp->loc.z;
    }
  }
}

void GridLocService::updateLocation(Packet *p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Try to update Location Info of Packets
    if (locsh->valid_) {
      
	switch (locsh->type_) {

	    case LOCS_UPDATE:
		if (locsh->in_correct_grid)
		    updatePosition(locsh->next,iph);
		break;
	    case LOCS_REQUEST:
		updatePosition(locsh->next,iph);
		break;
	    case LOCS_UPDATE_ACK:
	    case LOCS_REPLY:
	    case LOCS_DATA:
		updatePosition(locsh->dst,iph);
		break;
	    default:
		break;
	}
	updatePosition(locsh->src);
    }
}


void GridLocService::piggybackLocation(Packet *p) {
    
    // Don't piggyback LS Packets
    if (HDR_CMN(p)->ptype()== PT_LOCS) { return; }

    struct hdr_locs *locsh = HDR_LOCS(p);

    // Piggyback Info of this Node
    if (locsh->valid_ == 0)
	locsh->init();
    locsh->type_ = LOCS_DATA;
    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->src.sqr = getGrid(); 
}

void GridLocService::evaluatePacket(const Packet *p) {
    evaluateLocation((Packet*)p);
}

void GridLocService::evaluateLocation(Packet *p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_gpsr *gpsrh = HDR_GPSR(p);
    struct hdr_greedy *greedyh = HDR_GREEDY(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    if ((cmnh->ptype()==PT_GPSR)&&(gpsrh->mode_ == GPSRH_BEACON)) {
	// Direct Neighbor Information from GPSR Beacons
	struct nodelocation neighb;

	neighb.id = iph->saddr();
	neighb.ts = Scheduler::instance().clock(); // Off by a few because Beacons don't have a TS
	neighb.loc.x = gpsrh->hops_[0].x;
	neighb.loc.y = gpsrh->hops_[0].y;
	neighb.loc.z = gpsrh->hops_[0].z;
	// Reverse lookup of Grid Info
	neighb.sqr = getGrid(neighb.loc.x,neighb.loc.y);

	loccache->add(&neighb);
	loctable->update(&neighb);

	return;
    }

        if ((cmnh->ptype()==PT_GREEDY)&&(greedyh->mode_ == GREEDYH_BEACON)) {
	// Direct Neighbor Information from GPSR Beacons
	struct nodelocation neighb;

	neighb.id = iph->saddr(); 
	neighb.ts = Scheduler::instance().clock(); // Off by a few because Beacons don't have a TS
	neighb.loc.x = greedyh->hops_[0].x;
	neighb.loc.y = greedyh->hops_[0].y;
	neighb.loc.z = greedyh->hops_[0].z;
	// Reverse lookup of Grid Info
	neighb.sqr = getGrid(neighb.loc.x,neighb.loc.y);

	loccache->add(&neighb);
	loctable->update(&neighb);

	return;
    }

    if (locsh->valid_) {
      
	bool new_entry;
	struct nodelocation* eval = NULL;

	switch (locsh->type_) {

	    case LOCS_UPDATE:
		if (locsh->in_correct_grid){ eval = &(locsh->next); } 
		break;
	    case LOCS_REQUEST: 
		eval = &(locsh->next); 
		break;
	    case LOCS_UPDATE_ACK:
	    case LOCS_REPLY:
	    case LOCS_NOTIFY:
	    case LOCS_DATA:
		eval = &(locsh->dst); 
		break; 
	    default:
		break;
	}

	// Evaluate extra Info:
	//  - update Location Table Entries
	//  - add/update Location Cache Entries
	if (eval != NULL) {
	    loctable->update(eval);
	    new_entry = loccache->add(eval);
	    if ((GLS_IMMEDIATE_NOTIFICATION) && (new_entry))
		parent->notifyPos(eval->id); 
	}

	// Every Packet has SRC Info
	loctable->update(&(locsh->src));
	new_entry = loccache->add(&(locsh->src));

	if ((GLS_IMMEDIATE_NOTIFICATION) && (new_entry))
	    parent->notifyPos(locsh->src.id); 
      
    }
}

void GridLocService::recvUpdate(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Don't process my own updates
    if ((iph->saddr() == parent->addr()) && (cmnh->num_forwards() == 0)) { return; }

    // Packet is in the right grid and is searching
    //  for the right server. Let it be.
    if ((locsh->in_correct_grid) && (iph->daddr() != parent->addr())) {
      return;
    }

    // Check if we're in the to-be-updated grid
    if (inSameGrid(getNodeInfo(),locsh->dst)) {

	// Mark Pkt as been in the correct grid
	locsh->in_correct_grid = true;

	// If we can't forward this packet to a "better" node,
	// we are the locserver for this node in this square
	if (!forwardUpdate(p)) { 

	    if (true) {
		struct nodelocation me = getNodeInfo();
		trace("LSCLS: %.12f _%d_ [%d] for %d [%d] in %d",
			      Scheduler::instance().clock(),
			      parent->addr(),
			      me.sqr.grid,
			      locsh->src.id,
			      locsh->src.sqr.grid,
			      locsh->dst.sqr.order);   
	    }
	    
	    loctable->add(&(locsh->src));

#ifdef ALLOW_ACKED_UPDATES
	    // Send ACK to SRC
	    struct nodelocation info;
	    info.id = parent->addr();
	    info.ts = Scheduler::instance().clock();
	    mn_->getLoc(&info.loc.x, &info.loc.y, &info.loc.z);
	    info.sqr = getGrid();

	    Packet *pkt = newUpdAck(p,&info);
	    Packet::free(p);
	    p = pkt;
	    return;
#else
	    Packet::free(p);
	    p = NULL;
	    return;
#endif
	}
	return;
    }

    // Has Pkt been in the correct grid
    if (locsh->in_correct_grid) {

	// Outdated info made someone believe that
	//  we're in the right grid. Send Updates
	//  to clarify our position
	sendUpdates((locsh->dst.sqr.order)+1);

	// Reset Update and delay it till the sent 
	//  updates have reached their target grids.

	// Reset DST to Target Grid 
	iph->daddr() = NO_NODE; 
	iph->dx_ = locsh->dst.loc.x;
	iph->dy_ = locsh->dst.loc.y;
	iph->dz_ = locsh->dst.loc.z;

	// Reset Update State
	locsh->in_correct_grid = false;

	// Reset next Target Info
	locsh->next.id = NO_NODE;
	locsh->next.ts = -1.0;
	locsh->next.loc.x = -1.0;
	locsh->next.loc.y = -1.0;
	locsh->next.loc.z = -1.0;
	locsh->next.sqr.grid = -1;
	locsh->next.sqr.order = 0;

	// Warn about this Event
	if (GLS_DEBUG>=2)
	  trace("LSResetUPD: %.12f _%d_ %d (%d->%d)",
			Scheduler::instance().clock(),
			parent->addr(),
			locsh->src.id,
			locsh->src.sqr.grid,
			locsh->dst.sqr.grid);  

	// Delay with some buffer time
	double delay = 2 * (locsh->dst.sqr.order+1) * Random::uniform(GLS_UPD_JITTER); 
	Scheduler::instance().schedule(parent,p,delay);
	p = NULL;
	return;
    }
}

void GridLocService::recvReply(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);

    // Check if the reply is for me
    if (locsh->dst.id == parent->addr()) {

	// Packet has been evaluated, discard it

	if (GLS_DEBUG>=1)
	    trace("LSRR: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locsh->src.id,
			  locsh->dst.id);            
	
        // Cancel all pending Requests
	reqtimer->remove(locsh->src.id);

        // Delete Request Entry in ReqTable
	reqtable->remove(locsh->src.id);

	// Notify SendBuffer - done in evaluation

	Packet::free(p);
	p = NULL;
    }   
}


void GridLocService::recvNotify(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);

    // Check if the notification is for me
    if (locsh->dst.id == parent->addr()) {

	// Packet has been evaluated, discard it

	if (GLS_DEBUG>=1)
	    trace("LSRN: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locsh->src.id,
			  locsh->dst.id);            
	
        // Cancel all pending Requests
	reqtimer->remove(locsh->src.id);

        // Delete Request Entry in ReqTable
	reqtable->remove(locsh->src.id);

	Packet::free(p);
	p = NULL;
    }   
}


void GridLocService::recvUpdAck(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);

    // Check if the ack is for me
    if (locsh->dst.id == parent->addr()) {

	if (GLS_DEBUG>=3)
	    trace("LSRACK: %.12f _%d_ (%d->%d) [%d:%d]",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locsh->src.id,
			  locsh->dst.id,
			  locsh->upddst_.grid,
			  locsh->upddst_.order);            
	
	unsigned int key = key(locsh->upddst_.grid,locsh->upddst_.order);

        // Cancel all pending Events
	updcache->remove(key);

	Packet::free(p);
	p = NULL;
    }   
}

void GridLocService::recvRequest(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Don't process my own requests
    if ((iph->saddr() == parent->addr()) && (cmnh->num_forwards() == 0)) { return; }

    // Request queries for me, answer it
    if (locsh->dst.id == parent->addr()) {

      struct nodelocation info;
      info.id = parent->addr();
      info.ts = Scheduler::instance().clock();
      mn_->getLoc(&info.loc.x, &info.loc.y, &info.loc.z);
      info.sqr = getGrid();

      Packet *pkt = newReply(p,&info);
      Packet::free(p);
      p = pkt;

      if (GLS_DEBUG>=1) {
	struct hdr_locs *rep_locsh = HDR_LOCS(pkt);
	trace("LSSOR: %.12f _%d_ (%d->%d)",
		      Scheduler::instance().clock(),
		      parent->addr(),
		      rep_locsh->src.id,
		      rep_locsh->dst.id);         
      }

      return;
    }

    if (iph->daddr() == parent->addr()) {

	// I'm the Target LocServer and should forward the Request
	//  to the next LocServer or the DST itself
	sendRequest(p);

    }
    
    // Request is on it's way, leave it alone
}

void GridLocService::sendRequest(Packet* &p, bool fresh /* = false */) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Define common fields
    cmnh->direction() = hdr_cmn::DOWN;

    // NOTE: I think we should query the table first and the cache later,
    //       because the GLS routing hierarchy is based on the tables.
    //       Caches should only have backup capacity - mk

    // Check Location Cache for the Position of the Next LocServer
    //struct nodelocation* nxtLocServer = findClosest(loccache,locsh->dst);
    struct nodelocation* nxtLocServer = findClosest(loctable,locsh->dst);

    // If LocCache did not produce results 
    // Check Location Table for the Position of the Next LocServer
    //if (!nxtLocServer) { nxtLocServer = findClosest(loctable,locsh->dst); }
    if (!nxtLocServer) { nxtLocServer = findClosest(loccache,locsh->dst); }

    if (!nxtLocServer) {
	// No Next LocServer Drop
	parent->drop(p,DROP_LOCS_NONXTLS);
	Packet::free(p);
	p = NULL;
	return;
    }

    // Found Next Hop or DST
    locsh->next.id = nxtLocServer->id;
    locsh->next.ts = nxtLocServer->ts;
    locsh->next.loc = nxtLocServer->loc;
    locsh->next.sqr = nxtLocServer->sqr;

    // Mark Next LocServer as DST 
    iph->daddr() = nxtLocServer->id;
    iph->dx() = nxtLocServer->loc.x;
    iph->dy() = nxtLocServer->loc.y;
    iph->dz() = nxtLocServer->loc.z;
    
    cmnh->next_hop_ = nxtLocServer->id;

    if (fresh) {
	// Send
	parent->recv(p,(Handler *)0);
	p = NULL;
    }
}

bool GridLocService::forwardUpdate(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Define common fields
    cmnh->direction() = hdr_cmn::DOWN;

    struct nodelocation* dst = &(locsh->dst);

    // Check Location Cache for the Position of the Next LocServer in this Grid
    struct nodelocation* nxtLocServer = findClosest(loccache,locsh->src,dst);

    // If LocCache did not produce results 
    // Check Location Table for the Position of the Next LocServer in this Grid
    if (!nxtLocServer) { nxtLocServer = findClosest(loctable,locsh->src,dst); }

    if (!nxtLocServer) { return false; }

    // Found Next Hop or DST
    
    // Mark Routing Info for Updating
    locsh->next.id = nxtLocServer->id;
    locsh->next.ts = nxtLocServer->ts;
    locsh->next.loc = nxtLocServer->loc;
    locsh->next.sqr = nxtLocServer->sqr;

    // Mark Next LocServer as DST
    iph->daddr() = nxtLocServer->id;
    iph->dx() = nxtLocServer->loc.x;
    iph->dy() = nxtLocServer->loc.y;
    iph->dz() = nxtLocServer->loc.z;
    
    cmnh->next_hop_ = nxtLocServer->id;

    return true;
}

bool GridLocService::poslookup(Packet *p) {
    
    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_locs *locsh = HDR_LOCS(p);

    // Prune Location Service Packets
    if (HDR_CMN(p)->ptype()== PT_LOCS) { return true; }
    
    // Check Location Cache for the Position of the DST
    struct nodelocation* qryTarget = (nodelocation*)loccache->search(iph->daddr());

    // If LocCache did not produce results 
    // Check Location Table for the Position of the DST
    if (!qryTarget) { qryTarget = (nodelocation*)loctable->search(iph->daddr()); }

    if (qryTarget) {
	
	// Found Location in Cache or Table
	if (GLS_DEBUG>=3)
	    trace("LSIIC: %.12f _%d_ [%d %.4f %.2f %.2f]",      
			  Scheduler::instance().clock(),
			  parent->addr(),
			  qryTarget->id,
			  qryTarget->ts,
			  qryTarget->loc.x,
			  qryTarget->loc.y);

	// Mark DST 
	iph->dx() = qryTarget->loc.x;
	iph->dy() = qryTarget->loc.y;
	iph->dz() = qryTarget->loc.z;
      
	// Mark DST for LocService
	locsh->dst.id = qryTarget->id;
	locsh->dst.ts = qryTarget->ts;
	locsh->dst.loc = qryTarget->loc;
	locsh->dst.sqr = qryTarget->sqr;

	// Update SRC Info
	locsh->type_ = LOCS_DATA;
	locsh->src.id = parent->addr();
	locsh->src.ts = Scheduler::instance().clock();
	locsh->src.sqr = getGrid();
	mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
      
	return true;
    }

    // No Location can be found in the LocCache or LocTable, so we'll
    // check if the dst is already queried and if it is not, we'll
    // send a Location Request Packet

    // Check if DST has been Queried

    locrequest* request = (locrequest*)reqtable->search(iph->daddr());

    if (!request) {
	
	// DST has not been queried in a long time (if ever)

	Packet *pkt = newRequest(iph->daddr());
	
	// Remember this request
	struct hdr_locs *locshdr = HDR_LOCS(pkt);
	// NOTE: I use maxhop_ & seqno_ to implement a cheap upper limit
	//       for requests
	unsigned int nownr = 0;
	unsigned int maxnr = GLS_MAX_REQUESTS;
	struct locrequest tmpreq = { iph->daddr(), Scheduler::instance().clock(), maxnr, nownr };
	reqtable->add(&(tmpreq));

	// Schedule next Request Cycle
	double delay =  (0.5 * ((double) (0x1 << (2 * nownr) )));
	reqtimer->add(iph->daddr(), delay);

	if (GLS_DEBUG>=1)
	    trace("LSSRC: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locshdr->src.id,
			  locshdr->dst.id);     
	
	sendRequest(pkt,true);  
	
    }else{

	// Request has been sent and is handled by timer 
	if (GLS_DEBUG>=3)
	    trace("LSRCIP: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  iph->saddr(),
			  iph->daddr()); 
	
    }
    
    return false;
}

Packet* GridLocService::newReply(Packet* req, struct nodelocation* infosrc) {

    Packet *pkt = parent->allocpkt();
	
    struct hdr_ip *req_iph = HDR_IP(req);
    struct hdr_locs *req_locsh = HDR_LOCS(req);

    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);

    locsh->init();
    locsh->type_ = LOCS_REPLY;
    locsh->seqno_ = 0;

    locsh->src.id = infosrc->id;
    locsh->src.ts = infosrc->ts;
    locsh->src.loc = infosrc->loc;
    locsh->src.sqr = infosrc->sqr; 

    locsh->dst.id = req_locsh->src.id;
    locsh->dst.ts = req_locsh->src.ts;
    locsh->dst.loc = req_locsh->src.loc;
    locsh->dst.sqr = req_locsh->src.sqr;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = req_iph->saddr();
    iph->ttl() = GLS_TTL;

    iph->dx_ = req_locsh->src.loc.x;
    iph->dy_ = req_locsh->src.loc.y;
    iph->dz_ = req_locsh->src.loc.z;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Requests need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    
    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;

    return pkt;
} 

Packet* GridLocService::newRequest(nsaddr_t dst_) {

    Packet *pkt = parent->allocpkt();
	
    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);

    locsh->init();
    locsh->type_ = LOCS_REQUEST;
    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->src.sqr = getGrid();

    locsh->dst.id = dst_;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = dst_;
    iph->ttl() = GLS_TTL;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->xmit_failure_ = 0;

    // All Location Requests need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    
    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;

    return pkt;
} 

Packet* GridLocService::newUpdate(struct nodelocation* dst_) {

    Packet *pkt = parent->allocpkt();
	
    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);

    locsh->init();
    locsh->type_ = LOCS_UPDATE;
    locsh->seqno_ = 0;

    locsh->dst.loc = dst_->loc;
    locsh->dst.sqr = dst_->sqr; 

    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->src.sqr = getGrid();
    // Predict the valid time for this Update
    //  depending on the level it is sent to
    int level = dst_->sqr.order + 1;
    locsh->src.timeout = (predto * level) + Scheduler::instance().clock();

    // For clarity the grid center is considered our next target
    locsh->next.loc = dst_->loc;
    locsh->next.sqr = dst_->sqr; 

    // Traceing
    locsh->updreason_ = upd_reason;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = NO_NODE; // For now. Might cause problems
    iph->ttl() = GLS_TTL;

    // Let GPSR route this pkt to the center of the target square
    iph->dx_ = dst_->loc.x;
    iph->dy_ = dst_->loc.y;
    iph->dz_ = dst_->loc.z;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Updates need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    
    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;

    return pkt;
} 

Packet* GridLocService::newUpdAck(Packet* upd, struct nodelocation* infosrc) {

    Packet *pkt = parent->allocpkt();
	
    struct hdr_ip *upd_iph = HDR_IP(upd);
    struct hdr_locs *upd_locsh = HDR_LOCS(upd);

    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);

    locsh->init();
    locsh->type_ = LOCS_UPDATE_ACK;
    locsh->seqno_ = 0;

    locsh->src.id = infosrc->id;
    locsh->src.ts = infosrc->ts;
    locsh->src.loc = infosrc->loc;
    locsh->src.sqr = infosrc->sqr; 

    locsh->dst.id = upd_locsh->src.id;
    locsh->dst.ts = upd_locsh->src.ts;
    locsh->dst.loc = upd_locsh->src.loc;
    locsh->dst.sqr = upd_locsh->src.sqr;

    locsh->upddst_ = upd_locsh->dst.sqr;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = upd_iph->saddr();
    iph->ttl() = GLS_TTL;

    iph->dx_ = upd_locsh->src.loc.x;
    iph->dy_ = upd_locsh->src.loc.y;
    iph->dz_ = upd_locsh->src.loc.z;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Requests need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    
    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;

    return pkt;
} 

Packet* GridLocService::newNotify(struct nodelocation* dst_) {

    Packet *pkt = parent->allocpkt();
	
    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);

    locsh->init();
    locsh->type_ = LOCS_NOTIFY;
    locsh->seqno_ = 0;

    locsh->dst.id = dst_->id;
    locsh->dst.ts = dst_->ts;
    locsh->dst.loc = dst_->loc;
    locsh->dst.sqr = dst_->sqr; 

    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->src.sqr = getGrid();

    locsh->next.loc = dst_->loc;
    locsh->next.sqr = dst_->sqr; 

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = dst_->id;
    iph->ttl() = GLS_TTL;

    iph->dx_ = dst_->loc.x;
    iph->dy_ = dst_->loc.y;
    iph->dz_ = dst_->loc.z;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Updates need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;

    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;
    
    return pkt;
} 

/************************/
/* GLS Packet Functions */
/************************/

void GridLocService::unansweredRequest(nsaddr_t id) {

    locrequest* request = (locrequest*)reqtable->search(id);

    if (request == NULL) {
	printf("Warning: Timer tried to access invalid Request ! Check Table/Timer Synchronisation.\n");
	exit(1);
    }

    unsigned int nownr = request->seqno + 1;
    if (nownr < request->maxhop) {
	
	// We may try again

	Packet *pkt = newRequest(id);
	  
	// Remember this request
	struct hdr_locs *locsh = HDR_LOCS(pkt);

	struct locrequest tmpreq = { id, Scheduler::instance().clock(), request->maxhop, nownr };
	reqtable->add(&tmpreq);

	// Schedule next Request Cycle
	double delay =  (0.5 * ((double) (0x1 << (2 * nownr) )));
	// Limit request delay
	if (delay >= GLS_REQUEST_MAX_PERIOD) { delay = GLS_REQUEST_MAX_PERIOD; }
	reqtimer->add(locsh->dst.id, delay);

	if (GLS_DEBUG>=2)
	    trace("LSNRC: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locsh->src.id,
			  locsh->dst.id);
	
	sendRequest(pkt,true);
    }else{

      // We tried some, and now we wait for the SendBuffer to
      //  initiate a new cycle (making sure the pkt still exists)
      reqtable->remove(id);

    }
    
}

void GridLocService::sendUpdates(unsigned int order) {

    if (order == 0) { 
	printf("Warning: Tried to send Updates to my own grid !\n"); 
	return; 
    }

    int grid = -1;
    int mygrid = (getGrid()).grid;

    // Get Base Value for all order-(n-1) squares of order-n
    int base = mygrid & (0xffffffff << shiftVal(order));

    if (GLS_DEBUG>=3)
      trace("LSSUPD: %.12f _%d_ [%d]->(%d)",
		    Scheduler::instance().clock(),
		    parent->addr(),
		    mygrid,
		    order);

    // Correct mygrid for hierarchy level 
    mygrid = (mygrid & (0xffffffff << shiftVal(order-1)));
    
    // Send to all 3 order-(n-1) squares that make up the 
    //  order-n square with my order-(n-1) square
    for (int i=0;i<4;i++) {
	grid = base + (i << shiftVal(order-1));

	// Don't send to my own square
	if (grid == mygrid) { continue; }

	struct nodelocation dst;
	dst.loc = getGridPos(grid,order-1);
	dst.sqr.grid = grid;
	dst.sqr.order = order-1;
	
	// Generate Update
	Packet *p = newUpdate(&dst);

#ifdef ALLOW_ACKED_UPDATES
	// Keep Info about this Update
	updcacheentry* target = new updcacheentry;
	target->sqr = dst.sqr;
	target->cnt = 0;
	unsigned int key = key(target->sqr.grid,target->sqr.order);
	double findelay = // Scales a little with order and number
	    (double)((target->sqr.order) << (target->cnt)) + 
	    Random::uniform(GLS_UPD_JITTER); 
	updcache->add(key,findelay,(void*)target);
#endif

	// Send Updates jittered, to avoid collisions
	//  and solve bootstrapping issue
	double delay = order * Random::uniform(GLS_UPD_JITTER);
	Scheduler::instance().schedule(parent,p,delay);

	p = NULL;
    }
}

void GridLocService::updateLocServers() {

    // get up-to-date Info
    struct nodelocation nowloc = getNodeInfo();

    // Check for border crossing
    for(int level = 1; level <= max_order; level++) {
	if (!inSameGrid(nowloc,prevloc,level)) {
	    // Send Updates to LocServers of all hierarchy 
	    //  levels that have changed
	    upd_reason = "XING";
	    sendUpdates(level+1);
	}
    }

    // Check for exceeded update distance or timeout
    double dx = (nowloc.loc.x-prevloc.loc.x); dx *= dx;
    double dy = (nowloc.loc.y-prevloc.loc.y); dy *= dy; 
    double distance = sqrt(dx+dy);
    double age = (nowloc.ts - prevloc.ts);

    if ((distance >= GLS_UPDATE_DISTANCE) || (age > predto)) {

	// Distance has higher priority in Trace
	if (age > predto) { upd_reason = "PRED"; }
	if (distance >= GLS_UPDATE_DISTANCE) { upd_reason = "DIST"; }

	// Set Timeout for Updates
	if (mn_->speed() != 0) {
	    predto = (double)(2*GLS_UPDATE_DISTANCE)/mn_->speed();
	}

	// Higher Hierarchies get fewer Updates
	for(unsigned int level = 1; level <= prevlevel; level++) {
	    // Send Updates to all LocServers of this level
	    sendUpdates(level);
	}
	// Reset Level Counter
	prevlevel--;
	if (prevlevel == 0) { prevlevel = max_order; }
    }

    // Inform active communication partners of position changes
    if (!((prevloc.loc.x == nowloc.loc.x) && (prevloc.loc.y == nowloc.loc.y))) {
	/*
	  ! NOT SAFE ! Do not maniplulate members of tmp.
	  Needs fixing...
	*/
	connectionentry* tmp = liveconns->getAll();

	while (tmp != NULL) {

	    struct nodelocation* qryTarget = (nodelocation*)loccache->search(tmp->dst);
	    if (!qryTarget) { qryTarget = (nodelocation*)loctable->search(tmp->dst); }
	    if (!qryTarget) { 
		printf("Warning: Live Connection does not know partner info ! No notification will be sent !\n");
	    } else {

		// Generate Notification
		Packet *p = newNotify(qryTarget);
		
		//parent->recv(p,(Handler *)0);
		
		// Send Updates jittered, to avoid collisions
		//  and solve bootstrapping issue
		double delay = Random::uniform(GLS_NOTIFY_JITTER);
		Scheduler::instance().schedule(parent,p,delay);
		
		p = NULL;
	    }
	    tmp = tmp->next;
	}
    }

    // Memorize nowloc as prevloc
    prevloc = nowloc;
}

void GridLocService::nackedUpdate(updcacheentry* e) {

    // Don't exceed maximum retry count
    e->cnt++;
    if (e->cnt >= GLS_MAX_UPDATE_RETRIES) { return; }

    // An Update we sent did not reach a target, thus
    //  we send it again, hoping that connectivity
    //  has improved.    

    upd_reason = "NACK";

    struct nodelocation dst;
    dst.loc = getGridPos(e->sqr.grid,e->sqr.order);
    dst.sqr.grid = e->sqr.grid;
    dst.sqr.order = e->sqr.order;

    // Generate Update
    Packet *p = newUpdate(&dst);

    // Keep Info about this Update
    unsigned int key = key(e->sqr.grid,e->sqr.order);
    double findelay = // Scales a little with order and number
	(double)((e->sqr.order) << (e->cnt)) + 
	Random::uniform(GLS_UPD_JITTER); 
    updcache->add(key,findelay,(void*)e);

    // Send Updates jittered, to avoid collisions
    //  and solve bootstrapping issue
    double delay = (e->sqr.order+1) * Random::uniform(GLS_UPD_JITTER);
    Scheduler::instance().schedule(parent,p,delay);
}

/*************************/
/* GLS Support Functions */
/*************************/

//! Returns the closest known next hop or NULL (Wrapper Function)
nodelocation* GridLocService::findClosest(void* cache, nodelocation loc_, nodelocation* dst_) {

    unsigned int size;
    CHCEntry** table = ((LSLocationCache*)cache)->getTable(&size);

    nodelocation* closest = NULL;
    nsaddr_t closestId = parent->addr();
  
    // Walk through Table
    for (unsigned int i=0; i<size; i++) {
        if (table[i] != NULL) {
	    CHCEntry* tmp = table[i];
	    while (tmp != NULL) {
		nodelocation* tmpinfo = (nodelocation*)tmp->info;
		if (closer(tmpinfo->id,closestId,loc_.id)) { 
		    if (dst_ != NULL) {
			// Closest must be in the same grid as the dst
			if (inSameGrid(*dst_,*tmpinfo)) { closest = tmpinfo; closestId = tmpinfo->id; }
		    }else{
			closest = tmpinfo; closestId = tmpinfo->id; 
		    }
		}
		tmp = tmp->next;
	    } 
	}
    }

    // Check if no closer target could be found
    if (closest == NULL) { return NULL; }

    return closest;
}

//! Return reference Position for a (grid,order)-pair
struct position GridLocService::getGridPos(struct square sqr) {
    return getGridPos(sqr.grid, sqr.order);
}

struct position GridLocService::getGridPos(int grid, int order) {

  struct position gridpos;
    gridpos.z = 0;

    // Get upper left grid no.
    grid = grid & (0xffffffff << shiftVal(order));

    // Get row & column of grid
    int tmp = grid;
    unsigned int row = 0;
    unsigned int col = 0;
    for (int i=0; i<16; i++) {
	row = row << 1; 
	col = col << 1;
	if (tmp & 0x40000000) { row++; }
	if (tmp & 0x80000000) { col++; }
	tmp = tmp << 2;
    } 

    // Calculate upper left corner
    gridpos.x = (GLS_SMALLEST_GRID * (col));
    gridpos.y = (GLS_SMALLEST_GRID * (row));
    // Correct for center
    if (order == 0) {
	gridpos.x += GLS_SMALLEST_GRID * 0.5;
	gridpos.y += GLS_SMALLEST_GRID * 0.5;
    }else{
	gridpos.x += GLS_SMALLEST_GRID * (1 << (order-1));
	gridpos.y += GLS_SMALLEST_GRID * (1 << (order-1));
    }

    return gridpos;
}

//! Check if two nodes are in the same grid of a specified order
bool GridLocService::inSameGrid(struct nodelocation src, struct nodelocation dst, int order /* = -1 */)
{
    if (order == -1) {
	// Choose biggest order of SRC & DST
	if (src.sqr.order > dst.sqr.order) { order = src.sqr.order; }
	else { order = dst.sqr.order; }
    }

    unsigned int mask = 0xffffffff; // valid for 32-bit arch.
    mask = mask << shiftVal(order);

    return ( ((src.sqr.grid & mask) == (dst.sqr.grid & mask)) ? true:false);
}

//! Check if him is closer to dst than me
bool GridLocService::closer(nsaddr_t me, nsaddr_t him, nsaddr_t dst) {
  // adapted from HGPS code
    int my_dist = me - dst;
    int his_dist = him - dst;

    if (me == him) return false;

    if (my_dist == 0) return true;
    if (his_dist == 0) return false;

    if (((my_dist > 0) && (his_dist<0)) ||
	((my_dist < 0) && (his_dist<0) && (my_dist < his_dist)) ||
	((my_dist > 0) && (his_dist>0) && (my_dist < his_dist))) {
	return true;
    }else{
	return false;
    }
}

//! Return the grid the caller is in */
struct square GridLocService::getGrid() {

    // Check own position
    double x,y,z;
    mn_->getLoc(&x, &y, &z);

    return getGrid(x,y);
}

//! Return the grid coresponding to x/y */
struct square GridLocService::getGrid(double x, double y) {

    // adapted from HGPS code
    double len_x, len_y;
    int grid_num = 0;
    struct square loc;

    // assume minx = miny = 0. otherwise, use x - minx in place of x similar for y
    len_x = len_y = max_len;
     
    while ((len_x > GLS_SMALLEST_GRID)|| (len_y > GLS_SMALLEST_GRID)) {
	if (x < len_x/2) {
	    if (y < len_y/2) { grid_num = grid_num * 4 + 0; }
	    else { y -= len_y/2; grid_num = grid_num * 4 + 1; }
	}else{
	    x -= len_x/2;
	    if (y<len_y/2) { grid_num = grid_num * 4 + 2; }
	    else{ y -= len_y/2; grid_num = grid_num * 4 + 3; }
	}
	len_x = len_x/2;
	len_y = len_y/2;
    }
    
    loc.grid = grid_num;
    loc.order = 0; // smallest order
    
    return loc;
}

struct nodelocation GridLocService::getNodeInfo() {
    struct nodelocation loc;
    loc.id = parent->addr();
    loc.ts = Scheduler::instance().clock();
    mn_->getLoc(&loc.loc.x, &loc.loc.y, &loc.loc.z);
    loc.sqr = getGrid();
    return loc;
}

/********/
/* Size */
/********/

int GridLocService::hdr_size(Packet* p) {

    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_locs *locsh = HDR_LOCS(p);

    // Defining Base Field Types in Bytes
    const unsigned int id         = 4;
    const unsigned int locCoord   = 3;
    const unsigned int timeStamp  = 2;
    const unsigned int TTL        = 1;
    const unsigned int grid       = 2;
    const unsigned int order      = 1;
    const unsigned int square     = grid + order;
    const unsigned int position   = locCoord + locCoord;

    // All LS based routing agents must be listed here - mk
    if (cmnh->ptype() == PT_GPSR) { return 0; } // GPSR Packet
    if (cmnh->ptype() == PT_GREEDY) { return 0; } // GREEDY Packet
    if (cmnh->ptype() == PT_LOCS) { // LOCS Packet
	switch (locsh->type_) {
	    case LOCS_UPDATE:
		return (2*id + 2*position + 2*timeStamp + square + timeStamp + TTL);
	    case LOCS_REQUEST: 
		return (2*id + 2*position + 2*timeStamp + id + TTL);
	    case LOCS_REPLY:
		return (2*id + 2*position + 2*timeStamp + TTL);  
	    case LOCS_UPDATE_ACK:
		return (2*id + 2*position + 2*timeStamp + order + TTL);  
	    default:
		printf("Invalid LOCS Packet wants to know it's size !\n");
		abort();
	}	
    }

    // Data Packet
    return (2*id + 2*position);
}

/********************/
/* Connection Cache */
/********************/

ConnectionCache::ConnectionCache(LocationService* parent, const double expire) {
    this->parent = parent;
    if (expire < 0.0) { this->expire = 0.0; }
    else { this->expire = expire; }

    first = last = NULL;
}

ConnectionCache::~ConnectionCache() {

    connectionentry* tmp;
    while (last != NULL) {
	tmp = last->prev;
	delete last;
	last = tmp;
    }
    first = last;
}

connectionentry* ConnectionCache::remove(connectionentry* victim) {

    assert(victim != NULL);
    connectionentry* tmp = victim->next;

    // Correct successor
    if (victim->next != NULL) { victim->next->prev = victim->prev; }
    // Correct predecessor
    if (victim->prev != NULL) { victim->prev->next = victim->next; }
    // Correct first and last
    if (first == victim) { first = victim->next; }
    if (last == victim) { last = victim->prev; }
    
    delete victim;
    return tmp;
}

void ConnectionCache::cleanUp() {

    if (expire == 0.0) { return; }

    connectionentry* tmp = first;
    double now = Scheduler::instance().clock();

    while (tmp != NULL) {
	if ((now - tmp->ts) > expire) {
	    tmp = remove(tmp);
	}else{
	    tmp = tmp->next;
	}
    }
}

void ConnectionCache::add(const unsigned int dst) {
    
    cleanUp();

    connectionentry* tmp = first;

    // Walk the list and try to find an existing entry
    while (tmp != NULL) {
	if (tmp->dst == (nsaddr_t)dst) {
	    // Update
	    tmp->ts = Scheduler::instance().clock();
	    return;
	}else{
	    tmp = tmp->next;
	}
    }

    // No entry could be found, so we append a new one to the end of the chain
    tmp = new connectionentry();
    tmp->dst = (nsaddr_t)dst;
    tmp->ts = Scheduler::instance().clock();
    tmp->next = NULL;
    tmp->prev = last;
    if (last != NULL) { last->next = tmp; }
    last = tmp;

    if (first == NULL) { first = tmp; }
}

void ConnectionCache::printTable() {
    
    connectionentry* tmp = first;
    printf("%d has LiveConns:", parent->addr());
    while (tmp != NULL) {
	printf("(%d,%.4f) ",tmp->dst,tmp->ts);
	tmp = tmp->next;
    }
    printf("\n");
}


#endif // _GridLocService_cc




