#include "hls_basic.h"
#include "../gpsr/gpsr.h"
#include "../greedy/greedy.h"
#include "../gopher/gopher.h"

// Header ////////////////////////////////////////////////
 
class HLSHeaderClass : public PacketHeaderClass {
public:
    HLSHeaderClass() : PacketHeaderClass("PacketHeader/HLS",sizeof(hdr_hls)) {
	bind_offset(&hdr_hls::offset_);
    }
} class_hlsheader;
 


// UpdateSender ////////////////////////////////////////////
UpdateSender::UpdateSender(Cellbuilder* cellbuilder, HLS* hls)
{
  cellbuilder_ = cellbuilder;
  hls_ = hls; 
}
void UpdateSender::start(){}


// UpdateReceiver //////////////////////////////////////////
UpdateReceiver::UpdateReceiver(HLSLocationCache* activeEntries, 
			       HLSLocationCache* passiveEntries, 
			       HLSLocationCache* outOfCellEntries, 
			       HLS* hls)
{
  this->activeEntries_ = activeEntries;
  this->passiveEntries_ = passiveEntries;
  this->outOfCellEntries_ = outOfCellEntries;
  this->hls_ = hls;
}

void UpdateReceiver::recv(Packet* &p, bool forceActiveSave)
{}

// RequestProcessor ////////////////////////////////////////
RequestProcessor::RequestProcessor(HLSLocationCache* passiveEntries, HLS* hls)
{
  this->passiveEntries_ = passiveEntries;
  this->hls_ = hls;
}

void RequestProcessor::recv(Packet* &p)
{}

void RequestProcessor::processRequestUnreachableCell(Packet* &p)
{}

// this method is called when a request for location information
// is flooded in the cell
// 
void RequestProcessor::recvCellcastRequest(Packet* &p)
{}

void RequestProcessor::cellcastReplyOnMacReceived(const Packet* p)
{}
void RequestProcessor::recvCirclecastRequest(Packet* &p, 
					     bool fromDropCallback)
{}

void RequestProcessor::circlecastRequestOnMacReceived(const Packet* p)
{}

void RequestProcessor::recvCellcastReply(Packet* &p)
{}
// produces a reply packet to a location query
// when A wants to know the location of B and the request 
// reaches B after the routing, this is the answer packet
Packet* RequestProcessor::newReply(Packet* req, struct nodeposition* infosrc) {

    Packet *pkt = hls_->allocpkt();
	
    struct hdr_ip *req_iph = HDR_IP(req);
    struct hdr_hls *req_hlsh = HDR_HLS(req);

    struct hdr_hls *hlsh = HDR_HLS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    // The new routing
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);
    struct hdr_gopher *gopherh = HDR_GOPHER(pkt);

    hlsh->init();
    hlsh->type_ = HLS_REPLY;

    hlsh->src.id = infosrc->id;
    hlsh->src.ts = infosrc->ts;
    hlsh->src.pos = infosrc->pos;
 
    hlsh->dst.id = req_hlsh->src.id;
    hlsh->dst.ts = req_hlsh->src.ts;
    hlsh->dst.pos = req_hlsh->src.pos;

    // request id
    hlsh->reqid.set(req_hlsh->reqid);

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = hls_->addr();
    // the destination is not the ip-source of the packet
    // (because this can also be someone who sent a request to
    // a higher level cell) but the request source of the packet
    iph->daddr() = req_hlsh->src.id;// req_iph->saddr();
    iph->ttl() = 128;

    iph->dx_ = req_hlsh->src.pos.x;
    iph->dy_ = req_hlsh->src.pos.y;
    iph->dz_ = req_hlsh->src.pos.z;

    cmnh->ptype() = PT_HLS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Requests need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    gpsrh->geoanycast = false;

    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;
    greedyh->geoanycast = false;

    gopherh->mode_ = GOPHERH_DATA_GREEDY;
    gopherh->port_ = hdr_gopher::LOCS;
    gopherh->geoanycast = false;
    
    return pkt;
} // end of RequestProcessor::newReply(...)


// this produces a reply to a cellcast request
// it means that the sender of the reply is the src in the req packet
// and the information is in info
void RequestProcessor::sendCellcastReply(const Packet* req, 
					 struct nodeposition* info)
{
    Packet *pkt = hls_->allocpkt();
	
    struct hdr_ip *req_iph = HDR_IP(req);
    struct hdr_hls *req_hlsh = HDR_HLS(req);

    struct hdr_hls *hlsh = HDR_HLS(pkt);
    struct hdr_ip *iph = HDR_IP(pkt);
    struct hdr_cmn *cmnh = HDR_CMN(pkt);
    struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
    struct hdr_greedy *greedyh = HDR_GREEDY(pkt);
    struct hdr_gopher *gopherh = HDR_GOPHER(pkt);

    hlsh->init();
    hlsh->type_ = HLS_CELLCAST_REPLY;

    // enter my position (the infos of the node who answers the
    // request)
    hlsh->src.id = hls_->addr();
    hlsh->src.ts = Scheduler::instance().clock();
    position pos;
    hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z);
    hlsh->src.pos = pos;
    // request id
    hlsh->reqid = req_hlsh->reqid;

    // enter the position of the node about which we had information
    // (stored in info)
    hlsh->dst.copyFrom(info);

    // write the id of the node who send the cellcast request
    hlsh->cellcastRequestSender = req_hlsh->cellcastRequestSender;

    iph->sport() = RT_PORT;
    iph->dport() = RT_PORT;
    iph->saddr() = hls_->addr();
    // the destination of the reply packet is the node who sent the
    // CELLCAST_REQUEST
    iph->daddr() = req_iph->saddr();
    
    iph->ttl() = HLS_TTL;

    iph->dx_ = req_hlsh->src.pos.x;
    iph->dy_ = req_hlsh->src.pos.y;
    iph->dz_ = req_hlsh->src.pos.z;

    cmnh->ptype() = PT_HLS;
    cmnh->addr_type_ = NS_AF_INET;
    cmnh->num_forwards() = 0;
    cmnh->next_hop_ = NO_NODE;
    cmnh->direction() = hdr_cmn::DOWN;
    cmnh->xmit_failure_ = 0;

    // All Location Requests need to be transported by GPSR as Data Pkts
    gpsrh->mode_ = GPSRH_DATA_GREEDY;
    gpsrh->port_ = hdr_gpsr::LOCS;
    gpsrh->geoanycast = false;

    greedyh->mode_ = GREEDYH_DATA_GREEDY;
    greedyh->port_ = hdr_greedy::LOCS;
    greedyh->geoanycast = false;

    gopherh->mode_ = GOPHERH_DATA_GREEDY;
    gopherh->port_ = hdr_gopher::LOCS;
    gopherh->geoanycast = false;
    
    if(hls_verbose)
      {
	struct hdr_hls* hlsh = HDR_HLS(pkt);
	// HLS CELLCAST send (answer)
	hls_->trace("HLS_CC_sa %.12f (%d_%d) %d -> %d [%d %.5f %.2f %.2f]", 
		    Scheduler::instance().clock(), // timestamp
		    hlsh->reqid.node,        // the unique...
		    hlsh->reqid.nr,          // id of the request
		    hls_->addr(),                  // my address 
		    hlsh->cellcastRequestSender,// the one who asked
		    hlsh->dst.id,            // the info we got
		    hlsh->dst.ts,            // info timestamp
		    hlsh->dst.pos.x,         // the position ...		  
		    hlsh->dst.pos.y);        // of the node
      } 
    hls_->parent->recv(pkt, NULL);
}



// HandoverTimer
// responsible for periodically calling the timerCallback function
// in the HandoverManager which will check if we have left
// a cell a must therefore handover the information belonging to
// that cell
HandoverTimer::HandoverTimer(HandoverManager* manager, 
			     double intervall)
{
  this->manager_ = manager;
  this->intervall_ = intervall;
}

void HandoverTimer::expire(Event* e)
{
  manager_->timerCallback();
  double delay = Random::uniform(HLS_HANDOVER_JITTER);
  resched(intervall_ + delay); // reschedule the timer after the intervall
}
// end of HandoverTimer

// Handovermanager
HandoverManager::HandoverManager(HLSLocationCache* activeEntries, 
				 HLSLocationCache* passiveEntries,
				 HLSLocationCache* outOfCellEntries,
				 HLS* hls,
				 Cellbuilder* cellbuilder)
{
  this->hls_ = hls;
  this->activeEntries_ = activeEntries;
  this->passiveEntries_ = passiveEntries;
  this->outOfCellEntries_ = outOfCellEntries;
  this->cellbuilder_ = cellbuilder;
  this->timer_ = new HandoverTimer(this, HLS_HANDOVER_INTERVALL);
  double delay = Random::uniform(2*HLS_HANDOVER_JITTER);
  timer_->sched(HLS_HANDOVER_INTERVALL+delay);
  lastCell = -1;
}

HandoverManager::~HandoverManager()
  {
    delete timer_;
  }
// end of HandoverManager

