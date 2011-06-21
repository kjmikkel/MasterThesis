/*
 * File: Code for a new Location Service for the ns
 *       network simulator
 * Author : Wolfgang Kiess


   architecture:
     The features of Location Services
       - send PosInfos
       - receive PosInfos
       - receive and save PosInfos
       - doing Positionrequests
       - receive und answer Positionrequests are implemented in individual classes
     Therefore a compact interface must be implemented. (across a parentclass with virtual functions). So you can implement a very simple version first, where components can be enhanced and exchanged easily.

     Baseversion of the components:
     Sending of PosInfos : always after a given timeintervall
     Forwarding of PosInfos : no special functionality
     receive and save PosInfos: as described with a compact maximum number of spaces.
     Positionrequests : to the homecell on level 1
     receive and answer Positionrequests: Routing to the cell, 
		first node of the cell answers or asks the cell on next level;
		if there is no node in the cell reachable, the packet gets 
		forwarded to the cell on the next level.

     TODO :
     - the parameter reqid is abused to determine if a node has already
       answered a cellcast or circlecast. This parameter was just
       introduced to enable a better tracing 
 * 
 */


#include "hls_basic.h"
#include "mac.h"
#include "quadratic.h"

// just for broadcast
#include "../gpsr/gpsr.h"
#include "../greedy/greedy.h"
#include "../gopher/gopher.h"


HLS::HLS(Agent* p)
  : LocationService(p)
{
  parent = p;
}

void HLS::init()
{
  // radiorange, x and y required 
  double maxx = mn_->T_->upperX();
  double maxy = mn_->T_->upperY();

  double rrange = God::instance()->getRadioRange();


  reqtable_ = new LSRequestCache(this,HLS_REQTABLE_SIZE,HLS_REQTABLE_TIMEOUT);

  cellbuilder_ = new QuadraticCellbuilder(rrange, maxx, maxy);
  updateSender_ = new BasicUpdateSender(cellbuilder_, this);

  activeEntries_ = new HLSLocationCache(this, HLS_ACTIVE_ENTRIES_SIZE, ENTRY_LIFETIME);
    //new Store(NUMBER_OF_ACTIVE_ENTRIES);
  passiveEntries_ =  new HLSLocationCache(this, CHC_BASE_SIZE, ENTRY_LIFETIME);
    //new Store(NUMBER_OF_PASSIVE_ENTRIES);
  outOfCellEntries_ = new HLSLocationCache(this, HLS_ACTIVE_ENTRIES_SIZE, ENTRY_LIFETIME);
  updateReceiver_ = new BasicUpdateReceiver(activeEntries_,
					    passiveEntries_, 
					    outOfCellEntries_,
					    this);
  requestProcessor_ = new BasicRequestProcessor(passiveEntries_, this);
  handoverManager_ = new AdvancedHandoverManager(
    //new BasicHandoverManager(
					      activeEntries_, 
					      passiveEntries_, 
					      outOfCellEntries_,
					      this,
					      cellbuilder_);
  
  // initialize the last request number value
  lastReqNr = -1;

  updateSender_->start();
}

HLS::~HLS() {
  delete cellbuilder_;
  delete updateSender_; 
  delete activeEntries_; 
  delete passiveEntries_;
  delete outOfCellEntries_;
  delete updateReceiver_; 
  delete requestProcessor_; 
  delete reqtable_;
  delete handoverManager_;
}

