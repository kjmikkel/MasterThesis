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

#ifndef _ReaLocService_cc
#define _ReaLocService_cc


#include "realocservice.h"
#include "hdr_locs.h"

#include "chc.h"

/*
  Notes:
*/

/**************************/
/* Request Timer Children */
/**************************/

void RLSRequestScheduler::handle() {
    ls_->nextRequestCycle(local_key);
}

void RLSRequestDelay::handle() {
    ls_->forwardRequest((Packet*&)local_info);
}

void RLSRequestDelay::deleteInfo(void* info) {
    Packet::free((Packet*)info);
}

/*******/
/* RLS */
/*******/

ReaLocService::ReaLocService(Agent* p)
  : LocationService(p)
{
    parent = p;

    loccache = new LSLocationCache(this,RLS_LOCCACHE_SIZE,RLS_LOCCACHE_TIMEOUT);
    forwcache = new LSSeqNoCache(this,RLS_FORWCACHE_SIZE,RLS_FORWCACHE_TIMEOUT);
    seqnocache = new LSSeqNoCache(this,RLS_FORWCACHE_SIZE);
    reqtable = new LSRequestCache(this,RLS_REQTABLE_SIZE,RLS_REQTABLE_TIMEOUT);

#ifndef ALLOW_MAX_FLOOD
    reqtimer = new RLSRequestScheduler(this,RLS_HEAP_SIZE);
#endif

#ifdef ALLOW_REQUEST_SUPPRESSION
    reqdelay = new RLSRequestDelay(this,RLS_REQDELAY_SIZE);
    supinfocache = new LSSupCache(this,RLS_SUPINFOCACHE_SIZE,RLS_SUPINFOCACHE_TIMEOUT);
#endif
}

ReaLocService::~ReaLocService() {
    delete loccache;
    delete forwcache;
    delete reqtable;
    delete seqnocache;
#ifndef ALLOW_MAX_FLOOD
    delete reqtimer;
#endif
#ifdef ALLOW_REQUEST_SUPPRESSION
    delete reqdelay;
    delete supinfocache;
#endif
}

void ReaLocService::recv(Packet* &p) {

    if (!active()) { return; } 


    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    if ((iph->saddr() == parent->addr()) && (cmnh->num_forwards() == 0)) {
	// New Data Pakets need no evaluation , since they contain nothing
        // They only need piggybacked src info
        piggybackSourceLocation(p);
    }else{
	// We'll try to update info in traversing packets that have outdated info
	updateLocation(p);
	// Some of the traversing Packets have Location Information for us
	evaluateLocation(p);
    }

    if (locsh->valid_) {
      
	// Location Service Packet that needs to be processed
	switch (locsh->type_) {
	    case LOCS_REQUEST: { 
		if (--iph->ttl() == 0) {
		    if (locs_verbose) {
			trace("LSTTL: %.12f _%d_ %d (%d->%d) %d [%d]",
				      Scheduler::instance().clock(),
				      parent->addr(),
				      cmnh->uid(),
				      locsh->src.id,
				      locsh->dst.id,
				      locsh->seqno_,
				      locsh->maxhop_);
		    }
		    Packet::free(p);
		    p = NULL;
		    return;
		}
		recvRequest(p); 
		break; 
	    }
	    case LOCS_REPLY:  
		recvReply(p);
		break;
	    case LOCS_DATA:
		break;
	}

	// Propagate node location as lasthop information
	//  if the packet has not been consumed
	if (p != NULL) {
	    piggybackLasthopLocation(p);
	}
    }
}

void ReaLocService::callback(Packet* &p) {

    // Do we need to evaluate callbacks for 
    // Cache consistency or forwarding ?

    // For Tracing we mark the pkt as callback
    struct hdr_locs *locsh = HDR_LOCS(p);
    if (locsh->valid_) { locsh->callback_ = 1; }
    
}

void ReaLocService::sleep() { 
    active_ = false;
#ifndef ALLOW_MAX_FLOOD
    reqtimer->shutdown(); 
#endif
}

void ReaLocService::piggybackSourceLocation(Packet *p) {
    struct hdr_locs *locsh = HDR_LOCS(p);
    
    assert((HDR_IP(p)->saddr() == parent->addr()) && (HDR_CMN(p)->num_forwards() == 0));

    if (locsh->valid_ == 0) {
	locsh->init();
	locsh->type_ = LOCS_DATA;
    }

    // Newly generated Data Pkts need piggybacked src info
    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->lasthop.id = parent->addr();
}