// BUS /////////////////////////////////////////////////////
BasicUpdateSender::BasicUpdateSender(Cellbuilder* cellbuilder, HLS* hls)
  : UpdateSender(cellbuilder, hls)
{
  updateIntervall_ =  HLS_UPDATE_CHECK_INTERVALL; // seconds
  timer_ = new BUSTimer(this, updateIntervall_);
  lastUpdatePosition_ = new nodeposition[cellbuilder->getMaxLevel()];

  for(int i=0;i<cellbuilder->getMaxLevel();i++)
    {
      lastUpdatePosition_[i].pos.x = -999;
      lastUpdatePosition_[i].pos.y = -999;
      lastUpdatePosition_[i].pos.z = -999;
      lastUpdatePosition_[i].ts = -1;
      lastUpdatePosition_[i].targetcell = -1;
    }

  upd_reason = "";
}

BasicUpdateSender::~BasicUpdateSender()
{
  delete timer_;
  delete lastUpdatePosition_;
}

void BasicUpdateSender::start()
{
  double delay = Random::uniform(HLS_UPDATE_STARTUP_MAX_JITTER);
  timer_->sched(HLS_UPDATE_STARTUP+delay); 
}

void BasicUpdateSender::timerExpired()
{
  int level = 1;
  position pos = position();
  hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z); // writes the correct posinfos
  // to pos
  
  int address = hls_->addr();
  int responsibleCell = cellbuilder_->getRC(address, level, pos);

  while(responsibleCell != NO_VALID_LEVEL)
    {
      double dx = (pos.x - lastUpdatePosition_[level-1].pos.x); dx *= dx;
      double dy = (pos.y - lastUpdatePosition_[level-1].pos.y); dy *= dy; 
      double distance = sqrt(dx+dy);// contains the distance between the actual point and
      // the last sending point to the given level;
      
      // level-1 as index because indexing starts at 0, the levels start at 1
      if(lastUpdatePosition_[level-1].targetcell != responsibleCell)
	{
	  // we have on the actual level another RC, thus tell this RC
	  // that we exist
	  upd_reason = "NWRC"; // new RC
	  lastUpdatePosition_[level-1] = sendUpdatePacketToCell(responsibleCell, level);
	}
      else if(distance >= HLS_UPDATE_DISTANCE)
	{
	  upd_reason = "DIST"; // distance
	  lastUpdatePosition_[level-1] = sendUpdatePacketToCell(responsibleCell, level);
	}
#ifdef TIME_TRIGGERED_UPDATES_FOR_SECOND_HIGHEST_LEVEL
      // time triggerd for cell on second highest level
      else if((level == (cellbuilder_->getMaxLevel()-1)) &&
	      (lastUpdatePosition_[level-1].ts + (2*HLS_MAX_UPDATE_INTERVALL)) 
	      < Scheduler::instance().clock())
	{	  
	  // send a time triggerd update to the cell on the second highest level     
	  upd_reason = "TIME";
	  lastUpdatePosition_[level-1] = sendUpdatePacketToCell(responsibleCell, level);
	}
#endif
      // time triggered for top level cell
      else if((level == cellbuilder_->getMaxLevel()) &&
	      (lastUpdatePosition_[level-1].ts + HLS_MAX_UPDATE_INTERVALL) 
	      < Scheduler::instance().clock())
	{
	  // we are on the highest level and it was long ago that we've send an update to the
	  // top level cell
	  upd_reason = "TIME";
	  lastUpdatePosition_[level-1] = sendUpdatePacketToCell(responsibleCell, level);	  
	}

      level++;
      responsibleCell = cellbuilder_->getRC(hls_->addr(), level, pos); 
    }
}

// produces an update packet with the destination of the given cell
nodeposition BasicUpdateSender::sendUpdatePacketToCell(int cellid, int level)
{
  Packet *pkt = hls_->allocpkt();

  //////////////////////////////////////////
  // produce the packet
  /////////////////////////////////////////

  struct hdr_ip *iph = HDR_IP(pkt);
  struct hdr_cmn *cmnh = HDR_CMN(pkt);
  struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
  struct hdr_greedy *greedyh = HDR_GREEDY(pkt);
  struct hdr_gopher *gopherh = HDR_GOPHER(pkt);
  struct hdr_hls *hlsh = HDR_HLS(pkt);

  
  iph->sport() = RT_PORT;
  iph->dport() = RT_PORT;
  iph->saddr() = hls_->parent->addr();
  iph->daddr() = NO_NODE;

  position cellPosition = cellbuilder_->getPosition(cellid);
  // Let GPSR route this pkt to the center of the responsible cell
  // (or to the closest position possible)
  iph->dx_ = cellPosition.x;
  iph->dy_ = cellPosition.y;
  iph->dz_ = cellPosition.z;
  iph->ttl() = HLS_UPDATE_TTL_PER_LEVEL * level;

  cmnh->ptype() = PT_HLS;
  cmnh->addr_type_ = NS_AF_INET;
  cmnh->num_forwards() = 0;
  cmnh->next_hop_ = NO_NODE;
  cmnh->direction() = hdr_cmn::DOWN;
  cmnh->xmit_failure_ = 0;

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // init my own header
  hlsh->init();
  // now we have to put the actual position information into the
  // packet (together with a timestamp)
  nodeposition actualNodepos = hls_->getNodeInfo();
  actualNodepos.targetcell = cellid;
  hlsh->src.copyFrom(&actualNodepos);

  hlsh->type_ = HLS_UPDATE;

  // set the cell field
  hlsh->cell.id    = cellid;
  hlsh->cell.level = level;
  hlsh->cell.pos.x = cellPosition.x;
  hlsh->cell.pos.y = cellPosition.y;
  hlsh->cell.pos.z = cellPosition.z;

  // the reason
  hlsh->updreason_ = upd_reason;

  //////////////////////////////////////////////////////////
  // end of packet production
  //////////////////////////////////////////////////////////
  if(hls_trace_updates)
    {
      position pos;
      pos = cellbuilder_->getPosition(cellid);
      // HLS Update
      hls_->trace("HLS_UPD_s %.12f %d <%d %.2f %.2f (%d)> %s", 
		  Scheduler::instance().clock(), // timestamp
		  hls_->addr(),                  // address of update sender
		  cellid,                        // target cell
		  pos.x,                         // the  
		  pos.y,                         // coordinates of the cell
		  level,                         // level of the cell      
		  upd_reason);                   // the reason for the update
      
    }

  //////////////////////////////////////////////////////////////////////
  // send the packet
  /////////////////////////////////////////////////////////////////////

  hls_->parent->recv(pkt, NULL);

  return actualNodepos;
}

//BUR //////////////////////////////////////////////////////
BasicUpdateReceiver::BasicUpdateReceiver(HLSLocationCache* activeEntries, 
					 HLSLocationCache* passiveEntries, 
					 HLSLocationCache* outOfCellEntries,
					 HLS* hls)
  : UpdateReceiver(activeEntries, passiveEntries, outOfCellEntries, hls){};


// this method is called whenever an update message is received at
// this node. If the forceActiveSave flag is set, this means that 
// the packet must be put in the active store. This is necessary
// when the correct cell for the update packet can't be reached.
// (then, this recv-method will be called from the callback method
// in HLS). During the next round of active store checks (when it is
// checked if we are still in the correct cell or if we have to 
// give the entries to a neighbour), the routing tries to sent the 
// update to a node which is closer to (or in) the destination cell.
void BasicUpdateReceiver::recv(Packet* &p, bool forceActiveSave)
{
  hdr_hls* hlsh = HDR_HLS(p);

#ifdef AGGRESSIVE_CACHING
  passiveEntries_->add(&hlsh->src);
#endif

  position destPos;
  position myPos;
  // getting my actual position
  hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
  // getting the destination of the update package
  destPos = hlsh->cell.pos;
  // test
  if(hlsh->src.targetcell == hls_->getCell() && 
     !sameCell(&destPos, &myPos, hls_->cellbuilder_))
    {
      printf("### holy shit!error in sameCell calculation\n");
      exit(-1);
    }
  // endtest
  if(sameCell(&destPos, &myPos, hls_->cellbuilder_))
    {
      if(hls_trace_updates)
	{
	  int myCell = hls_->cellbuilder_->getCellID(myPos);
	  if(hlsh->type_ == HLS_UPDATE)
	    {
	      // HLS Update receive
	      hls_->trace("HLS_UD_r  %.12f %d<-%d [%.2f %.2f <%d>] <%d %.2f %.2f (%d)>", 
			  Scheduler::instance().clock(), // timestamp
			  hls_->addr(),                  // address of update receiver
			  hlsh->src.id,                  // address of update sender
			  myPos.x,                       // my...
			  myPos.y,                       // coordinates
			  myCell,                        // my cell
			  hlsh->cell.id,                 // target cell
			  destPos.x,                     // center...
			  destPos.y,                     // of the target cell
			  hlsh->cell.level);             // the level of that cell
	    }
	}

      activeEntries_->add(&hlsh->src);
      // we have been in the cell, thus we can free the packet.
      // this will tell gpsr that the packet arrived
      Packet::free(p);
      p = NULL;
      return;
    }
  
  if(forceActiveSave)
    {
      if(hls_trace_updates)
	{
	  int myCell = hls_->cellbuilder_->getCellID(myPos);
	  if(hlsh->type_ == HLS_UPDATE)
	    {
	      // HLS Update forced receive
	      hls_->trace("HLS_UD_fr %.12f %d<-%d [%.2f %.2f <%d>] <%d %.2f %.2f (%d)>", 
			  Scheduler::instance().clock(), // timestamp
			  hls_->addr(),                  // address of update receiver
			  hlsh->src.id,                  // address of update sender
			  myPos.x,                       // my...
			  myPos.y,                       // coordinates
			  myCell,                        // my cell
			  hlsh->cell.id,                 // target cell
			  destPos.x,                     // center...
			  destPos.y,                     // of the target cell
			  hlsh->cell.level);             // the level of that cell
	    }
	}


      outOfCellEntries_->add(&hlsh->src);
      // we have been in the cell, thus we can free the packet.
      // this will tell gpsr that the packet arrived
      Packet::free(p);
      p = NULL;
      return;
    }  
}// end of BUR::recv(..)