// is called by tap in GPSR_Agent, that means we can have a
// look at all packets we receive, no matter to which address
// they were sent to
// at the moment, we process beacons, piggybacked location info
// and cellcast replies here
void HLS::evaluatePacket(const Packet *p)
{ 
  struct hdr_hls *hlsh = HDR_HLS(p);
  //struct hdr_ip *iphdr = HDR_IP(p);
  struct hdr_gpsr *gpsrh = HDR_GPSR(p);
  struct hdr_greedy *greedyh = HDR_GREEDY(p);
  struct hdr_gopher *gopherh = HDR_GOPHER(p);
  struct hdr_cmn *cmnh = HDR_CMN(p);

  if ((cmnh->ptype()==PT_GPSR)&&(gpsrh->mode_ == GPSRH_BEACON)) {
    // Direct Neighbor Information from GPSR Beacons
    nodeposition neighb;
    neighb.id = HDR_IP(p)->saddr();//iphdr->saddr();
    neighb.ts = Scheduler::instance().clock(); // Off by a few because Beacons don't have a TS

    neighb.pos.x = gpsrh->hops_[0].x;
    neighb.pos.y = gpsrh->hops_[0].y;
    neighb.pos.z = gpsrh->hops_[0].z;    

    passiveEntries_->add(&neighb);
  } // end of BEACON processing

    if ((cmnh->ptype()==PT_GREEDY)&&(greedyh->mode_ == GREEDYH_BEACON)) {
    // Direct Neighbor Information from GPSR Beacons
    nodeposition neighb;
    neighb.id = HDR_IP(p)->saddr();//iphdr->saddr();
    neighb.ts = Scheduler::instance().clock(); // Off by a few because Beacons don't have a TS

    neighb.pos.x = greedyh->hops_[0].x;
    neighb.pos.y = greedyh->hops_[0].y;
    neighb.pos.z = greedyh->hops_[0].z;    

    passiveEntries_->add(&neighb);
  } // end of BEACON processing

    if ((cmnh->ptype()==PT_GOPHER)&&(gopherh->mode_ == GOPHERH_BEACON)) {
    // Direct Neighbor Information from GPSR Beacons
    nodeposition neighb;
    neighb.id = HDR_IP(p)->saddr();//iphdr->saddr();
    neighb.ts = Scheduler::instance().clock(); // Off by a few because Beacons don't have a TS

    neighb.pos.x = gopherh->hops_[0].x;
    neighb.pos.y = gopherh->hops_[0].y;
    neighb.pos.z = gopherh->hops_[0].z;    

    passiveEntries_->add(&neighb);
  } // end of BEACON processing

  if(hlsh->type_ == HLS_CELLCAST_REPLY)
    {
      requestProcessor_->cellcastReplyOnMacReceived(p);
    }

  if(hlsh->type_ == HLS_CIRCLECAST_REQUEST)
    {
      requestProcessor_->circlecastRequestOnMacReceived(p);
    }

  // it is not an explicit hls packet 
  if(hlsh->status_ != ON_THE_FLY_UPDATE)
    {
      // the packet didn't contain locservice info
      return;
    }
  
  // if it is part of a communication connection, the sender of the packet
  // will put his  position in the header to keep us in sync. 
  if(HDR_IP(p)->daddr() == addr())
    { 
      // we are the target, thus endpoint of the communication connection
      // update the entry 
      passiveEntries_->add(&hlsh->src);
      if(hls_verbose)
	{
	  // HLS Cache Update
	  trace("HLS_CUpd  %.12f %d [%d %.4f %.2f %.2f]",
		Scheduler::instance().clock(), // when
		addr(),                        // I
		hlsh->src.id,                  // the updater
		hlsh->src.ts,                  // ts of the information
		hlsh->src.pos.x,               // x ...
		hlsh->src.pos.y);              // and y of the target
	}
    }
#ifdef AGGRESSIVE_CACHING
  else 
    {
      // we are not the target but grep all info we can
      passiveEntries_->add(&hlsh->src);	      
    }  
#endif
}
  