void ReaLocService::piggybackLasthopLocation(Packet *p) {
    struct hdr_locs *locsh = HDR_LOCS(p);
    
    locsh->lasthop.id = parent->addr();
    locsh->lasthop.ts = Scheduler::instance().clock();
    mn_->getLoc(&locsh->lasthop.loc.x, &locsh->lasthop.loc.y, &locsh->lasthop.loc.z);
}

void ReaLocService::updatePosition(struct nodelocation &entry, struct hdr_ip* iph /* = NULL */) {
  struct nodelocation* tmp = NULL;

  tmp = (nodelocation*)loccache->search(entry.id);
  if ((tmp!=NULL) && (tmp->ts > entry.ts)) {
    if (locs_verbose) {
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
      // Update Routing Info
      iph->dx() = tmp->loc.x;
      iph->dy() = tmp->loc.y;
      iph->dz() = tmp->loc.z;
    }
  }
}

void ReaLocService::updateLocation(Packet *p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_ip *iph = HDR_IP(p);

    // Check Location Info of LocService Packets
    if (locsh->valid_) {
      switch (locsh->type_) {
	  case LOCS_DATA:
	  case LOCS_REPLY:
	    updatePosition(locsh->dst,iph);
          case LOCS_REQUEST:
	  default:
	    updatePosition(locsh->src);
      }
    }
}

void ReaLocService::evaluatePacket(const Packet *p) {
  evaluateLocation((Packet*)p);
}

void ReaLocService::evaluateLocation(Packet *p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);

    if (locsh->valid_) {

        bool new_entry;
	switch (locsh->type_) {
	    case LOCS_REPLY:
	        new_entry = loccache->add(&locsh->dst);
		if ((RLS_IMMEDIATE_NOTIFICATION) && (new_entry))
		  parent->notifyPos(locsh->dst.id); 
	    case LOCS_REQUEST:
	    default: 
	        new_entry = loccache->add(&locsh->src);
		if ((RLS_IMMEDIATE_NOTIFICATION) && (new_entry))
		  parent->notifyPos(locsh->src.id); 
	}
    }

}

void ReaLocService::sendRequest(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);

    // Define common fields
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->next_hop_ = MAC_BROADCAST;

#ifdef ALLOW_DIRECTIONAL_FLOODING

    double myx,myy,myz;
    mn_->getLoc(&myx, &myy, &myz);
    double dist = distance(locsh->lasthop.loc.x, locsh->lasthop.loc.y, myx, myy);
    double maxdist = God::instance()->getRadioRange();
    double delay = RLS_DF_MAX_DELAY * (1 - ((dist * dist) / (maxdist * maxdist)));

    if ((delay == RLS_DF_MAX_DELAY)||(delay < 0)) {

	// Originating Nodes need not wait the max.delay
	//  since no one else is sending this pkt

	// Nodes that receive the pkt even though they are 
	//  beyond norm radio range may produce a negative
	//  delay. This may not happen.
	// NOTE: This might happen in a small percentage of
	//       cases, where the phy has no "hardcoded" 
	//       max.range

	delay = 0;
    }

#else

    double delay = bcastDelay();

#endif
    
    // Requests need a manual update of the lasthop info
    piggybackLasthopLocation(p);

    // In an ideal case we could just hand the pkt over to the LL
    //target_->recv(p,(Handler *)0);

    Scheduler::instance().schedule(target_,p,delay);

    p = NULL;
}