// BRP /////////////////////////////////////////////////////
BasicRequestProcessor::BasicRequestProcessor( HLSLocationCache* passiveEntries, HLS* hls)
  : RequestProcessor(passiveEntries, hls)
{
  cellcastRequestCache_ = new CellcastRequestCache(10, this);
  circlecastRequestCache_ = new CirclecastRequestCache(5, this);
  answerTimerList_ = NULL;
}

BasicRequestProcessor::~BasicRequestProcessor()
{

  delete cellcastRequestCache_;
  delete circlecastRequestCache_;
  AnswerTimer* tmp1 = answerTimerList_;
  AnswerTimer* tmp2;
  while(tmp1 != NULL)
    {
      tmp2 = tmp1;
      tmp1 = tmp1->next;
      delete(tmp2);
    }
}

// this method is called when we receive a request packet
void BasicRequestProcessor::recv(Packet* &p)
{
  
    struct hdr_hls *hlsh = HDR_HLS(p);
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);
     
    // Don't process my own requests
    if ((iph->saddr() == hls_->parent->addr()) && (cmnh->num_forwards() == 0)) { return; }

    // checking for correct cell is done later
    // (so, the answer can also be given from the cache)
    // first check if we know the answer, if yes, => reply
    // if we don't know the answer, check if it's the correct cell
    
    
    // Request query is for me, answer it
    if (hlsh->dst.id == hls_->parent->addr()) {
      
      struct nodeposition info;
      info.id = hls_->parent->addr();
      info.ts = Scheduler::instance().clock();
      hls_->mn_->getLoc(&info.pos.x, &info.pos.y, &info.pos.z);

      if(hls_verbose)
	{
	  // HLS Request answer
	  hls_->trace("HLS_REQ_a %.12f (%d_%d) %d -> %d", 
		      Scheduler::instance().clock(), // timestamp
		      hlsh->reqid.node,              // the unique...
		      hlsh->reqid.nr,                // id of the request
		      hls_->addr(),                  // my address 
		      hlsh->src.id);                 // address of request sender
	}
      
      Packet *pkt = newReply(p,&info);
      Packet::free(p);
      // schedule the answer
      hls_->parent->recv(pkt, NULL);      
      p = NULL;
                  
      return;
    }
    

    // in the destination cell, there are different possibilities:
    // - I know the info and can forward the request to the node
    // - I have to broadcast the request in the cell and wait for a reply
    //     * positive reply: send it to the node
    //     * negative reply: send it to the next RC or drop the reqeust
    //       if I'm the home cell on the highest level

    nodeposition* nodepos = hls_->findEntry(hlsh->dst.id);

    if(nodepos != NULL)
      {
	// this means we have information about the position
	// of the requested node
	// forward the request to the position of that node
	if(nodepos->ts < hlsh->dst.ts)
	  {
	    // if our info is older than the one we receive, we update
	    // our info (implicit update)
	    nodepos->ts    = hlsh->dst.ts;
	    nodepos->pos.x = hlsh->dst.pos.x;
	    nodepos->pos.y = hlsh->dst.pos.y;
	    nodepos->pos.z = hlsh->dst.pos.z;
	    return;
	  }
	else
	  {
	    if(hls_verbose)
	      { 
		position myPos;
		hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
		int myCell = hls_->cellbuilder_->getCellID(myPos);
		// HLS Request located (we have found a posinfo for the node)
		hls_->trace("HLS_REQ_l %.12f (%d_%d) %d ->%d [%.5f %.2f %.2f] <%d %d (%d)>", 
			    Scheduler::instance().clock(), // timestamp
			    hlsh->reqid.node,              // the unique...
			    hlsh->reqid.nr,                // id of the request
			    hls_->addr(),                  // my address 
			    hlsh->dst.id,                  // address of target
			    nodepos->ts,                   // timestamp of entry
			    nodepos->pos.x,                // coordinates...
			    nodepos->pos.y,                // of the entry
			    myCell,                        // my cell id
			    hlsh->cell.id,                 // the target cell of the request
			    hlsh->cell.level);             // the target level
	      }	
	    // write the target position to the ip-header for the routing
	    iph->dx_ = nodepos->pos.x;
	    iph->dy_ = nodepos->pos.y;
	    iph->dz_ = nodepos->pos.z;
	    
	    // write the information to the hls-header to enable relocation
	    // (that means when a packet passes a node with newer location 
	    // info, the info in the packet will be updated)
	    hlsh->dst.ts = nodepos->ts;
	    hlsh->dst.pos.x = nodepos->pos.x;
	    hlsh->dst.pos.y = nodepos->pos.y;
	    hlsh->dst.pos.z = nodepos->pos.z;

	    //
	    if(hlsh->status_ != REQUEST_LOCATED)
	      {
		// depending on the level on which the information was
		// located, the TTL is very low. From now on, the packet
		// will be directed to the node which may be many hops
		// away, thus we need to reset the TTL to enable reaching
		// far away nodes
		iph->ttl() = HLS_TTL;
		hlsh->status_ = REQUEST_LOCATED;
	      }
	    
	    return;
	  }
      }

    // if we arrive here, we haven't had any information about
    // the position of the node

    // if our node isn't member of the destination cell,
    // let the routing do it's work
    position myPos;
    hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
    
    if(!sameCell(&myPos, &hlsh->cell.pos, hls_->cellbuilder_ ))
      {
      if(hls_verbose)
	{
	  // HLS Request forward
	  hls_->trace("HLS_REQ_f %.12f (%d_%d) %d ->...->%d", 
		      Scheduler::instance().clock(), // timestamp
		      hlsh->reqid.node,              // the unique...
		      hlsh->reqid.nr,                // id of the request
		      hls_->addr(),                  // my address 
		      hlsh->dst.id);                 // target address
	}
	return;
      }

    if(hls_->cellbuilder_->getCellID(myPos) != hlsh->cell.id)
      {
	printf("### error during resolving cellid\n");
      }

    // if we arrive here, we are in the same cell but don't 
    // know the position of the node. this means
    // we have to broadcast the request to the cell (marking 
    // it as broadcast to avoid infinite recursion) and process
    // the reply (if there is info, sending the request to the
    // node, if there isn't info, send the request to the
    // cell on the next level or if we are the highest level,
    // drop the request)

    cellcastRequestCache_->store(p, CELLCAST_BUFFER_ENTRY_TIMEOUT);
    sendCellcastRequest(hlsh->dst.id, hlsh->reqid, &hlsh->cell);

    // we must set the packet to NULL because we are the destination
    // if we'd return it to the routing, it would be dropped because
    // of no route and the place would be freed/ reserved for another
    // packet
    p = NULL;
}

// sends a request for information about the given node
// if another node in transmission range has information
// about the location of node nodeid, it'll answer;
// if not, no answer packet is sent
void BasicRequestProcessor::sendCellcastRequest(int nodeid, request_id reqid, 
						struct_cell* cell)
{
  if(hls_verbose)
    {
      // HLS CELLCAST send request
      hls_->trace("HLS_CC_sr %.12f (%d_%d) %d -*%d", 
		  Scheduler::instance().clock(), // timestamp
		  reqid.node,                    // the unique...
		  reqid.nr,                      // id of the request
		  hls_->addr(),                  // my address 
		  nodeid);                       // target address
    }

  Packet *p = hls_->allocpkt();
  
  struct hdr_hls *hlsh = HDR_HLS(p);
  struct hdr_ip *iph = HDR_IP(p);
  struct hdr_cmn *cmnh = HDR_CMN(p);
  struct hdr_gpsr *gpsrh = HDR_GPSR(p);
  struct hdr_greedy *greedyh = HDR_GREEDY(p);
  struct hdr_gopher *gopherh = HDR_GOPHER(p);

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;
  gpsrh->nhops_ = 1;

  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;
  greedyh->nhops_ = 1;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;
  gopherh->nhops_ = 1;


  hls_->mn_->getLoc(&gpsrh->hops_[0].x, 
		    &gpsrh->hops_[0].y, 
		    &gpsrh->hops_[0].z);
  
  cmnh->ptype_ = PT_HLS;
  cmnh->next_hop_ = IP_BROADCAST;
  cmnh->addr_type_ = NS_AF_INET;

  iph->daddr() = IP_BROADCAST << Address::instance().nodeshift();
  iph->dport() = RT_PORT;

  hlsh->type_ = HLS_CELLCAST_REQUEST;

  hls_->mn_->getLoc(&hlsh->src.pos.x, 
		    &hlsh->src.pos.y, 
		    &hlsh->src.pos.z);
  hlsh->src.id = hls_->addr();
  hlsh->src.ts = Scheduler::instance().clock();
  hlsh->dst.id = nodeid;   // set the id of the requested node
  // set the cell information
  hlsh->cell.id    = cell->id;
  hlsh->cell.level = cell->level;
  hlsh->cell.pos.x = cell->pos.x;
  hlsh->cell.pos.y = cell->pos.y;
  hlsh->cell.pos.z = cell->pos.z;  

  // the request id
  hlsh->reqid = reqid;

  // my node is the sender of the request
  hlsh->cellcastRequestSender = hls_->addr();

  Scheduler::instance().schedule(hls_->target_, p, 0);
} // end of sendCellcastRequest

