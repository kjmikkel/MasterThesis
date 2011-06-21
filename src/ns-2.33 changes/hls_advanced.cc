#include "hls_basic.h"
#include "../gpsr/gpsr.h"
#include "../greedy/greedy.h"
#include "../gopher/gopher.h"

// AHM AdvancedHandoverManager //////////////////////////////////////
AdvancedHandoverManager::AdvancedHandoverManager(HLSLocationCache* activeEntries, 
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
void AdvancedHandoverManager::recv(Packet* &p, bool forceActiveSave)
{
  hdr_hls* hlsh = HDR_HLS(p);

  position destPos;
  position myPos;
  // getting my actual position
  hls_->mn_->getLoc(&myPos.x, &myPos.y, &myPos.z);
  // getting the destination of the update package
  destPos = hlsh->cell.pos;

  if(sameCell(&destPos, &myPos, hls_->cellbuilder_))
    {
      if(hls_trace_handovers)
	{
	  int myCell = hls_->cellbuilder_->getCellID(myPos);
	  // HLS Handover real receive
	  hls_->trace("HLS_H_rr  %.12f %d<-%d [%.2f %.2f <%d>] <%d %.2f %.2f> %d", 
		      Scheduler::instance().clock(), // timestamp
		      hls_->addr(),                  // address of update receiver
		      HDR_IP(p)->saddr(),            // address of the previous info owner
		      myPos.x,                       // my...
		      myPos.y,                       // coordinates
		      myCell,                        // my cell
		      hlsh->cell.id,                 // target cell
		      destPos.x,                     // center...
		      destPos.y,                     // of the target cell	   
		      hlsh->numberOfNodeinfosToHandover);
	}

      nodeposition* data = (nodeposition*) p->accessdata();
      for(int i=0;i<hlsh->numberOfNodeinfosToHandover;i++) {      
	activeEntries_->add(&data[i]);
	//	printf("targetcell %d sendingnode %d\n", data[i].targetcell, data[i].id);
      }
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
	  hls_->trace("HLS_H_fr  %.12f %d<-%d [%.2f %.2f <%d>] <%d %.2f %.2f> %d", 
		      Scheduler::instance().clock(), // timestamp
		      hls_->addr(),                  // address of update receiver
		      HDR_IP(p)->saddr(),            // address of the previous info owner
		      myPos.x,                       // my...
		      myPos.y,                       // coordinates
		      myCell,                        // my cell
		      hlsh->cell.id,                 // target cell
		      destPos.x,                     // center...
		      destPos.y,                     // of the target cell	      
		      hlsh->numberOfNodeinfosToHandover);
	}
      nodeposition* data = (nodeposition*) p->accessdata();
      for(int i=0;i<hlsh->numberOfNodeinfosToHandover;i++) {      
	outOfCellEntries_->add(&data[i]);
     
      }      
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
} // end of AHM::recv(..)

void AdvancedHandoverManager::timerCallback()
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
void AdvancedHandoverManager::checkCacheAndTransmit(HLSLocationCache* cache, int actualCell)
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
      printf("out of memory in AdvancedHandoverManager::checkCacheAndTransmit\n");
      exit(-1);
    }
				  // = new nodeposition[arraysize];
  // end workaround declaration (more code inloop)
  // activeEntries nach eintr�en durchsuchen, die nicht in actualCell
  // geh�en.
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
			printf("out of memory in AHM:checkCacheAndTransmit\n");
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


  if(index > 0)
    {
      bubblesort(tmparray, index);
      int targetcell = tmparray[0].targetcell;
      int startindex = 0;
      int counter = 0;
      for(int i=0;i<index;i++)
	{
	  // we delete the information here from the cache to avoid
	  // conflicts in the loop over the table of the cache
	  cache->remove(tmparray[i].id);

	  if(tmparray[i].targetcell == targetcell)
	    {
	      // count how much entries with the same target cell exist
	      counter++;
	    }
	  else
	    {
	      // the target cell has changed, thus transmit the information
	      // with the same target cell in one big packet and go on
	      handoverInformation(&tmparray[startindex], counter);
	      targetcell = tmparray[i].targetcell;
	      counter = 1;
	      startindex = i;	      
	    }
	}
      // also the last one needs to be handed over
      handoverInformation(&tmparray[startindex], counter);
    }
  free(tmparray);
} // end of AHM::checkCacheAndTransmit

// this function is responsible for handing over the information
// it constructs a packet and sends it to the target cell
// (to the cell where the information should be)
void AdvancedHandoverManager::handoverInformation(nodeposition* info, int numberOfInfos)
{  
  int numberOfInformationBytes = sizeof(nodeposition)*numberOfInfos;
  Packet *pkt = hls_->allocpkt();
  pkt->allocdata(numberOfInformationBytes);

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

  // GPSR setup
  gpsrh->mode_ = GPSRH_DATA_GREEDY;
  gpsrh->port_ = hdr_gpsr::LOCS;
  gpsrh->geoanycast = true;

  // GREEDY setup
  greedyh->mode_ = GREEDYH_DATA_GREEDY;
  greedyh->port_ = hdr_greedy::LOCS;
  greedyh->geoanycast = true;

  // GOPHER setup
  gopherh->mode_ = GOPHERH_DATA_GREEDY;
  gopherh->port_ = hdr_gopher::LOCS;
  gopherh->geoanycast = true;

  // init my own header
  hlsh->init();
  // now we have to put the actual position information into the
  // packet (together with a timestamp)
  nodeposition* data = (nodeposition*) pkt->accessdata();
  memcpy(data, info, numberOfInformationBytes);
  hlsh->numberOfNodeinfosToHandover = numberOfInfos;

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
      hls_->trace("HLS_H_s   %.12f %d <%d %.2f %.2f> -> <%d %.2f %.2f> %d", 
		  Scheduler::instance().clock(), // timestamp
		  hls_->addr(),                  // address of handover sender
		  myCell,                        // my cell
		  cellPos.x,                     // my cell's position x ...
		  cellPos.y,                     // and y
		  info->targetcell,              // target cell
		  cellPosition.x,                // the  
		  cellPosition.y,                // coordinates of the cell      
		  hlsh->numberOfNodeinfosToHandover);
    }

  hls_->parent->recv(pkt, NULL);  
} // end of AHM::handoverInformation(...)








void AdvancedHandoverManager::bubblesort(nodeposition* A, int size) {
  if(size==1)
    {
      return;
    }
  for(int i=size;i>0;i--) {
    for(int j=0;j<i-1;j++) {
      if (A[j].targetcell > A[j+1].targetcell) {
        exchange(&A[j], &A[j+1]);
      }
    }
  }
}

void AdvancedHandoverManager::exchange(nodeposition* a, nodeposition* b)
{
  nodeposition tmp = *a;
  *a = *b;
  *b = tmp;
}