#ifdef ALLOW_REQUEST_SUPPRESSION
bool ReaLocService::suppressedPkt(Packet* &p) {

    /* 
       Implements a combination of a distance-/counter-based 
       suppression scheme for broadcasts.
       Scheme can be replaced if needed.
    */

    struct hdr_locs *locsh = HDR_LOCS(p);
    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);

    // Check if an supinfocache entry exists
    supinfo* pktinfo = (supinfo*)supinfocache->search(cmnh->uid());

    if (pktinfo == NULL) {

	// First time broadcast
	double myx,myy,myz;
	mn_->getLoc(&myx, &myy, &myz);
	double dist = distance(locsh->lasthop.loc.x, locsh->lasthop.loc.y, myx, myy);
	
	bool tooClose = false;
	if (dist < RLS_DIST_THRESHOLD) { tooClose = true; }
	else                           { tooClose = false; }

	struct supinfo tmpsi = { 
	    cmnh->uid(), Scheduler::instance().clock(), 1, dist, tooClose
	};
	supinfocache->add(&tmpsi);
	return tooClose;

    }else{

	// Possible rebroadcast (delayed, queued pkt exists)
	if ((pktinfo->blocked) || (pktinfo->cnt >= RLS_CNT_THRESHOLD)) {

	    // Node is not allowed to forward this pkt or
	    // pkt has been received too often
	    return true;

	}else{

	    // Lasthop distance evaluation shall decide whether to 
	    // rebroadcast this pkt or not
	    double myx,myy,myz;
	    mn_->getLoc(&myx, &myy, &myz);
	    double dist = distance(locsh->lasthop.loc.x, locsh->lasthop.loc.y, myx, myy);
	    double dmin;
	    if (dist < pktinfo->dmin) { dmin = dist; }
	    else { dmin = pktinfo->dmin; }
	    
	    bool tooClose = false;
	    if (dmin < RLS_DIST_THRESHOLD) { tooClose = true; }
	    else                           { tooClose = false; }

	    struct supinfo tmpsi = { 
		cmnh->uid(), Scheduler::instance().clock(), 
		pktinfo->cnt++, dmin, tooClose
	    };
	    supinfocache->add(&tmpsi);
	    return tooClose;

	}
    }
}
#endif

void ReaLocService::forwardRequest(Packet* &p) {

    // Forward Packet
    sendRequest(p); 
}

void ReaLocService::recvRequest(Packet* &p) {

    struct hdr_locs *req_locsh = HDR_LOCS(p);
    struct hdr_cmn *req_cmnh = HDR_CMN(p);

    // Check if we had this Pkt before
    unsigned int key_ = key(req_locsh->src.id,req_locsh->dst.id);

    int seqno_;
    seqno_ = forwcache->find(key_);

#ifdef ALLOW_MULTIPLE_REPLIES
    // If we're not the DST, check if we had this Pkt
    if (!(req_locsh->dst.id == parent->addr()))
#endif
	// Prune Requests that have already been processed
	if ((seqno_>=0)&&((unsigned int)seqno_>=req_locsh->seqno_)) {
	    if (locs_show_processing)
		trace("LSDAP: %.12f _%d_ %d (%d->%d) %d [%d]",
			      Scheduler::instance().clock(),
			      parent->addr(),
			      req_cmnh->uid(),
			      req_locsh->src.id,
			      req_locsh->dst.id,
			      req_locsh->seqno_,
			      req_locsh->maxhop_);
	    Packet::free(p);
	    p = NULL;
	    return;
	}
    
    
    // We never had this request (or not for a long time) -> Remember it
    struct seqnoentry tmpsqn = { 
	key_, Scheduler::instance().clock(), req_locsh->seqno_ 
    };
    forwcache->add(&tmpsqn);           

    if (req_locsh->dst.id == parent->addr()) {

	// Packet is for me and needs a reply 
	struct nodelocation info;
	info.id = parent->addr();
	info.ts = Scheduler::instance().clock();
	mn_->getLoc(&info.loc.x, &info.loc.y, &info.loc.z);

	Packet *pkt = genReply(p,&info);
	Packet::free(p);
	p = pkt;

	if (locs_verbose) {
	    struct hdr_locs *locsh = HDR_LOCS(pkt);
	    trace("LSSOR: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  locsh->src.id,
			  locsh->dst.id);         
	}
	return;

    }else{
	
      // Packet is not for me process it
     
      if (locs_show_processing){
	  trace("LSPR: %.12f _%d_ %d (%d->%d) %d",
			Scheduler::instance().clock(),
			parent->addr(),
			req_cmnh->uid(),
			req_locsh->src.id,
			req_locsh->dst.id,
			req_locsh->seqno_);      
      }         

#ifdef ALLOW_CACHED_REPLY	

	// Check if we can answer the Request from our cache
	struct nodelocation* qryTarget = (nodelocation*)loccache->search(req_locsh->dst.id);

	if (qryTarget) {

	    Packet *pkt = genReply(p, qryTarget);
	    Packet::free(p);
	    p = pkt;

	    if (locs_verbose) {
		struct hdr_locs *locsh = HDR_LOCS(pkt);
		trace("LSSCR: %.12f _%d_ (%d->%d)",
		      Scheduler::instance().clock(),
		      parent->addr(),
		      locsh->dst.id,
		      locsh->src.id);            
	    }
	    return;
	}
	
#endif

	// Check if we were the last node that should
	//  recv this request
	if (--req_locsh->maxhop_ == 0) {
	  Packet::free(p);
	  p = NULL;
	  return;
	}

#ifdef ALLOW_REQUEST_SUPPRESSION

	struct hdr_cmn *cmnh = HDR_CMN(p);
	
	if (!suppressedPkt(p)) {
	    
	    if (!reqdelay->queued(cmnh->uid())) {
		
		// Schedule pkt for rebroadcast after a RAD
		reqdelay->add(cmnh->uid(), RLS_SUPRAD_TIME, (void*)p);
		p = NULL;
		return;
		
	    }else{
		
		// Pkt is already scheduled for rebroadcast
		// discard this copy
		Packet::free(p);
		p = NULL;
		return;
		
	    }
	    
	}else{
	    
	    // Rebroadcast is suppressed and all scheduled copies
	    // will be cancelled
	    reqdelay->remove(cmnh->uid());
	    Packet::free(p);
	    p = NULL;
	    return;
	    
	}
	
#else
	
	// Forward Packet
	sendRequest(p);
	
#endif
	
    }
}