void BasicRequestProcessor::recvCellcastRequest(Packet* &p)
{
  // If the node knows something, he will respond.
  // If not, no respond is send, because we don't want to overload the network.
  // Keep up work after the first timeout in sender of cellcastRequest

  struct hdr_hls *hlsh = HDR_HLS(p);

  nodeposition* nodepos = hls_->findEntry(hlsh->dst.id);
  
  if(nodepos != NULL)
    {
 
      double timeout;
      if(hlsh->dst.id == hls_->addr())
	{
	  // optimization:
	  // if our own node is requested, we answer directly 
	  // without waiting
	  sendCellcastReply(p, nodepos);	  
	  Packet::free(p);
	}
      else
	{	
	  // make the timer also depending of the age of the information
	  // (this means if we have a relatively new info, we 
	  // answer earlier than if we have an old info)
	  timeout = Random::uniform(CELLCAST_ANSWER_JITTER);
	  // schedule the timer to send our answer if we haven't heard another
	  // one answering in the meantime
	  CellcastAnswerTimer* tmpTimer = new CellcastAnswerTimer(this, timeout, p,
								  nodepos);
	  // put the timer in the list
	  tmpTimer->next = answerTimerList_;
	  answerTimerList_ = tmpTimer;
	}
    }
  else
    {
      Packet::free(p);
    }

  p = NULL;
}

// this method is called when we receive a cellcast reply on MAC, that means
// someone send a reply. We don't know if we are the target of that reply,
// the processing of this will be done in the function recvCellcastReply
void BasicRequestProcessor::cellcastReplyOnMacReceived(const Packet* p)
{
  struct hdr_hls* hlsh = HDR_HLS(p);
  struct hdr_ip* iph = HDR_IP(p);

  if(hlsh->cellcastRequestSender != hls_->addr())
    {
      // I haven't sent the request, but I can try to cache the
      // location
#ifdef AGGRESSIVE_CACHING
      passiveEntries_->add(&hlsh->dst);
#endif


      // search the answer timer list to see if we also have
      // scheduled an answer packet. If so, we'll eliminate the
      // packet (and the timer)
      AnswerTimer* tmpTimer = answerTimerList_;
      AnswerTimer* secondTmpTimer;
      AnswerTimer* last = NULL;
      
      while(tmpTimer != NULL)
	{

	  if(tmpTimer->status() != TIMER_PENDING)// the timer has already expired
	    {
	      // this code is for freeing the memory.
	      secondTmpTimer = tmpTimer->next;
	      if(answerTimerList_ == tmpTimer)
		{
		  answerTimerList_ = tmpTimer->next;
		} 
	      if(last != NULL)
		{
		  last->next = secondTmpTimer;
		}
	      
	      // we can remove the timer from our list
	      delete(tmpTimer);
	      tmpTimer = secondTmpTimer;
	      continue;
	    }


	  if(tmpTimer->reqId_.equals(&hlsh->reqid)) // we have a pending timer
	    {
		if(tmpTimer->status() == TIMER_PENDING)
		  {
		    tmpTimer->cancel();
		  }
		// after canceling the timer, we can remove it
		secondTmpTimer = tmpTimer->next;
		if(answerTimerList_ == tmpTimer)
		  {
		    answerTimerList_ = tmpTimer->next;
		  }
		if(last != NULL)
		  {
		    last->next = secondTmpTimer;
		  }
		
		// trace that we wanted to send but didn't do so because
		// another node sent his answer earliere
		if(hls_verbose)
		  {
		    struct hdr_hls* hlsh = tmpTimer->getHlsHeaderOfPacket();

		    hls_->trace("HLS_CC_da %.12f (%d_%d) %d -> %d [%d %.5f %.2f %.2f]", 
				Scheduler::instance().clock(), // timestamp
				hlsh->reqid.node,        // the unique...
				hlsh->reqid.nr,          // id of the request
				hls_->addr(),                  // my address 
				hlsh->cellcastRequestSender,// the one who asked
				hlsh->dst.id,            // the info we got
				hlsh->dst.ts,            // info timestamp
				hlsh->dst.pos.x,         // the position ...		  
				hlsh->dst.pos.y);        // of the node		    
		  }
		
		delete(tmpTimer);
		tmpTimer = secondTmpTimer;
		continue;
	      }
	  last = tmpTimer;
	  tmpTimer = tmpTimer->next;       
	}
    }
}

// we received a cellcast reply (and are the real destination of it)
void BasicRequestProcessor::recvCellcastReply(Packet* &p)
{
  struct hdr_hls* hlsh = HDR_HLS(p);
  struct hdr_ip* iph = HDR_IP(p);

  if((hlsh->cellcastRequestSender != hls_->addr())) 
    // we haven't send the request for this reply
    {
      return;
    }

  if(hls_verbose)
    {
      // HLS CELLCAST receive (answer)
      hls_->trace("HLS_CC_ra %.12f (%d_%d) %d <-%d [%d %.5f %.2f %.2f]", 
		  Scheduler::instance().clock(), // timestamp
		  hlsh->reqid.node,        // the unique...
		  hlsh->reqid.nr,          // id of the request
		  hls_->addr(),                  // my address 
		  hlsh->src.id,            // the one who answered
		  hlsh->dst.id,            // the info we got
		  hlsh->dst.ts,            // info timestamp
		  hlsh->dst.pos.x,         // the position ...		  
		  hlsh->dst.pos.y);        // of the node
    }


  // we have to get the appropriate Request from my cache and
  // forward it to the destination address
  Packet* requestPacket = cellcastRequestCache_->getRequest(hlsh->reqid);
  if(requestPacket == NULL)
    {
      // we use the cellcast reply also for the circlecast replies
      requestPacket = circlecastRequestCache_->getRequest(hlsh->reqid);
    }

  if(requestPacket != NULL)
    {
      // the result of the query is at the moment stored in "dst"
      passiveEntries_->add(&hlsh->dst);
      // give it to the routing to go on
      hls_->parent->recv(requestPacket, NULL);      
    }

  // I have been the target of the packet, thus setting it to null
  Packet::free(p);
  p = NULL;
}

// is called by HLS when we receive a circlecast request with our
// node as destination. This method is just responsible for 
// the forwarding of the packet, the answers will be generated in
// the method "circlecastRequestOnMacReceived()"
// a) because we have been directly addressed
// b) there is not better way and the packet should be dropped.
// In case a), we check if we are in the correct cell, if so, we
// send the packet to the next cell in the list, if not, we just 
// give it back.
// In case b) we send it directly to the next cell.
void BasicRequestProcessor::recvCirclecastRequest(Packet* &p, 
						  bool fromDropCallback)
{
  struct hdr_hls *hlsh = HDR_HLS(p);
  struct hdr_ip *iph = HDR_IP(p);
 
  position myPos;
  hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
  
  // we are in the correct cell or the cell couldn't be 
  // reached by the routing:
  if(sameCell(&myPos, &hlsh->cell.pos, hls_->cellbuilder_ ) ||
     fromDropCallback ||
     iph->ttl() <= 0)
    // the ttl thing is necessary because we also get a dropPacketCallback
    // from a TTL expiration: 
    // the result would be that we do a HLS_CIC_f in the else branch,
    // the TTL is negative (and is only dropped if TTL == 0), thus it
    // loops until eternity...
    {
      // next cell
      hlsh->neighborIndex++;  
      int nextCell = NO_CELL;
      if(hlsh->neighborIndex < NUMBER_OF_NEIGHBOR_CELLS)
	{
	  nextCell = hlsh->neighbors[hlsh->neighborIndex];
	}
      // there is no next cell
      if(nextCell == -1)
	{
	  if(hls_verbose)
	    {
	      // HLS Circle cast drop
	      hls_->trace("HLS_CIC_d %.12f (%d_%d) %d ->...->%d", 
			  Scheduler::instance().clock(), // timestamp
			  hlsh->reqid.node,              // the unique...
			  hlsh->reqid.nr,                // id of the request
			  hls_->addr(),                  // my address 
			  hlsh->dst.id);                 // target address
	    }
	  Packet::free(p);
	  p = NULL;
	  return;
	}
      else
	{
	  position cellPosition 
	    = hls_->cellbuilder_->getPosition(nextCell);

	  iph->dx_ = cellPosition.x;
	  iph->dy_ = cellPosition.y;
	  iph->dz_ = cellPosition.z;
	  iph->ttl() = 10;

	  // set the cell field
	  hlsh->cell.init();
	  hlsh->cell.id    = nextCell;
	  hlsh->cell.level = -1;
	  hlsh->cell.pos.x = cellPosition.x;
	  hlsh->cell.pos.y = cellPosition.y;
	  hlsh->cell.pos.z = cellPosition.z;

	  hls_->parent->recv(p, NULL);
	  p = NULL;
	}
    }
  else
    {
      if(hls_verbose)
	{
	  // HLS Circle cast next cell
	  hls_->trace("HLS_CIC_f %.12f (%d_%d) %d ->...->%d <%d>", 
		      Scheduler::instance().clock(), // timestamp
		      hlsh->reqid.node,              // the unique...
		      hlsh->reqid.nr,                // id of the request
		      hls_->addr(),                  // my address 
		      hlsh->dst.id,                  // target address
		      hlsh->neighbors[hlsh->neighborIndex]); // traget cell
	}
    }
}