void HLS::recv(Packet* &p) {
  struct hdr_hls *hlsh = HDR_HLS(p);
  
  if((HDR_CMN(p)->ptype()) == PT_HLS)
    {
#ifdef AGGRESSIVE_CACHING

      // save positioninformation included in this packet into passiveEntries
      passiveEntries_->add(&hlsh->src);	
#endif
	

    switch(hlsh->type_)
      {
      case HLS_UPDATE :
	updateReceiver_->recv(p, false);
	break;
      case HLS_HANDOVER :
	// at the moment, handover packets will be treated like 
	// updates (which isn't really correct, we extract the src 
	// information in the hls-header, but the source is the 
	// one who sent us the handover packet. 
	// normally, there should be another field just for
	// this data.
	handoverManager_->recv(p, false);
	
	break;
      case HLS_REQUEST :
	requestProcessor_->recv(p);
	break;	

      case HLS_CELLCAST_REQUEST :
	requestProcessor_->recvCellcastRequest(p);
	p = NULL;
	break;
      case HLS_CELLCAST_REPLY :
	requestProcessor_->recvCellcastReply(p);
	break;
      case HLS_REPLY :
	// we should put the result in the passive entries table
	processReply(p);
	break;
      case HLS_CIRCLECAST_REQUEST :
	requestProcessor_->recvCirclecastRequest(p, false);
	break;
      default:
	printf("##error : HDR_HLS(p)->type unknown (hls.cc) node %d\n", 
	       addr());
	break;
      }
  } 
  return;
} // end of HLS::recv(...)



void HLS::dropPacketCallback(Packet* &p) 
{ 
  // if the dropped packet is a locs packet there are two 
  // possibilities:
  // update/handover packet => save in active entries (I am not a member of the
  // right cell the entry belongs to. After callCheckTimer expires the packet
  // is transfered (back) to the cell.)
  // request packet => The targetcell is not reachable. The Request has to be 
  // forwarded to the cell on the next level.


  if((HDR_CMN(p)->ptype()) == PT_HLS)
    {
      switch(HDR_HLS(p)->type_)
	{
	case HLS_UPDATE : 
	  // force the updateReceiver to save the info in his active store
	  updateReceiver_->recv(p, true);//, true);
	  break;
	case HLS_REQUEST :
	  requestProcessor_->processRequestUnreachableCell(p);
	  p = NULL;
	  break;		  	  
	case HLS_HANDOVER :
	  handoverManager_->recv(p, true);
	  break;
	case HLS_REPLY :
	  if(hls_verbose)
	    { 
	      position pos;
	      mn_->getLoc(&pos.x, &pos.y, &pos.z);
	      struct hdr_hls *hlsh = HDR_HLS(p);
	      
	      // HLS Reply drop (no route to requesting node)
	      trace("HLS_REP_d %.12f (%d_%d) %d %.2f", 
		    Scheduler::instance().clock(), // timestamp
		    hlsh->reqid.node,          // the unique...
		    hlsh->reqid.nr,            // id of the request
		    addr(),                    // my address 	  
		    distance(hlsh->dst.id, pos));// the distance between my pos
	      // and the node who wanted the info
	    }
	  break;          
	case HLS_CELLCAST_REQUEST :
	  printf("### %d: dropPacketCallback of cellcast request\n", addr());
	  trace("%.21f %d: dropPacketCallback of cellcast request\n", 
		Scheduler::instance().clock(), addr());
	  break;
	case HLS_CELLCAST_REPLY :
	  break;
      case HLS_CIRCLECAST_REQUEST :
	requestProcessor_->recvCirclecastRequest(p, true);
	break;
	default :
	  printf("### %d: dropPacketCallback with undefined type\n", addr());
	  trace("%.21f %d: dropPacketCallback with undefined type %d\n", 
		Scheduler::instance().clock(), addr(), HDR_HLS(p)->type_);
	  break;
	}
    }
  return; 
} 