void ReaLocService::recvReply(Packet* &p) {

    struct hdr_locs *locsh = HDR_LOCS(p);

    if (locsh->dst.id == parent->addr()) {
	// The original Request was mine, get rid of the reply 
	if (locs_verbose)
	  trace("LSRR: %.12f _%d_ (%d->%d)",
			Scheduler::instance().clock(),
			parent->addr(),
			locsh->src.id,
			locsh->dst.id);            

#ifndef ALLOW_MAX_FLOOD
	// Cancel all pending Requests
	reqtimer->remove(locsh->src.id);
#endif

        // Delete Request Entry in ReqTable
	reqtable->remove(locsh->src.id);

	// Already notified parent in evaluation

	Packet::free(p);
	p = NULL;
    }   
}

bool ReaLocService::poslookup(Packet *p) {
    
    if (!active()) { return false; }

    struct hdr_ip *iphdr = HDR_IP(p);
    struct hdr_locs *locshdr = HDR_LOCS(p);
    
    // Prune Location Service Packets
    if ((locshdr->type_ == LOCS_REPLY) || (locshdr->type_ == LOCS_REQUEST))
	return true;

    // Check Location Cache for the Position of the DST
    struct nodelocation* qryTarget = (nodelocation*)loccache->search(iphdr->daddr());

    if (qryTarget) {
      // found location in cache
      if (locs_verbose)
	trace("LSIIC: %.12f _%d_ [%d %.4f %.2f %.2f]",      
		      Scheduler::instance().clock(),
		      parent->addr(),
		      iphdr->daddr(),
		      qryTarget->ts,
		      qryTarget->loc.x,
		      qryTarget->loc.y);
      // Mark DST for Agent
      iphdr->dx() = qryTarget->loc.x;
      iphdr->dy() = qryTarget->loc.y;
      iphdr->dz() = qryTarget->loc.z;
      
      // Mark DST for LocService
      locshdr->dst.id = qryTarget->id;
      locshdr->dst.ts = qryTarget->ts;
      locshdr->dst.loc.x = qryTarget->loc.x;
      locshdr->dst.loc.y = qryTarget->loc.y;
      locshdr->dst.loc.z = qryTarget->loc.z;
      
      // Update SRC Info
      locshdr->type_ = LOCS_DATA;
      locshdr->src.id = parent->addr();
      locshdr->src.ts = Scheduler::instance().clock();
      mn_->getLoc(&locshdr->src.loc.x, &locshdr->src.loc.y, &locshdr->src.loc.z);
      
      return true;
    }

    // No Location can be found in the Location Cache, so we'll
    // check if the dst is already queried and if it is not, we'll
    // send a Location Request Packet

    // Check if DST has been queried

    locrequest* request = (locrequest*)reqtable->search(iphdr->daddr());

    if (!request) {
	
	// DST has not been queried in a long time (if ever)
	//   -> start new cycle if requests can still be sent

        // Inquire SeqNo from Cache in case we had requested
        //  this target before
	int seqno_ = seqnocache->find(iphdr->daddr());
	if (seqno_ == -1) { seqno_ = 0; }

	// Generate Request
#ifdef ALLOW_MAX_FLOOD
	Packet *pkt = genRequest(iphdr->daddr(), seqno_,RLS_REQ_MAXHOP);
#else
	Packet *pkt = genRequest(iphdr->daddr(), seqno_,RLS_REQ_INITHOP);
#endif
	struct hdr_locs *locsh = HDR_LOCS(pkt);
	struct hdr_cmn *cmnh = HDR_CMN(pkt);

	// Keep info about having sent this packet in caches
	unsigned int key_ = key(locsh->src.id,locsh->dst.id);
	double now = Scheduler::instance().clock();

	struct seqnoentry tmpfrw = { 
	    key_, now, locsh->seqno_ 
	};
	forwcache->add(&tmpfrw);    

	struct locrequest tmpreq = { 
	    iphdr->daddr(), now, locsh->maxhop_,locsh->seqno_ 
	};
	reqtable->add(&tmpreq);

	struct seqnoentry tmpsqn = { 
	    locsh->dst.id, now, locsh->seqno_ 
	};
	seqnocache->add(&tmpsqn);

#ifndef ALLOW_MAX_FLOOD
	// Schedule next Request Cycle
	double findelay = (RLS_REQTABLE_HOP_TIMEOUT*locsh->maxhop_);
	reqtimer->add(locsh->dst.id, findelay);
#endif

	if (locs_verbose)
	    trace("LSSRC: %.12f _%d_ %d (%d->%d) %d",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  cmnh->uid(),
			  locsh->src.id,
			  locsh->dst.id,
			  locsh->seqno_);     
	
	sendRequest(pkt);  
	
    }else{

	// Request has been sent and is handled by timer or timeout
	if (locs_verbose && locs_show_in_progress) {
	    struct hdr_cmn *cmnhdr = HDR_CMN(p);
	    trace("LSRCIP: %.12f _%d_ %d (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  cmnhdr->uid(),
			  iphdr->saddr(),
			  iphdr->daddr()); 
	}
    }
    return false;
}