void BasicRequestProcessor::circlecastRequestOnMacReceived(const Packet* p)
{
 
  struct hdr_hls *hlsh = HDR_HLS(p);

  // just look in the active entries (that means only the real 
  // location servers will answer the request)
  nodeposition* nodepos = hls_->findActiveEntry(hlsh->dst.id);
  
  if(nodepos != NULL)
    {
      double timeout;
      if(hlsh->dst.id == hls_->addr())
	{
	  // optimization:
	  // if our own node is requested, we answer directly 
	  // without waiting
	  sendCellcastReply(p, nodepos);
	}
      else
	{	  
	  // first we go through the list and check if we have already set up
	  // a timer for this request. If so, we won't schedule another one
	  AnswerTimer* list = answerTimerList_;
	  while(list != NULL)
	    {
	      // this comparison should be based on other parameters than the 
	      // request ID (the circlecast request does not contain this info
	      // according to the hdr_size function). Sender of the circlecast
	      // and target node should function with the same success (this
	      // also identifies a request, a circlecast sender would not send
	      // a circlecast packet twice but wait for an answer)
	      if(list->reqId_.equals(&hlsh->reqid))
		{
		  // don't schedule a timer because there is already one
		  return;
		}
	      list = list->next;
	    }
	  // we haven't had a timer, thus schedule an answer
	  
	  timeout = Random::uniform(CELLCAST_ANSWER_JITTER);
	  // schedule the timer to send our answer if we haven't heard another
	  // one answering in the meantime
	  CirclecastAnswerTimer* tmpTimer = new CirclecastAnswerTimer(this, timeout, p,
								  nodepos);
	  // put the timer in the list
	  tmpTimer->next = answerTimerList_;
	  answerTimerList_ = tmpTimer;
	}
    }
}


// sends a request packet to the first cell in the cell list
// When it arrives there, it will be forwarded to the next 
// cell until it has visited all cells in the list.
// When a node in radiorange receives the request (he listens on
// the mac) and knows something about the position of the requested
// node, it'll send a cellcast answer to the node which initiated
// the circel request.
void BasicRequestProcessor::sendCirclecastRequest(int nodeid, request_id reqID, 
						  int* cells)
{
  Packet *p = hls_->allocpkt();
  
  struct hdr_hls *hlsh = HDR_HLS(p);
  struct hdr_ip *iph = HDR_IP(p);
  struct hdr_cmn *cmnh = HDR_CMN(p);
  struct hdr_gpsr *gpsrh = HDR_GPSR(p);
  struct hdr_greedy *greedyh = HDR_GREEDY(p);
  struct hdr_gopher *gopherh = HDR_GOPHER(p);
 
  iph->sport() = RT_PORT;
  iph->dport() = RT_PORT;
  iph->saddr() = hls_->addr(); // I am the originator of the packet
  iph->daddr() = NO_NODE;
  iph->ttl()   = HLS_TTL;

  position cellPosition = hls_->cellbuilder_->getPosition(cells[0]);

  // TRACE
  if(hls_verbose)
    {
      // HLS Request circle cast send
      hls_->trace("HLS_CIC_s %.12f (%d_%d) %d ->%d <%d,%d,%d,%d,%d,%d,%d,%d>", 
		  Scheduler::instance().clock(), // timestamp
		  reqID.node,                    // address of requestor
		  reqID.nr,                      // local number of the request
		  hls_->addr(),                  // my address
		  nodeid,                        // target node
		  cells[0],                      // destination cells
		  cells[1],
		  cells[2],
		  cells[3],
		  cells[4],
		  cells[5],
		  cells[6],
		  cells[7]);
    }


  // Let GPSR route this pkt to the center of the responsible cell
  iph->dx_ = cellPosition.x;
  iph->dy_ = cellPosition.y;
  iph->dz_ = cellPosition.z;

  cmnh->ptype() = PT_HLS;
  cmnh->addr_type_ = NS_AF_INET;
  cmnh->num_forwards() = 0;
  cmnh->next_hop_ = NO_NODE;
  cmnh->direction() = hdr_cmn::DOWN;
  cmnh->xmit_failure_ = 0;

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // out node is the source of the packet
  nodeposition tmpPos = hls_->getNodeInfo();
  hlsh->src.copyFrom(&tmpPos);

  hlsh->type_ = HLS_CIRCLECAST_REQUEST;

  // set the cell field
  hlsh->cell.init();
  hlsh->cell.id    = cells[0];
  hlsh->cell.level = -1;
  hlsh->cell.pos.x = cellPosition.x;
  hlsh->cell.pos.y = cellPosition.y;
  hlsh->cell.pos.z = cellPosition.z;
  
  // put the id of the requested node in the packet
  hlsh->dst.id = nodeid;

  // the unique id for the request
  hlsh->reqid  = reqID;

  // we are the sender of the circelcast request
  hlsh->cellcastRequestSender = hls_->addr();

  // put the list of cells which we have to reach in the packet
  for(int i=0;i<NUMBER_OF_NEIGHBOR_CELLS;i++)
    {
      hlsh->neighbors[i] = cells[i];       
    }
  // the first cell in the list will be the fist target
  hlsh->neighborIndex = 0;
  
  // call the recv function of the node to give it to the routing
  hls_->parent->recv(p, NULL);  
}