bool HLS::poslookup(Packet *p) {
    struct hdr_ip *iphdr = HDR_IP(p);

    if(iphdr->daddr() < 0) // don't process broad- or geocast packets
      {
	// there can't be an entry for this address
	return true;
      }
    
    // Prune Location Service Packets
    
    if (HDR_CMN(p)->ptype()== PT_HLS)  { return true; }
    
    // 1. check the local cache (active and passive)
    nodeposition* nodepos = findEntry(iphdr->daddr());

    if((nodepos != NULL) &&
       ((Scheduler::instance().clock() - nodepos->ts) < HLS_MAX_CACHE_LOOKUP_AGE))
      {
	if(hls_verbose)
	  {
	    // HLS Cache Lookup
	    trace("HLS_CL    %.12f %d [%d %.4f %.2f %.2f] {%.2f}",
		  Scheduler::instance().clock(), // when
		  addr(),                        // I
		  nodepos->id,                   // target
		  nodepos->ts,                   // ts of the information
		  nodepos->pos.x,                // x ...
		  nodepos->pos.y,                // and y of the target
		  distance(nodepos->id, nodepos->pos)); // deviation
	  }
	
	iphdr->dx_ = nodepos->pos.x;
	iphdr->dy_ = nodepos->pos.y;
	iphdr->dz_ = nodepos->pos.z;

	// write my address to the packet to keep loc_service at my
	// communication partner up to date
	struct hdr_hls* hlsh = HDR_HLS(p);
	mn_->getLoc(&hlsh->src.pos.x, &hlsh->src.pos.y, &hlsh->src.pos.z);
	hlsh->src.ts = Scheduler::instance().clock();
	hlsh->src.id = addr();
	hlsh->status_ = ON_THE_FLY_UPDATE;
	return true;
	}
    // 2. Check on neighbourtable of routing is not necessary. All packets the 
    // node receives are checked in evaluatePacket method. So informatiosn are 
    // saved in passive entries
    
    // 3. Send a posRequests
    // No Location can be found in the Location Cache, so we'll
    // check if the dst is already queried and if it is not, we'll
    // send a Location Request Packet


      
    // Check if DST has been queried
    locrequest* request = (locrequest*)reqtable_->search(iphdr->daddr());

    
    if (!request) {
      // DST has not been queried in a long time (if ever)
      
      // remember that we sent the request
      struct locrequest tmpreq = { 
	iphdr->daddr(), Scheduler::instance().clock(), 0, 0 
      };
      reqtable_->add(&tmpreq);

      sendRequest(iphdr->daddr(), 1); // 1 because of level 1
    }
    
    // false has to been returned since nothing was found. The routing asks the locservice, if the Position is present, again after a given timeout.
    // hier muss false zurckgegeben werden, da noch nichts gefunden wurde.
    return false;
}

// sends a reqeust to the responsible cell on the given level
// returns true if it was a valid level (that means a responsible
// cell could have been determined) or false if it wasn't a valid level
bool HLS::sendRequest(int nodeid, int level)
{
  Packet *p = allocpkt();
  
  struct hdr_hls *hlsh = HDR_HLS(p);
  struct hdr_ip *iph = HDR_IP(p);
  struct hdr_cmn *cmnh = HDR_CMN(p);
  struct hdr_gpsr *gpsrh = HDR_GPSR(p);
  struct hdr_greedy *greedyh = HDR_GREEDY(p);
  struct hdr_gopher *gopherh = HDR_GOPHER(p);

  iph->sport() = RT_PORT;
  iph->dport() = RT_PORT;
  iph->saddr() = parent->addr();
  iph->daddr() = NO_NODE;

  // determine the RC for the given node
  position pos;
  mn_->getLoc(&pos.x, &pos.y, &pos.z);
  int responsibleCell = cellbuilder_->getRC(nodeid, level, pos);

  if(responsibleCell == NO_VALID_LEVEL)
    {
      return false;
    }

  lastReqNr++;
  struct request_id reqID(addr(), lastReqNr);

  position cellPosition = cellbuilder_->getPosition(responsibleCell);

  // TRACE
  if(hls_verbose)
    {
      // HLS Request send
      trace("HLS_REQ_s %.12f (%d_%d) %d ->%d <%d %.2f %.2f (%d)>", 
	    Scheduler::instance().clock(), // timestamp
	    reqID.node,                    // address of requestor
	    reqID.nr,                      // local number of the request
	    reqID.node,                    // address if requestor
	    nodeid,                        // target node
	    responsibleCell,               // destination cell
	    cellPosition.x,                // x and y 
	    cellPosition.y,                // of the center of the cell
	    level);                        // level of the cell
    }


  // Let GPSR route this pkt to the center of the responsible cell
  iph->dx_ = cellPosition.x;
  iph->dy_ = cellPosition.y;
  iph->dz_ = cellPosition.z;
  iph->ttl() = HLS_REQUEST_TTL_PER_LEVEL * level;

  cmnh->ptype() = PT_HLS;
  cmnh->addr_type_ = NS_AF_INET;
  cmnh->num_forwards() = 0;
  cmnh->next_hop_ = NO_NODE;
  cmnh->direction() = hdr_cmn::DOWN;
  cmnh->xmit_failure_ = 0;

  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  greedyh->mode_ = GPSRH_DATA_GREEDY;
  greedyh->port_ = hdr_gpsr::LOCS;
  greedyh->geoanycast = true;

  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // now we have to put our position information into the
  // packet (together with a timestamp)
  hlsh->src.id = parent->addr();
  hlsh->src.ts = Scheduler::instance().clock();
  mn_->getLoc(&hlsh->src.pos.x, &hlsh->src.pos.y, &hlsh->src.pos.z);
  hlsh->type_ = HLS_REQUEST;

  // set the cell field
  hlsh->cell.init();
  hlsh->cell.id    = responsibleCell;
  hlsh->cell.level = level;
  hlsh->cell.pos.x = cellPosition.x;
  hlsh->cell.pos.y = cellPosition.y;
  hlsh->cell.pos.z = cellPosition.z;
  
  // put the id of the requested node in the packet
  hlsh->dst.id = nodeid;
  // the unique id for the request
  hlsh->reqid = reqID;

  parent->recv(p, NULL);
  return true;
} //end of send Request