void ReaLocService::nextRequestCycle(nsaddr_t dst_) {
    
    locrequest* request = (locrequest*)reqtable->search(dst_);

    if (!request) { // request expired; don't try any longer
	return;
    }

    if (request->maxhop >= RLS_REQ_MAXHOP) {
		
	// Max Hop Distance exceeded; DST can not be reached
	if (locs_verbose)
	    trace("LSMHDE: %.12f _%d_ (%d->%d)",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  parent->addr(),
			  dst_);         
	request->dst = NO_NODE;
	  
    }else{

	// Maybe DST is further away, increase radius
#ifdef ALLOW_EXP_REQUEST
	unsigned int this_maxhop = (request->maxhop << 1);
	if (this_maxhop > RLS_REQ_MAXHOP)
	    this_maxhop = RLS_REQ_MAXHOP;
#else
	unsigned int this_maxhop = (request->maxhop + RLS_REQ_LINSTEPHOP);
#endif

	// Inquire SeqNo from Cache
	int seqno_ = seqnocache->find(dst_);
	if (seqno_ == -1) { seqno_ = 0; }

	// Generate Request
	Packet *pkt = genRequest(dst_, (request->seqno + 1),this_maxhop);
	struct hdr_locs *locsh = HDR_LOCS(pkt);
	struct hdr_cmn *cmnh = HDR_CMN(pkt);
  
	// Keep info about having sent this packet in caches
	unsigned int key_ = key(locsh->src.id,locsh->dst.id);
	double now = Scheduler::instance().clock();

	struct seqnoentry tmpfrw = { 
	    key_, now, locsh->seqno_ 
	};
	forwcache->add(&tmpfrw);    

	struct locrequest tmpreq = { 
	    dst_, now, locsh->maxhop_,locsh->seqno_ 
	};
	reqtable->add(&tmpreq);

	struct seqnoentry tmpsqn = { 
	    locsh->dst.id, now, locsh->seqno_ 
	};
	seqnocache->add(&tmpsqn);

	// Schedule next Request Cycle
	double findelay = (RLS_REQTABLE_HOP_TIMEOUT*locsh->maxhop_);
	reqtimer->add(locsh->dst.id, findelay);

	if (locs_verbose)
	    trace("LSNRC: %.12f _%d_ %d (%d->%d) %d [%d]",
			  Scheduler::instance().clock(),
			  parent->addr(),
			  cmnh->uid(),
			  locsh->src.id,
			  locsh->dst.id,
			  locsh->seqno_,
			  this_maxhop);  

	sendRequest(pkt);
    }
}