// called when the routing hasn't been able to forward the request
// to the responsible cell or when a cellcast failed (due to a timeout
// of the cellcast). We have to direct the request
// to the cell on the next level, trace the complete failure of
// the request when we are already on the highest level or send a
// circle cast request
void BasicRequestProcessor::processRequestUnreachableCell(Packet* &p_old)
{
  struct hdr_hls *hlsh_old = HDR_HLS(p_old);

  if(hls_verbose)
    { 
      position pos;
      hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z);

      // HLS Request unreachable (cell)
      hls_->trace("HLS_REQ_u %.12f (%d_%d) %d (%.2f %.2f) <%d %.2f %.2f (%d)>", 
		  Scheduler::instance().clock(), // timestamp
		  hlsh_old->reqid.node,              // the unique...
		  hlsh_old->reqid.nr,                // id of the request	 
		  hls_->addr(),                  // my address 
		  pos.x,                         // my position
		  pos.y,
		  hlsh_old->cell.id,                 // id of target cell
		  hlsh_old->cell.pos.x,              // position ...
		  hlsh_old->cell.pos.y,              // of the cell
		  hlsh_old->cell.level);             // level of the cell 
    }

  if((hlsh_old->status_ == REQUEST_LOCATED) &&
     ((Scheduler::instance().clock() - hlsh_old->dst.ts) < HLS_MAX_CACHE_LOOKUP_AGE))
    {
      position pos;
      hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z);
      struct hdr_hls *hlsh = HDR_HLS(p_old);
      hlsh_old->status_ = REQUEST_LOCATED_AND_RETRIED;
      // determine the distance between me and the last known location of the
      // node
      double deltaX = pos.x - hlsh->dst.pos.x;
      double deltaY = pos.y - hlsh->dst.pos.y;
      double totalDistance = sqrt((deltaX*deltaX)+(deltaY*deltaY));
      if(totalDistance < God::instance()->getRadioRange())
	{
	  if(hls_verbose)
	    {
	      // HLS Request forced forward
	      hls_->trace("HLS_RQ_ff %.12f (%d_%d) %d ->...->%d", 
			  Scheduler::instance().clock(), // timestamp
			  hlsh_old->reqid.node,              // the unique...
			  hlsh_old->reqid.nr,                // id of the request
			  hls_->addr(),                  // my address 
			  hlsh_old->dst.id);                 // target address
	    }
	  // if we are in radiorange of the last known position, 
	  // there is a high possibility that also
	  // the target of the request is in our radiorange but we don't 
	  // know it (e.g. because the beacons were lost). Thus we try to
	  // send it "manually" without using the routing agent.
	  struct hdr_ip *iph = HDR_IP(p_old);	  
	  struct hdr_cmn *cmh = HDR_CMN(p_old);
	  iph->daddr() = hlsh_old->dst.id;
	  iph->ttl() = 1; // drop packet if we don't reach it on the next hop
	  iph->dx_ = hlsh_old->dst.pos.x;
	  iph->dy_ = hlsh_old->dst.pos.y;
	  iph->dz_ = hlsh_old->dst.pos.z;

	  cmh->direction() = hdr_cmn::DOWN;
	  cmh->next_hop_ = hlsh_old->dst.id;
	  // give the packet directly to the MAC layer
	  hls_->target_->recv(p_old, (Handler *)0);
	  return;
	}
      // if we aren't in radiorange, the packet will be dropped as LNR
    }

  if(hlsh_old->status_ == REQUEST_LOCATED_AND_RETRIED)
    {
      TRACE_CONN(p_old,hls_->addr(),hls_->addr(),hlsh_old->dst.id);
      // we have already located the position and the information is young
      // enough to be accurate
      dropAndTraceRequestPacket(p_old, LOCATED_AND_NO_ROUTE);
      return;
    }

  // determine the RC for the given node
  position pos;
  hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z);
  int level = hlsh_old->cell.level + 1;
  int targetNode = hlsh_old->dst.id;

  int responsibleCell = hls_->cellbuilder_->getRC(targetNode, level, pos);

  if(responsibleCell == NO_VALID_LEVEL)
    {
      // prepare the circlecast request
      int number;
      int* neighbors;
      if(hlsh_old->circleCastAlreadySend == true)
	{
	  dropAndTraceRequestPacket(p_old, CIRCELCAST_ALREADY_SEND);
	  return;
	}
      hls_->cellbuilder_->getNeighborCells(hlsh_old->cell.id, &number, &neighbors);
      
      // we still have to put the neighbor cells in a better order
      int myCell = hls_->cellbuilder_->getCellID(pos);
      int index = -1;
      for(int i=0;i<number;i++)
	{
	  if(neighbors[i] == myCell)
	    {
	      index = i;
	      break;
	    }
	}
      
      if(index != -1)
	{
	  int* tmp = new int[number];
	  int counter = 0;
	  // copy the middle part
	  for(int i=index;i<number;i++)
	    {
	      if(neighbors[i] != -1)
		{
		  tmp[counter] = neighbors[i];
		  counter++;
		}
	    }
	  // copy the lower part
	  for(int i=0;i<index;i++)
	    {
	      tmp[counter] = neighbors[i];
	      counter++;
	    }

	  // fill the rest with NO_CELL
	  while(counter < number)
	    {
	      tmp[counter] = NO_CELL;
	      counter++;
	    }
	  free(neighbors);
	  neighbors = tmp;
	}
      // end of putting the neighbors in a better order

      circlecastRequestCache_->store(p_old, CIRCLECAST_ENTRY_TIMEOUT);
      sendCirclecastRequest(targetNode, hlsh_old->reqid, neighbors);
      hlsh_old->circleCastAlreadySend = true;
      free(neighbors);
      return;
    }


  // produce a new packet which will be sent to the next-level RC
  // no reuse of the old one because of possible problems with the packetnumber
  // e.g. it arrives at a router where it has already been, the router 
  // thinks it's a routing loop and throws the packet away
  Packet *p_next_level = hls_->allocpkt();
  
  struct hdr_hls *hlsh = HDR_HLS(p_next_level);
  struct hdr_ip *iph = HDR_IP(p_next_level);
  struct hdr_cmn *cmnh = HDR_CMN(p_next_level);
  struct hdr_gpsr *gpsrh = HDR_GPSR(p_next_level);
  struct hdr_greedy *greedyh = HDR_GREEDY(p_next_level);
  struct hdr_gopher *gopherh = HDR_GOPHER(p_next_level);
 
  iph->sport() = RT_PORT;
  iph->dport() = RT_PORT;
  iph->saddr() = hls_->addr(); // I am the originator of the packet
  iph->daddr() = NO_NODE;
  
  struct request_id reqID = hlsh_old->reqid;

  position cellPosition = hls_->cellbuilder_->getPosition(responsibleCell);

  // TRACE
  if(hls_verbose)
    {
      // HLS Request next (level)
      hls_->trace("HLS_REQ_n %.12f (%d_%d) %d ->%d <%d %.2f %.2f (%d)>", 
		  Scheduler::instance().clock(), // timestamp
		  reqID.node,                    // address of requestor
		  reqID.nr,                      // local number of the request
		  hls_->addr(),                  // my address
		  hlsh_old->dst.id,              // target node
		  responsibleCell,               // destination cell
		  cellPosition.x,                // x and y 
		  cellPosition.y,                // of the center of the cell
		  level);                        // level of the cell
    }


  // Let GPSR route this pkt to the center of the responsible cell
  iph->dx_ = cellPosition.x;
  iph->dy_ = cellPosition.y;
  iph->dz_ = cellPosition.z;
  iph->ttl() = level * HLS_REQUEST_TTL_PER_LEVEL;

  cmnh->ptype() = PT_HLS;
  cmnh->addr_type_ = NS_AF_INET;
  cmnh->num_forwards() = 0;
  cmnh->next_hop_ = NO_NODE;
  cmnh->direction() = hdr_cmn::DOWN;
  cmnh->xmit_failure_ = 0;

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // put the info from the old packet in the new one
  hlsh->src.copyFrom(&hlsh_old->src);

  hlsh->type_ = HLS_REQUEST;

  // set the cell field
  hlsh->cell.init();
  hlsh->cell.id    = responsibleCell;
  hlsh->cell.level = level;
  hlsh->cell.pos.x = cellPosition.x;
  hlsh->cell.pos.y = cellPosition.y;
  hlsh->cell.pos.z = cellPosition.z;
  
  // put the id of the requested node in the packet
  hlsh->dst.id = hlsh_old->dst.id;

  // the unique id for the request
  hlsh->reqid  = reqID;

  // call the recv function of the node to give it to the routing
  hls_->parent->recv(p_next_level, NULL);

  Packet::free(p_old);
  return;
}

void BasicRequestProcessor::cellcastTimerCallback(Packet* p)
{
  if(hls_verbose)
    {
      // HLS CELLCAST timeout
      hls_->trace("HLS_CC_to %.12f (%d_%d) %d -*%d", 
		  Scheduler::instance().clock(), // timestamp
		  HDR_HLS(p)->reqid.node,        // the unique...
		  HDR_HLS(p)->reqid.nr,          // id of the request
		  hls_->addr(),                  // my address 
		  HDR_HLS(p)->dst.id);           // target address
    }
  processRequestUnreachableCell(p);
}

// when this method is called it means that also the circlecast
// has failed:
// thus we drop the request and declare it to have failed
void BasicRequestProcessor::dropAndTraceRequestPacket(Packet* p, char *reason)
{
  // after returning, the packet will be dropped and freed
  if(hls_verbose)
    { 
      position pos;
      hls_->mn_->getLoc(&pos.x, &pos.y, &pos.z);
      struct hdr_hls *hlsh = HDR_HLS(p);
      // HLS Request drop 
      // idev = information deviation, how old is our info
      // mydev = how far the dropping node is away from the (actual pos)
      //         of the target
      hls_->trace("HLS_REQ_d %.12f (%d_%d) %d (%d) %s idev %.2f mydev %.2f metonode %.2f", 
		  Scheduler::instance().clock(), // timestamp
		  hlsh->reqid.node,        // the unique...
		  hlsh->reqid.nr,          // id of the request
		  hls_->addr(),                  // my address 	  
		  hlsh->cell.level,        // the level on which we're 
		  reason,
		  // distance between the requested node and the info
		  distance(hlsh->dst.id, hlsh->dst.pos),
		  // distance between us and the position of the info
		  distance(hls_->addr(), hlsh->dst.pos),
		  // distance between me and the node
		  distance(hlsh->dst.id, pos));
      // dropping
    }  
  Packet::free(p);
}

// BHM BasicHandoverManager //////////////////////////////////////
BasicHandoverManager::BasicHandoverManager(HLSLocationCache* activeEntries, 
					   HLSLocationCache* passiveEntries,
					   HLSLocationCache* outOfCellEntries,
					   HLS* hls,
					   Cellbuilder* cellbuilder)
  : HandoverManager(activeEntries, passiveEntries, outOfCellEntries, 
		    hls, cellbuilder)
{}