// this mehtod is called when we received a reply packet to
// a position lookup
void HLS::processReply(Packet* &p)
{
  // put the result in the passive store
  struct hdr_hls *hlsh = HDR_HLS(p);
  if(hlsh->dst.id == addr())
    {
      if(hls_verbose)
	{
	  // HLS Reply receive
	  trace("HLS_REP_r %.12f (%d_%d) %d <-%d [%.4f %.2f %.2f] {%.2f}", 
		Scheduler::instance().clock(), // timestamp
		hlsh->reqid.node,              // the unique...
		hlsh->reqid.nr,                // id of the request
		addr(),                        // address of requestor
		hlsh->src.id,                  // node who answered
		hlsh->src.ts,                  // the age of the answer
		hlsh->src.pos.x,               // the  
		hlsh->src.pos.y,               // coordinates
		distance(hlsh->src.id, hlsh->src.pos)); // deviation
		
	}
      // out node is the target of the reply
      printf("***********************************************\n");
      printf("*** (%d): reply from (%d) received (ts %f) ****\n", 
	     addr(), hlsh->src.id, Scheduler::instance().clock());
      printf("***********************************************\n");
      
      // save it
      passiveEntries_->add(&hlsh->src);

      // delete request entry in reqtable
      reqtable_->remove(hlsh->src.id);

      // notify the routing agent that the reply arrived
      parent->notifyPos(hlsh->src.id);

      // we've been the target of the reply, so free it
      Packet::free(p);
      p = NULL;
    }
#ifdef AGGRESSIVE_CACHING
  else
    {
      // we save the destination because the source has 
      // already been saved in the recv-function
      passiveEntries_->add(&hlsh->dst);
    }
#endif
}

// returns the id of the cell in which the node is at the moment
int HLS::getCell()
{
  position pos;
  mn_->getLoc(&pos.x, &pos.y, &pos.z);
  return cellbuilder_->getCellID(pos);
}

// searches our caches (active an passive) for an entry for the
// node with id nodeid.
// returns a pointer to the nodeposition information if we found
// something, otherwise NULL
nodeposition* HLS::findEntry(int nodeid)
{
  if(nodeid == addr())
    {
      // we are looking for ourself
      nodeposition nodepos = getNodeInfo();//new nodeposition();
      passiveEntries_->add(&nodepos);
      return (nodeposition*) passiveEntries_->search(nodeid);
    }

  nodeposition* nodepos = (nodeposition*) activeEntries_->search(nodeid);

  if(nodepos == NULL)
    {
      nodepos = (nodeposition*) outOfCellEntries_->search(nodeid);
    }
 
 if(nodepos == NULL)
    {
      // what happens if entries are present in passive and active cache?
      nodepos = (nodeposition*) passiveEntries_->search(nodeid);
    }

  return nodepos;
}