Packet* ReaLocService::genReply(Packet* req, struct nodelocation* infosrc) {

    Packet *pkt = parent->allocpkt();

    struct hdr_ip *req_iph = HDR_IP(req);
    struct hdr_locs *req_locsh = HDR_LOCS(req);

    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    
    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    iph->daddr() = req_iph->saddr();
    iph->ttl() = RLS_TTL;
    
    iph->dx_ = req_locsh->src.loc.x;
    iph->dy_ = req_locsh->src.loc.y;
    iph->dz_ = req_locsh->src.loc.z;
    
    locsh->init();
    locsh->type_ = LOCS_REPLY;
    locsh->src.id = infosrc->id;
    locsh->src.ts = infosrc->ts;
    locsh->src.loc.x = infosrc->loc.x;
    locsh->src.loc.y = infosrc->loc.y;
    locsh->src.loc.z = infosrc->loc.z;

    locsh->seqno_ = 0;
    locsh->maxhop_ = RLS_TTL+1;
    
    locsh->dst.id = req_locsh->src.id;
    locsh->dst.ts = req_locsh->src.ts;
    locsh->dst.loc.x = req_locsh->src.loc.x;
    locsh->dst.loc.y = req_locsh->src.loc.y;
    locsh->dst.loc.z = req_locsh->src.loc.z;
    
    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->xmit_failure_ = 0;
    
    cmnh->direction() = hdr_cmn::DOWN;
    
    return pkt;
}

Packet* ReaLocService::genRequest(nsaddr_t dst_, int seqno_, int maxhop_) {

    Packet *pkt = parent->allocpkt();

    struct hdr_locs *locsh = HDR_LOCS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);


    locsh->init();
    locsh->type_ = LOCS_REQUEST;
    locsh->src.id = parent->addr();
    locsh->src.ts = Scheduler::instance().clock();
    locsh->seqno_ = seqno_;
    locsh->maxhop_ = maxhop_;
    mn_->getLoc(&locsh->src.loc.x, &locsh->src.loc.y, &locsh->src.loc.z);
    locsh->dst.id = dst_;

    // Initilize LastHop Info for Directional Flooding 
    locsh->lasthop.id = parent->addr();
    locsh->lasthop.ts = Scheduler::instance().clock();
    locsh->lasthop.loc.x = locsh->src.loc.x;
    locsh->lasthop.loc.y = locsh->src.loc.y;
    locsh->lasthop.loc.z = locsh->src.loc.z;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = parent->addr();
    //iph->daddr() = IP_BROADCAST;
    iph->daddr() = dst_;
    iph->ttl() = RLS_TTL;

    cmnh->ptype() = PT_LOCS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->xmit_failure_ = 0;
    cmnh->size() = parent->hdr_size(pkt);

    
    return pkt;
}

/********/
/* Size */
/********/

int ReaLocService::hdr_size(Packet* p) {

    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_locs *locsh = HDR_LOCS(p);

    // Defining Base Field Types in Bytes
    const unsigned int id         = 4;
    const unsigned int locCoord   = 3;
    const unsigned int timeStamp  = 2;
    const unsigned int TTL        = 1;
    const unsigned int ReqSeqNo   = 1;
    const unsigned int position   = locCoord + locCoord;

    if (cmnh->ptype() == PT_LOCS) { // LOCS Packet

	switch (locsh->type_) {
	    case LOCS_REQUEST: 
#ifdef ALLOW_DIRECTIONAL_FLOODING
		// For suppression all pkts need lasthop info
		return (2*id + 3*position + timeStamp + id + TTL + ReqSeqNo);
#else
		return (id + 2*position + timeStamp + id + TTL + ReqSeqNo);
#endif
	    case LOCS_REPLY:
		return (2*id + 2*position + 2*timeStamp + TTL);  
	    default:
		printf("Invalid LOCS Packet wants to know it's size !\n");
		abort();
	}	
    }
    
    // Data Packet
    return (2*id + 2*position);
}
#endif // _ReaLocService_cc