// will be called whenever HLS receives a handover packet.
// (also if we are not in the correct cell!)
// if the forceActiveSave flag is set, it means that we MUST put the
// information in the packet in the activeEntries (normally because
// there is no better node for the routing than our node and the
// packet will otherwise be dropped)
void BasicHandoverManager::recv(Packet* &p, bool forceActiveSave)
{
  hdr_hls* hlsh = HDR_HLS(p);

  position destPos;
  position myPos;
  // getting my actual position
  hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
  // getting the destination of the update package
  destPos = hlsh->cell.pos;
  // test
  if(hlsh->src.targetcell == hls_->getCell() && 
     !sameCell(&destPos, &myPos, hls_->cellbuilder_))
    {
      printf("### holy shit! Method sameCell doesn't calculate correctly\n");
      exit(-1);
    }
  // endtest
  if(sameCell(&destPos, &myPos, hls_->cellbuilder_))
    {
      if(hls_trace_handovers)
	{
	  int myCell = hls_->cellbuilder_->getCellID(myPos);
	  // HLS Handover real receive
	  hls_->trace("HLS_H_rr  %.12f %d<-%d [%d] [%.2f %.2f <%d>] <%d %.2f %.2f>", 
		      Scheduler::instance().clock(), // timestamp
		      hls_->addr(),                  // address of update receiver
		      HDR_IP(p)->saddr(),            // address of the previous info owner
		      hlsh->src.id,                  // the node-if of the information
		      myPos.x,                       // my...
		      myPos.y,                       // coordinates
		      myCell,                        // my cell
		      hlsh->cell.id,                 // target cell
		      destPos.x,                     // center...
		      destPos.y);                    // of the target cell	   
	}
      
      
      activeEntries_->add(&hlsh->src);
      // we have been in the cell, thus we can free the packet.
      // this will tell gpsr that the packet arrived
      Packet::free(p);
      p = NULL;
      return;
    }
  
  if(forceActiveSave)
    {
      if(hls_trace_handovers)
	{
	  int myCell = hls_->cellbuilder_->getCellID(myPos);
	  // HLS Handover forced receive
	  hls_->trace("HLS_H_fr  %.12f %d<-%d [%d] [%.2f %.2f <%d>] <%d %.2f %.2f>", 
		      Scheduler::instance().clock(), // timestamp
		      hls_->addr(),                  // address of update receiver
		      HDR_IP(p)->saddr(),            // address of the previous info owner
		      hlsh->src.id,                  // address of update sender
		      myPos.x,                       // my...
		      myPos.y,                       // coordinates
		      myCell,                        // my cell
		      hlsh->cell.id,                 // target cell
		      destPos.x,                     // center...
		      destPos.y);                    // of the target cell	      
	}
      
      outOfCellEntries_->add(&hlsh->src);
      // we have been in the cell, thus we can free the packet.
      // this will tell gpsr that the packet arrived
      Packet::free(p);
      p = NULL;
      return;
    }

#ifdef AGGRESSIVE_CACHING
  // we are not in the same cell and no forced 
  // saving, thus just store in the passive entries
  passiveEntries_->add(&hlsh->src);
#endif
} // end of BHM::recv(..)

void BasicHandoverManager::timerCallback()
{
  int actualCell = hls_->getCell();
  if(actualCell != lastCell)
    {
      // the entries in activeEntries will only be checked if
      // we really changed our cell since the last timerCallback
      checkCacheAndTransmit(activeEntries_, actualCell);
    }		
  // the entries of which we know that they aren't in the correct cell
  // must be checked
  checkCacheAndTransmit(outOfCellEntries_, actualCell);

  lastCell = actualCell;
}


// gets the table of the cache, checks if the entries belong to the 
// cell actualCell and if not, construct a packet and try to 
// send it to the destination cell
void BasicHandoverManager::checkCacheAndTransmit(HLSLocationCache* cache, int actualCell)
{
  // we have changed the cell, thus we must search the activeEntries
  // for information which must be handed over to our old cell
  unsigned int size;
  CHCEntry** table = cache->getTable(&size);

  int arraysize = 20;
  int index = 0;
  nodeposition *tmparray 
    = (nodeposition *)calloc(arraysize, sizeof(nodeposition));
  if(tmparray == NULL)
    {
      printf("out of memory in BasicHandoverManager::checkCacheAndTransmit\n");
      exit(-1);
    }
				  // = new nodeposition[arraysize];
  // end workaround declaration (more code inloop)


  // search in activeEntries for Entries that do not belong in actualCell

  // Walk through Table
  for (unsigned int i=0; i<size; i++) {
    if (table[i] != NULL) {
      CHCEntry* tmp = table[i];
      while (tmp != NULL) 
	{	  
	  nodeposition* tmpinfo = (nodeposition*)tmp->info;
	  
	  if(tmpinfo->targetcell != actualCell)
	    {
	      // we have to copy the info to a tmp variable:
	      // if we would handover the info and can't find a better node
	      // than our one, the routing will hand the packet pack to 
	      // the locservice to put it in activeEntries. There, it
	      // will be stored, an afterwards deleted from us because
	      // we thought to have successfully handed over the info ...
	      // to avoid this, copy the info, delete it from activeEntries
	      // and then hand it over (possibly to ourself)

	      // deletion will be done after the for-loop
	      tmparray[index].copyFrom(tmpinfo);
	      index++;
	      if(index >= arraysize)
		{
		  // we have to resize the array
		  arraysize = arraysize*2;
		  nodeposition *tmp
		    = (nodeposition *)realloc(tmparray, 
					      sizeof(nodeposition)*arraysize);
		    if(tmp == NULL)
		      {
			printf("out of memory in BHM:checkCacheAndTransmit\n");
			exit(-1);
		      }
		    tmparray = tmp;
		  printf("### resize tmparray in checkCacheAndTransmit node %d new size %d\n", hls_->addr(), arraysize);
		}
	    }		
	  tmp = tmp->next;
	}
    } // end of while
  } // end of for
  
  for(int i=0;i<index;i++)
    {
      // we delete the information here from the cache to avoid
      // conflicts in the loop over the table of the cache
      cache->remove(tmparray[i].id);
      handoverInformation(&tmparray[i]);
    }
  free(tmparray);
} // end of BHM::checkCacheAndTransmit

// this function is responsible for handing over the information
// it constructs a packet and sends it to the target cell
// (to the cell where the information should be)
void BasicHandoverManager::handoverInformation(nodeposition* info)
{  
  Packet *pkt = hls_->allocpkt();

  struct hdr_ip *iph = HDR_IP(pkt);
  struct hdr_cmn *cmnh = HDR_CMN(pkt);
  struct hdr_gpsr *gpsrh = HDR_GPSR(pkt);
  struct hdr_greedy *greedyh = HDR_GREEDY(pkt);
  struct hdr_gopher *gopherh = HDR_GOPHER(pkt);
  struct hdr_hls *hlsh = HDR_HLS(pkt);

  
  iph->sport() = RT_PORT;
  iph->dport() = RT_PORT;
  iph->saddr() = hls_->parent->addr();
  iph->daddr() = NO_NODE;
  iph->ttl()   = HLS_TTL;

  position cellPosition = cellbuilder_->getPosition(info->targetcell);
  // Let GPSR route this pkt to the center of the responsible cell
  // (or to the closest position possible)
  iph->dx_ = cellPosition.x;
  iph->dy_ = cellPosition.y;
  iph->dz_ = cellPosition.z;

  cmnh->ptype() = PT_HLS;
  cmnh->addr_type_ = NS_AF_INET;
  cmnh->num_forwards() = 0;
  cmnh->next_hop_ = NO_NODE;
  cmnh->direction() = hdr_cmn::DOWN;
  cmnh->xmit_failure_ = 0;

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // init my own header
  hlsh->init();
  // now we have to put the actual position information into the
  // packet (together with a timestamp)
  hlsh->src.copyFrom(info);

  hlsh->type_ = HLS_HANDOVER;

  // set the cell field
  hlsh->cell.id    = info->targetcell;
  hlsh->cell.level = -1; //don't know the level in  the handover
  hlsh->cell.pos.x = cellPosition.x;
  hlsh->cell.pos.y = cellPosition.y;
  hlsh->cell.pos.z = cellPosition.z;

  if(hls_trace_handovers)
    {
      int myCell = hls_->getCell();
      position cellPos = cellbuilder_->getPosition(myCell);

      // HLS Handover send
      hls_->trace("HLS_H_s   %.12f %d <%d %.2f %.2f> [%d %.4f %.2f %.2f] -> <%d %.2f %.2f>", 
		  Scheduler::instance().clock(), // timestamp
		  hls_->addr(),                  // address of handover sender
		  myCell,                        // my cell
		  cellPos.x,                     // my cell's position x ...
		  cellPos.y,                     // and y
		  hlsh->src.id,                  // information to handover
		  hlsh->src.ts,                  // the info's ts
		  hlsh->src.pos.x,               // and the ...
		  hlsh->src.pos.y,               // position
		  info->targetcell,              // target cell
		  cellPosition.x,                // the  
		  cellPosition.y);               // coordinates of the cell      
    }

  hls_->parent->recv(pkt, NULL);  
} // end of BHM::handoverInformation(...)


AnswerTimer::AnswerTimer(BasicRequestProcessor* brp, double timeout,
			 nodeposition* information)  
{
  brp_ = brp;
  info_.copyFrom(information);
  sched(timeout);
  next = NULL;
}

void AnswerTimer::expire(Event* e)
{}

AnswerTimer::~AnswerTimer()
{}

hdr_hls* AnswerTimer::getHlsHeaderOfPacket()
{
  return NULL;
}