// same as above, we just don't search our passive cache
// (thus only location servers will answer the request)
nodeposition* HLS::findActiveEntry(int nodeid)
{
  if(nodeid == addr())
    {
      // we are looking for ourself
      nodeposition nodepos = getNodeInfo();

      passiveEntries_->add(&nodepos);
      return (nodeposition*) passiveEntries_->search(nodeid);
    }

  nodeposition* nodepos = (nodeposition*) activeEntries_->search(nodeid);

  if(nodepos == NULL)
    {
      nodepos = (nodeposition*) outOfCellEntries_->search(nodeid);
    }
  return nodepos;
}


// returns the information about our current position, cell, ...
nodeposition HLS::getNodeInfo()
{
  struct nodeposition nodepos;
  nodepos.id = parent->addr();
  nodepos.ts = Scheduler::instance().clock();
  mn_->getLoc(&nodepos.pos.x, &nodepos.pos.y, &nodepos.pos.z);
  nodepos.targetcell = -1;
  return nodepos;
}

// calculates the size of the HLS header of this packet
// (not the real size! Rather the number of bytes which would
// be necessary in an optimized implementation)
int HLS::hdr_size(Packet* p)
{
  struct hdr_cmn *cmnh = HDR_CMN(p);
  struct hdr_hls *hlsh = HDR_HLS(p);

  // Defining Base Field Types in Bytes
  const unsigned int nodeid     = 4; // IPv4 Adressen should be sufficient
  const unsigned int timestamp  = 2;
  const unsigned int locCoord   = 3; // to have enough prescision
  const unsigned int position   = locCoord + locCoord;
  const unsigned int cell       = 2;
  //const unsigned int request_id = 2; // is always used together with source ID !
  const unsigned int level      = 1;

  const unsigned int nodeposition = nodeid + timestamp + position; // 12

  const unsigned int TTL        = 1;
  const unsigned int flags      = 1;
  const unsigned int basic_info = TTL + flags;
  
  // All LS based routing agents must be listed here - copied by wk from mk
  if (cmnh->ptype() == PT_GPSR) { return 0; } // GPSR Packet


  if (cmnh->ptype() == PT_HLS) { // HLS Packet
    switch(hlsh->type_)
      {
      case HLS_UPDATE :	
	return(basic_info+nodeposition+cell);
	// src of the update, target cell
	break;
      case HLS_HANDOVER :
	return(basic_info+(hlsh->numberOfNodeinfosToHandover + 1)*nodeposition+cell);
	// information to handover, target cell, previous location server
	break;
      case HLS_REQUEST :
	return(basic_info+nodeposition+cell+level+nodeid);
	// src of the request, target cell, level of target cell, target node
	// (from the perspective of the source) 
	break;	
      case HLS_CELLCAST_REQUEST :
	return(basic_info+nodeposition+cell+nodeid);
	// sender of the ccrequest, target cell, node for which is asked
	break;
      case HLS_CELLCAST_REPLY :
	return(basic_info+3*nodeposition);
	// source and target of the reply + the information
	break;
      case HLS_REPLY :
	return(basic_info+2*nodeposition);
	// sender and receiver of the answer
	break;
      case HLS_CIRCLECAST_REQUEST :
	return (basic_info+nodeposition+cell*2+nodeid);
	// sender of request, first cell and actual target cell (all
	// other cells can be calculated with this), node for which is
	// asked
	break;
      default:
	printf("Invalid HLS Packet wants to know it's size !\n");
	abort();
      }	
  }
  
  // Data Packet
  return (2*nodeposition);
} // end of hdr_size

// returns the distance between the actual location of the node nodeid
// and the given position
double distance(int nodeid, position pos)
{
  position nodepos;
  God::instance()->getPosition(nodeid, &nodepos.x, &nodepos.y, &nodepos.z);
  double deltaX = pos.x - nodepos.x;
  double deltaY = pos.y - nodepos.y;
  //double deltaZ = pos.z - nodepos.z;

  return sqrt((deltaX*deltaX)+(deltaY*deltaY));
}