// CellcastAnswerTimer /////////////////////////////////////
CellcastAnswerTimer::CellcastAnswerTimer(BasicRequestProcessor* brp, double timeout,
					 Packet* p, nodeposition* information)  
  : AnswerTimer(brp, timeout, information)
{
  packet_ = p;
  reqId_.set(HDR_HLS(p)->reqid);
}

CellcastAnswerTimer::~CellcastAnswerTimer()
{
  Packet::free(packet_);      
}

void CellcastAnswerTimer::expire(Event* e)
{
  brp_->sendCellcastReply(packet_, &info_);
  //Packet::free(packet_);
}

hdr_hls* CellcastAnswerTimer::getHlsHeaderOfPacket()
{
  return HDR_HLS(packet_);
}

// end of cellcastAnswerTimer

// CirclecastAnswerTimer /////////////////////////////////////
CirclecastAnswerTimer::CirclecastAnswerTimer(BasicRequestProcessor* brp, double timeout,
					     const Packet* p, nodeposition* information)  
  : AnswerTimer(brp, timeout, information)
{
  packet_ = p->copy();
  reqId_.set(HDR_HLS(p)->reqid);
}

CirclecastAnswerTimer::~CirclecastAnswerTimer()
{
  Packet::free(packet_);        
}

void CirclecastAnswerTimer::expire(Event* e)
{
  brp_->sendCellcastReply(packet_, &info_);
}

hdr_hls* CirclecastAnswerTimer::getHlsHeaderOfPacket()
{
  return HDR_HLS(packet_);
}


// StopAndGoTimerHandler ///////////////////////////////////
// The stopandgotimerhandler isn't really needed any more, it was
// used in an old implementation of the cellcast stuff.
// But it's a nice class, perhaps we need it later, thus keep
// it ;-)
StopAndGoTimerHandler::StopAndGoTimerHandler(double intervall)
{
  counter_ = 0;
  intervall_ = intervall;
}

void StopAndGoTimerHandler::incrementCounter()
{  
  if(counter_ > 0)
    {
      counter_++;
    }
  else
    {
      resched(intervall_);
      counter_ = 1;
    }
}

void StopAndGoTimerHandler::decrementCounter()
{
  counter_--;
  if(counter_ < 0)
    {
      printf("### error in StopAndGoTimerHandler:decrementCounter() counter_ negative\n");
      exit(-1);
    }
  if((counter_ == 0) && (status() == TIMER_PENDING))
    {
      // cancel the pending timer because there is no other event
      cancel();
    }
}

// this is called when the timer expires. This method will also decrement
// the counter and will only reschedule the timer if the counter is 
// greated than 0
void StopAndGoTimerHandler::expire(Event* e)
{
  // for calling the child classes' expire method
  this->stopAndGoTimerExpired();

  if(counter_ > 0)
    {
      resched(intervall_);
    }
}

// BUSTimer ////////////////////////////////////////////////
BUSTimer::BUSTimer(BasicUpdateSender* bus, double intervall)
{
  this->bus_ = bus;
  this->intervall_ = intervall;
}

void BUSTimer::expire(Event* e)
{
  bus_->timerExpired();
  double delay = Random::uniform(HLS_UPDATE_JITTER);
  resched(intervall_ + delay); // reschedule the timer after the intervall
}

////////////////////////
// Location Cache //////
////////////////////////

bool HLSLocationCache::add(nodeposition* info) {
    return (ChainedHashCache::add(info->id, (void*)(info)));
}

bool HLSLocationCache::update(nodeposition* info) {
    return (ChainedHashCache::update(info->id, (void*)(info)));
}

bool HLSLocationCache::iupdate (void* ventry, void* vdata) {
    if (((nodeposition*)ventry)->ts < ((nodeposition*)vdata)->ts) {
	memcpy(ventry,vdata,sizeof(nodeposition));
	return true;
    }
    return false;
}

bool HLSLocationCache::itimeout (void* ventry) {
  if ((Scheduler::instance().clock() - ((nodeposition*)ventry)->ts) > lifetime) 
    { 
      return true; 
    }

  
  //}
  return false;
}

void HLSLocationCache::iprint (void* ventry) {
    nodeposition* entry = (nodeposition*)ventry;
    printf("(%d), %.4f",
	   entry->id,
	   entry->ts);
}

// end of HLSLocationCache

// RequestCache //////////////////////////////////////////////////////
// For the cellcast requests and the circlecast requests /////////////
RequestCache::RequestCache(int initialCacheSize,
			   BasicRequestProcessor* requestProcessor)
{
  packetCache = new Packet*[initialCacheSize];
  timerCache  = new RequestCacheTimer*[initialCacheSize];
  actualCacheSize = initialCacheSize;
  brp_ = requestProcessor;

  for(int i=0;i<actualCacheSize;i++)
    {
      packetCache[i] = NULL;
      timerCache[i] = new RequestCacheTimer(this, i);
    }
}

RequestCache::~RequestCache()
{
  for(int i=0;i<actualCacheSize;i++)
    {
      if(packetCache[i] != NULL)
	{
	  Packet::free(packetCache[i]);
	}
      delete(timerCache[i]);
    }  
}


// when this function is called, it means that the timer for
// the packet at the given position has expired.
void RequestCache::timerCallback(int number)
{
  Packet* tmp = packetCache[number];
  packetCache[number] = NULL;
  timerExpired(tmp);
}

// this is the method which must be reimplemented by the child 
// classes
void RequestCache::timerExpired(Packet* p)
{}



// stores the packet in the cache and sets the lifetime
void RequestCache::store(Packet* p, double lifetime)
{
  int index = getFreeSlot();
  packetCache[index] = p;
  timerCache[index]->sched(lifetime);
}



Packet* RequestCache::getRequest(request_id reqid)
{
  // loop through the cache
  for(int i=0;i<actualCacheSize;i++)
    {
      // there is an entry ...
      if(packetCache[i] != NULL)
	{
	  // ... with the same request id
	  if(reqid.equals(&HDR_HLS(packetCache[i])->reqid))
	    {
	      // take the packet from the cache and initialize
	      // the slot
	      Packet* tmp = packetCache[i];
	      packetCache[i] = NULL;
	      timerCache[i]->cancel();
	      return tmp;	      
	    }
	}
    }
  // we haven't found anything, thus we return NULL
  return NULL;
}

int RequestCache::getFreeSlot()
{
  
  for(int i=0;i<actualCacheSize;i++)
    {
      if(packetCache[i] == NULL)
	{
	  return i;
	}
    }
  // we arrive here, that means no free space was found,
  // thus we double the space of the caches
  int nextFreeSlot = actualCacheSize;
  actualCacheSize = actualCacheSize * 2;
  Packet** tmpPacket = (Packet**) realloc(packetCache, 
					  sizeof(Packet*)*actualCacheSize);
  RequestCacheTimer** tmpTimer
    = (RequestCacheTimer**) realloc(timerCache, 
				    sizeof(RequestCacheTimer)*actualCacheSize);
  
  if((tmpPacket == NULL) || (tmpTimer == NULL))
    {
      printf("out of memory in RequestCache::getFreeSlot\n");
      exit(-1);
    }
  packetCache = tmpPacket;
  timerCache  = tmpTimer;
  // initialize the caches
  for(int i=nextFreeSlot;i<actualCacheSize;i++)
    {
      packetCache[i] = NULL;
      timerCache[i] = new RequestCacheTimer(this, i);
    }
  
  return nextFreeSlot;
}

// RequestCacheTimer ////////////////////////////////////////////////
RequestCacheTimer::RequestCacheTimer(RequestCache* cache, int number)
{
  cache_ = cache;
  this->number = number;
}

void RequestCacheTimer::expire(Event* e)
{
  cache_->timerCallback(number);
}


//////////////////////////////////////////////////////////////////////
// cellcastRequestCache //////////////////////////////////////////////
CellcastRequestCache::CellcastRequestCache(int initialCacheSize,
		       BasicRequestProcessor* requestProcessor)
  : RequestCache(initialCacheSize, requestProcessor)
{}

void CellcastRequestCache::timerExpired(Packet* p)
{
  // telling the brp that the timer for that packet has
  // expired
  brp_->cellcastTimerCallback(p);
}
//////////////////////////////////////////////////////////////////////
// circlecastRequestCache ////////////////////////////////////////////
CirclecastRequestCache::CirclecastRequestCache(int initialCacheSize,
					       BasicRequestProcessor* requestProcessor)
  : RequestCache(initialCacheSize, requestProcessor)
{}

void CirclecastRequestCache::timerExpired(Packet* p)
{  
  // telling the brp that the timer for that packet has
  // expired
  brp_->dropAndTraceRequestPacket(p,CIRCLECAST_TIMEOUT);
}

//////////////////////////////////////////////////////////////////////
// globale helper functions
// returns true if pos1 and pos2 are member of the same cell
bool sameCell(position* pos1, position* pos2, Cellbuilder* cellbuilder)
{
  int cell1, cell2;
  cell1 = cellbuilder->getCellID(*pos1);
  cell2 = cellbuilder->getCellID(*pos2);  
  if(cell1 == cell2)
    {
      return true;
    }
  else
    {
      return false;
    }
}

// init static variable for hdr_hls::offset_
int hdr_hls::offset_;
