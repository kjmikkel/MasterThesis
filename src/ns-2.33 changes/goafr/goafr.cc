/* -*-	Mode:C++; c-basic-offset:4; tab-width:4; indent-tabs-mode:t -*- */
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

  $Id: gpsr.cc,v 1.92 2003/01/24 11:01:07 lochert Exp $
*/

// Modified by Mikkel Kjær Jensen to GOAFR

// GOAFR for ns2 w/wireless extensions

#include <math.h>
#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdarg.h>

#include "cmu-trace.h"
#include "random.h"
#include "mobilenode.h"
#include "goafr.h"

// Location Services
#include "../locservices/hdr_locs.h"
#include "../locservices/omnilocservice.h"
#include "../locservices/realocservice.h"
#include "../locservices/gridlocservice.h"
#include "../hls/hls.h"
#include "ellipsis.h"

#include "god.h"

// Formerly defined global in this file
#include "geo_util.h"

#define KARP_PERI               // Karp's Perimeter mode in his PhD paper
#ifndef PING_TTL
#define PING_TTL 128 /* 10240; Why must this be 10240 ? Interferes evaluation */
#endif

#define MAX_COORD 10

#define BEACON_RESCHED                                                     \
           beacon_timer_->resched(bint_ + Random::uniform(2*bdesync_* bint_) - \
			          bdesync_ * bint_)


/** Cmp-Operator for binsearch */
static int GOAFRNeighbEntCmp(const void *a, const void *b)
{
	nsaddr_t ia = ((const GOAFRNeighbEnt *) a)->dst;
	nsaddr_t ib = (*(const GOAFRNeighbEnt **) b)->dst;
	if (ia > ib) return 1;
	if (ib > ia) return -1;
	return 0;
}

static int coordcmp(const void *c, const void *d){
	nsaddr_t ic = *(const nsaddr_t *) c;
	nsaddr_t id = *(const nsaddr_t *) d;
	if (ic > id) return 1;
	if (id > ic) return -1;
	return 0;
}

GOAFRNeighbTable::GOAFRNeighbTable(GOAFR_Agent *mya)
{
	int i;

	counter_clock = true;

	nents = 0;
	maxents = 100;
	tab = new GOAFRNeighbEnt *[100];
	a = mya;
	for (i = 0; i < 100; i++)
		tab[i] = new GOAFRNeighbEnt(a);
  
	val_item = new DHeapEntry[God::instance()->nodes()];
	valid = new DHeap(God::instance()->nodes());
}

GOAFRNeighbTable::~GOAFRNeighbTable()
{
	int i;

	// cancel timers
	for (i = 0; i < nents; i++) {
		tab[i]->dnt.force_cancel();
		tab[i]->ppt.force_cancel();
		delete tab[i];
	}
	delete[] tab;
}

GOAFRNeighbTableIter GOAFRNeighbTable::InitLoop() {
	return (GOAFRNeighbTableIter) 0;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::NextLoop(GOAFRNeighbTableIter *it) {
	if (((unsigned int) *it) >= (unsigned int) nents)
		return 0;

	return tab[(*it)++];
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findshortest(MobileNode *mn, double x, double y, double z)
{
	GOAFRNeighbEnt *ne = 0;
	double shortest, t, myx, myy, myz;
	int i;

	mn->getLoc(&myx, &myy, &myz);
	// warning, this might be false
	shortest = distance(myx, myy, myz, x, y, z);
	for (i = 0; i < nents; i++){
		if ((t = distance(tab[i]->x, tab[i]->y, tab[i]->z, x, y, z)) < shortest) {
			shortest = t;
			ne = tab[i];
		}
	}
	return ne;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findshortest_cc(MobileNode *mn, double x, double y, double z, double alpha)
{
	GOAFRNeighbEnt *ne = 0;
	double dist, t, myx, myy, myz, new_dist, best = 1;
	int i;

	mn->getLoc(&myx, &myy, &myz);
	dist = distance(myx, myy, myz, x, y, z);

	for (i = 0; i < nents; i++) {
		new_dist = distance(tab[i]->x, tab[i]->y, tab[i]->z, x, y, z);

		if (new_dist < dist &&
			(t = alpha * tab[i]->load / 100 + (1 - alpha) * (new_dist / dist)) < best) {
			best = t;
			ne = tab[i];
		}
	}

	return ne;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findshortestXcptLH(MobileNode *mn, 
									nsaddr_t lastHopId,
									double x, double y, double z)
{
	GOAFRNeighbEnt *ne = 0;
	double shortest, t, myx, myy, myz;
	int i;

	mn->getLoc(&myx, &myy, &myz);
	shortest = distance(myx, myy, myz, x, y, z);
	for (i = 0; i < nents; i++)
		if (((t = distance(tab[i]->x, tab[i]->y, tab[i]->z, x, y, z)) 
			 < shortest) && 
			(tab[i]->dst != lastHopId)) {
			shortest = t;
			ne = tab[i];
		}
	return ne;
}

GOAFRNeighbEnt * 
GOAFRNeighbTable::ent_findnext_onperi(MobileNode *mn, int node, double dx, double dy, double dz, int plan){
	double myx, myy, myz;
	double brg, brg_tmp, brg_tmp2, minbrg = 3*M_PI;

#ifndef KARP_PERI
	if(counter_clock )
		minbrg = -3*M_PI;
#endif
	double mindist = 0.0;

	GOAFRNeighbEnt *ne, *minne = NULL;

	mn->getLoc(&myx, &myy, &myz);
	brg = bearing(myx, myy, dx, dy);

#ifdef KARP_PERI
	brg = norm(atan2(myy-dy, myx-dx));
#endif

	int counter = 0;
	GOAFRNeighbTableIter niloo = InitLoop();
	while((bool)(ne = NextLoop(&niloo))){
#ifdef KARP_PERI
		if(ne->dst == node){
			ne->x = dx; ne->y = dy; ne->z = dz;
		}
		//continue;
		counter++;
		if(counter>100) {
			printf("nents %d in node %d\n", nents, mn->address());
			for(int i=0;i<nents;i++)
				{
					printf("nr %d dst %d\n", i, tab[i]->dst);
				}
			exit(-1);
		}
		struct DHeapEntry tmp;
		tmp.id = ne->dst; tmp.cost = 0; tmp.pred = NULL;
		itedge = valid->find(&tmp);
		if(plan && itedge == 0)
			continue;
		
		brg_tmp2 = norm(atan2(myy-ne->y, myx-ne->x));
		brg_tmp = norm(brg_tmp2 - brg);
		if(brg_tmp < 0)
			exit(1);
		if (brg_tmp < minbrg) {
			mindist = distance(myx, myy, myz, ne->x, ne->y, ne->z);
			minbrg = brg_tmp;
			minne = ne;
		}else{
			if(brg_tmp == minbrg){
				if(distance(myx, myy, myz, ne->x, ne->y, ne->z) < mindist){
					mindist = distance(myx, myy, myz, ne->x, ne->y, ne->z);
					minne = ne;
				}
			}
		}
#else //karp_peri
		if(distance(myx, myy, myz, ne->x, ne->y, ne->z) > God::instance()->getRadioRange())
			continue;    
    
		if(ne->dst == node){
			minne = ne;
			break;      
		}

		brg_tmp2 = bearing(myx, myy, ne->x, ne->y);
		brg_tmp = brg_tmp2 - brg;
		// change peri
		if(counter_clock){
			while(brg_tmp > 0)
				brg_tmp -= 2*M_PI;

			if (brg_tmp > minbrg) {
				mindist = distance(myx, myy, myz, ne->x, ne->y, ne->z);
				minbrg = brg_tmp;
				minne = ne;
			}
		}else{
			while(brg_tmp < 0)
				brg_tmp += 2*M_PI;

			if (brg_tmp < minbrg) {
				mindist = distance(myx, myy, myz, ne->x, ne->y, ne->z);
				minbrg = brg_tmp;
				minne = ne;
			}
		}      
#endif //karp_peri
	} //end while((bool)

	if(minne)
		minne->perilen++;
	return minne;
}

GOAFRNeighbEnt * 
GOAFRNeighbTable::ent_findnextcloser_onperi(MobileNode *mn, double dx, double dy, double dz){
	double myx, myy, myz;
	double mydist;
	GOAFRNeighbEnt *ne, *minne = NULL;

	mn->getLoc(&myx, &myy, &myz);
	mydist = distance(myx, myy, myz, dx, dy, dz);
  
	GOAFRNeighbTableIter ni = InitLoop();
	while((bool)(ne = NextLoop(&ni))){
		if(distance(dx, dy, dz, ne->x, ne->y, ne->z) < mydist){
			minne = ne;
			break;
		}
	}
	minne->perilen++;
	return minne;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findcloser_onperi(MobileNode *mn, double x, double y,
								   double z, int *perihop)
{
	double mydist, t, myx, myy, myz;
	int i, j;

	mn->getLoc(&myx, &myy, &myz);
	mydist = distance(myx, myy, myz, x, y, z);
	for (i = 0; i < nents; i++)
		for (j = 0; j < tab[i]->perilen; j++)
			if ((t = distance(tab[i]->peri[j].x, tab[i]->peri[j].y,
							  tab[i]->peri[j].z, x, y, z)) < mydist) {
				*perihop = j;
				return tab[i];
			}
	return 0;
}

int
GOAFRNeighbEnt::closer_pt(nsaddr_t myip, double myx, double myy, double myz, // me
						 double ptx, double pty, // perimeter startpoint
						 nsaddr_t ptipa, nsaddr_t ptipb, // me?, prev?
						 double dstx, double dsty, // dst
						 double *closerx, double *closery)
{
	if ((min(dst, myip) == min(ptipa, ptipb)) &&
		(max(dst, myip) == max(ptipa, ptipb)))
		// this edge is the same edge where (ptx, pty) lies; nope??
		return 0;
	if (!live)
		// this edge is not part of the planarized graph
		return 0;
	if (cross_segment(ptx, pty, dstx, dsty, myx, myy, x, y,
					  closerx, closery)) {
		if (distance(*closerx, *closery, 0.0, dstx, dsty, 0.0) <
			distance(ptx, pty, 0.0, dstx, dsty, 0.0)) {
			// edge has point closer than (ptx, pty)
			return 1;
		}
	}
	return 0;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findcloser_edgept(MobileNode *mn, double ptx, double pty,
								   nsaddr_t ptipa, nsaddr_t ptipb,
								   double dstx, double dsty,
								   double *closerx, double *closery)
{
	GOAFRNeighbTableIter ni;
	GOAFRNeighbEnt *minne = NULL, *ne;
	double myx, myy, myz;

	mn->getLoc(&myx, &myy, &myz);
	ni = InitLoop();
	while ((ne = NextLoop(&ni))) {
		if (ne->closer_pt(mn->address(), myx, myy, myz, ptx, pty, ptipa, ptipb,
						  dstx, dsty, closerx, closery)) {
			// found an edge with a point closer than (ptx, pty)
			minne = ne;
			ptx = *closerx;
			pty = *closery;
			ptipa = mn->address();
			ptipb = ne->dst;
		}
	}
	return minne;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_findface(MobileNode *mn, double x, double y, double z, int p)
{
	double myx, myy, myz;
	double brg;

	// find bearing to dst
	mn->getLoc(&myx, &myy, &myz);
	brg = bearing(myx, myy, x, y);
	// find neighbor with greatest bearing <= brg
	return ent_next_ccw(brg, myx, myy, p);
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_next_ccw(double basebrg, double x, double y, int p,
						  GOAFRNeighbEnt *inne /*= 0*/)
{
	GOAFRNeighbEnt *minne = NULL, *ne;
	GOAFRNeighbTableIter nil;
	double brg, minbrg = 3*M_PI;

	nil = InitLoop();
	while ((ne = NextLoop(&nil))) {
		if (inne && (ne == inne))
			continue;
		if (p && !ne->live)
			continue;
		brg = bearing(x, y, ne->x, ne->y) - basebrg;
		if (brg < 0)
			brg += 2*M_PI;
		if (brg < 0)
			brg += 2*M_PI;
		if (brg < minbrg) {
			minbrg = brg;
			minne = ne;
		}
	}
	return minne;
}

GOAFRNeighbEnt *
GOAFRNeighbTable::ent_next_ccw(MobileNode *mn, GOAFRNeighbEnt *inne, int p)
{
	double myx, myy, myz;
	double brg;
	GOAFRNeighbEnt *ne;

	// find bearing from mn to (x, y, z)
	mn->getLoc(&myx, &myy, &myz);
	brg = bearing(myx, myy, inne->x, inne->y);
	ne = ent_next_ccw(brg, myx, myy, p, inne);
	if (!ne)
		return inne;
	else
		return ne;
}

GOAFRNeighbEnt *GOAFRNeighbTable::ent_finddst(nsaddr_t dst)
{
	GOAFRNeighbEnt ne(NULL), **pne;

	ne.dst = dst;
	pne = ((GOAFRNeighbEnt **) bsearch(&ne, tab, nents,
								  sizeof(GOAFRNeighbEnt *), GOAFRNeighbEntCmp));
	if (pne)
		return *pne;
	else
		return NULL;
}

void
GOAFRNeighbTable::ent_delete(const GOAFRNeighbEnt *ent)
{
	GOAFRNeighbEnt **pne;
	GOAFRNeighbEnt *owslot=NULL;
	int i, j;

	if ((pne = (GOAFRNeighbEnt **) bsearch(ent, tab, nents,
									  sizeof(GOAFRNeighbEnt *), GOAFRNeighbEntCmp))) {
		i = pne - tab;
		// make sure no timers scheduled for this neighbor
		(*pne)->dnt.force_cancel();
		(*pne)->ppt.force_cancel();
		// slide any subsequent table entries backward
		if (i < (nents - 1))
			owslot = tab[i];
		for (j = i; j < nents - 1; j++)
			tab[j] = tab[j+1];
		if (i < (nents - 1))
			tab[nents-1] = owslot;
		nents--;
	}
}

GOAFRNeighbEnt * 
GOAFRNeighbTable::ent_add(const GOAFRNeighbEnt *ent)
{
	GOAFRNeighbEnt **pne;
	GOAFRNeighbEnt *owslot = NULL;
	int i, j, r, l;

	if ((pne = (GOAFRNeighbEnt **) bsearch(ent, tab, nents,
									  sizeof(GOAFRNeighbEnt *), GOAFRNeighbEntCmp))) {
		// already in table; overwrite
		// make sure there is no pending timer
		i = pne - tab;
		(*pne)->dnt.force_cancel();
		/* XXX overwriting table entry shouldn't affect when to probe this
		   perimeter */
		// careful not to overwrite timers!
		(*pne)->dst = ent->dst;
		(*pne)->x = ent->x;
		(*pne)->y = ent->y;
		(*pne)->z = ent->z;
		(*pne)->load = ent->load;

		return *pne;
	}

	// may have to grow table
	if (nents == maxents) {
		GOAFRNeighbEnt **tmp = tab;
		maxents *= 2;
		tab = new GOAFRNeighbEnt *[maxents];
		bcopy(tmp, tab, nents*sizeof(GOAFRNeighbEnt *));
		for (i = nents; i < maxents; i++)
			tab[i] = new GOAFRNeighbEnt(a);
		delete[] tmp;
	}

	// binary search for insertion point
	if (nents == 0)
		i = 0;
	else {
		l = 0;
		r = nents - 1;
		while ((r - l) > 0) {
			if (ent->dst < tab[l + ((r-l) / 2)]->dst)
				r = l + ((r - l) / 2) - 1;
			else
				l += (r - l) / 2 + 1;
		}
		if (r < l)
			i = r+1;
		else
			// r == l
			if (ent->dst < tab[r]->dst)
				i = r;
			else
				i = r+1;
	}

	// slide subsequent entries forward
	if (i <= (nents - 1))
		owslot = tab[nents];
	j = nents-1;
	while (j >= i) {
		// the second index has to be j, not j-1, otherwise we will get a segfault
		tab[j+1] = tab[j];
		j--;
	}
	// slam into table, without overwriting timers
	if (i <= (nents - 1))
		tab[i] = owslot;
	tab[i]->dst = ent->dst;
	tab[i]->x = ent->x;
	tab[i]->y = ent->y;
	tab[i]->z = ent->z;
	tab[i]->load = ent->load;
	// invalidate the perimeter that may be cached by this neighbor entry
	tab[i]->perilen = 0;
	// XXX gross way to indicate entry is *new* entry, graph needs planarizing
	tab[i]->live = -1;
	nents++;

	return tab[i];
}

int
GOAFRNeighbTable::meanLoad() {
    int i;
    int sum = 0;
      
    for (i = 0; i < nents; i++)
		sum += tab[i]->load;

    if (nents > 0)
		return sum / nents;
    else
		return 0;
}

void
GOAFRNeighbEnt::planarize(GOAFRNeighbTable *nt, int algo,
						  double x, double y, double z) {
	GOAFRNeighbEnt *ne;
	GOAFRNeighbTableIter niplent;
	double uvdist, canddist, midx=0.0, midy=0.0;

	uvdist = distance(x, y, z, this->x, this->y, this->z);
	// This switch is just here for initial setup, and to weed out weird planar requests
	switch(algo) {
	case PLANARIZE_RNG:
		break;
	case PLANARIZE_GABRIEL:
		// find midpt of segment me (u) <-> this (v)
		midx = (x + this->x) / 2.0;
		midy = (y + this->y) / 2.0;
		uvdist /= 2.0;
		break;
	default:
		fprintf(stderr, "Unknown graph planarization algorithm %d\n", algo);
		abort();
		break;
	}
	niplent = nt->InitLoop();
	while ((ne = nt->NextLoop(&niplent))) {
		if (ne == this)
			// w and v identical node--w not a witness
			continue;

		struct DHeapEntry tmp, *tmp2; 
		tmp.id = dst; tmp.cost = 0; tmp.pred = NULL;

		switch(algo) {
		case PLANARIZE_RNG:
			// find max dist. from me (u) to ne (w) vs. this (v) to ne (w)
			canddist = max(distance(x, y, z, ne->x, ne->y, ne->z),
						   distance(this->x, this->y, this->z, ne->x, ne->y, ne->z));
			// is max < dist from me (u) to this (v)?
			if (canddist < uvdist) {
				this->live = 0;
				nt->itedge = 0;

				nt->itedge = nt->valid->find(&tmp);
				if(nt->itedge != 0){
					tmp2 = nt->valid->remove(&tmp);
					delete tmp2;
				}
				return;
			}
			break;
		case PLANARIZE_GABRIEL:
			// is ne (w) inside circle of radius uvdist?
			if (distance(midx, midy, 0.0, ne->x, ne->y, 0.0) < uvdist) {
				this->live = 0;
				nt->itedge = 0;

				nt->itedge = nt->valid->find(&tmp);
				if(nt->itedge != 0){
					tmp2 = nt->valid->remove(&tmp);
					delete tmp2;
				}
				return;
			}
			break;
		default:
			break;
		}
	}
	this->live = 1;
}


void
GOAFRNeighbTable::planarize(int algo, int addr, double x, double y, double z)
{
	GOAFRNeighbEnt *ne;
	GOAFRNeighbTableIter nipl;

	valid->clean();
	nipl = InitLoop();
	struct DHeapEntry *tmp;
	while((ne = NextLoop(&nipl))){
		tmp = new struct DHeapEntry;
		tmp->id = ne->dst; tmp->cost = 0; tmp->pred = NULL;
		valid->insert(tmp);
	}

	nipl = InitLoop();
	while ((ne = NextLoop(&nipl))) {
		ne->planarize(this, algo, x, y, z);
	}
	assert(!valid->empty());
}

int hdr_goafr::offset_;

class GOAFRHeaderClass : public PacketHeaderClass {
public: 
	GOAFRHeaderClass() : PacketHeaderClass("PacketHeader/GOAFR", sizeof(hdr_goafr)) {
		bind_offset(&hdr_goafr::offset_);
	}
} class_goafrhdr;

static class GOAFRClass:public TclClass
{
public:
	GOAFRClass():TclClass ("Agent/GOAFR")
	{
	}
	TclObject *create (int, const char *const *)
	{
		return (new GOAFR_Agent ());
	}
} class_goafr;

GOAFR_Agent::GOAFR_Agent(void) : Agent(PT_GOAFR), use_mac_(0),
							   use_peri_(0), verbose_(1), active_(1), drop_debug_(0), peri_proact_(1),
							   use_implicit_beacon_(0), use_planar_(0), use_loop_detect_(0),
							   use_timed_plnrz_(0), use_beacon_(0), use_congestion_control_(0), 
							   use_reactive_beacon_(0), locservice_type_(0), use_span_(1), 
							   bint_(GOAFR_ALIVE_INT), bdesync_(GOAFR_ALIVE_DESYNC),
							   bexp_(GOAFR_ALIVE_EXP), pint_(GOAFR_PPROBE_INT), pdesync_(GOAFR_PPROBE_DESYNC),
							   lpexp_(GOAFR_PPROBE_EXP), /*ldb_(0),*/ mn_(0), 
							   ifq_(0), locservice_(0), 
				
			   beacon_timer_(0), lastperi_timer_(0), send_buf_timer(this)
{
    ntab_ = new GOAFRNeighbTable(this);

    ifq_ = 0;

    // Init SendBuffer
    for (int i=0;i<SEND_BUF_SIZE;i++) {
		send_buf[i].p = NULL;
		send_buf[i].t = -999.0;
    }

    // Init SendPermissions
    for (unsigned int i=0;i<GOAFR_PKT_TYPES;i++) {
		send_allowed[i] = true;
    }

    bind_time("bint_", &bint_);
    bind_time("bexp_", &bexp_);
    bind_time("bdesync_", &bdesync_);
    bind_time("pint_", &pint_);
    bind_time("pdesync_", &pdesync_);
    bind_time("lpexp_", &lpexp_);
    bind("verbose_", &verbose_);
    bind("drop_debug_", &drop_debug_);
    bind("peri_proact_", &peri_proact_);
    bind("use_mac_", &use_mac_);
    bind("use_peri_", &use_peri_);
    bind("use_implicit_beacon_", &use_implicit_beacon_);
    bind("use_planar_", &use_planar_);
    bind("use_loop_detect_", &use_loop_detect_);
    bind("use_timed_plnrz_", &use_timed_plnrz_);

    bind("use_beacon_", &use_beacon_);
    bind("use_reactive_beacon_", &use_reactive_beacon_);
    bind("use_congestion_control_", &use_congestion_control_);
    bind("cc_alpha_", &cc_alpha_);

    bind("locservice_type_", &locservice_type_);

    // Timer
    if ((use_beacon_)&&(!use_reactive_beacon_)) {
		beacon_timer_ = new GOAFR_BeaconTimer(this);
    }
    if (!peri_proact_) { 
		lastperi_timer_ = new GOAFR_LastPeriTimer(this);
    }
    if (use_timed_plnrz_) {
		planar_timer_ = new GOAFR_PlanarTimer(this);
    }
    pd_timer = new GOAFRPacketDelayTimer(this,16);

    if (use_reactive_beacon_) {
		beacon_delay_ = new GOAFRBeaconDelayTimer(this);
		beaconreq_delay_ = new GOAFRBeaconReqDelayTimer(this);
    }
    
    // What LocationService to use
    switch (locservice_type_) { // dynamic
	case _OMNI_: locservice_ = new OmniLocService(this); break;
	case _REACTIVE_: locservice_ = new ReaLocService(this); break;
	case _GRID_: locservice_ = new GridLocService(this); break;
	case _CELL_: locservice_ = new HLS(this); break;
	default: locservice_ = new OmniLocService(this); break;
    }
}

void
GOAFR_Agent::trace(char *fmt,...)
{
    va_list ap;

    if (!tracetarget)
		return;

    va_start(ap, fmt);
    vsprintf(tracetarget->pt_->buffer(), fmt, ap);
    tracetarget->pt_->dump();
    va_end(ap);
}

void
GOAFR_Agent::tracepkt(Packet *p, double now, int me, const char *type)
{
	char buf[1024];

	struct hdr_goafr *goafrh = HDR_GOAFR(p);

	snprintf (buf, 1024, "V%s %.5f _%d_:", type, now, me);

	if (goafrh->mode_ == GOAFRH_BEACON) {
		snprintf (buf, 1024, "%s (%f,%f,%f)", buf, goafrh->hops_[0].x,
				  goafrh->hops_[0].y, goafrh->hops_[0].z);
		if (verbose_)
			trace((char*)"%s", buf);
	}
}

// don't drop or modify the packet--it's not a copy!
void
GOAFR_Agent::tap(const Packet *p)
{
    if(!active_){ return; }

    hdr_cmn *hdrc = HDR_CMN(p);

    struct hdr_goafr *goafrh = HDR_GOAFR(p);
    
    /* ignore non-IP packets.
       ignore beacons; we process those on regular receive.
       assumes the MAC tap includes all unicast packets bound for us.
       process those here, and avoid calls to beacon_proc() elsewhere. */
    if (use_implicit_beacon_ &&
		(hdrc->addr_type_ == NS_AF_INET) &&
		(hdrc->next_hop_ != (nsaddr_t) IP_BROADCAST)) {
		// snoop it as proof of its sender's existence.
		switch (goafrh->mode_) {
	    case GOAFRH_DATA_GREEDY:
			// prev hop position lives in hops_[0]
			beacon_proc(goafrh->hops_[0].ip, goafrh->hops_[0].x, goafrh->hops_[0].y,
						goafrh->hops_[0].z, goafrh->load);
			break;
	    case GOAFRH_PPROBE:
			// prev hop position lives in hops_[nhops_-1]
			beacon_proc(goafrh->hops_[goafrh->nhops_-1].ip,
						goafrh->hops_[goafrh->nhops_-1].x,
						goafrh->hops_[goafrh->nhops_-1].y,
						goafrh->hops_[goafrh->nhops_-1].z,
						goafrh->load);
			break;
	    case GOAFRH_DATA_PERI:
		case GOAFRH_DATA_ADVANCE:
			// XXX was hops_[goafrh->currhop_-1]
			// prev hop position lives in hops_[0]
			beacon_proc(goafrh->hops_[0].ip,
						goafrh->hops_[0].x,
						goafrh->hops_[0].y,
						goafrh->hops_[0].z,
						goafrh->load);
			break;
	    default:
			fprintf(stderr, "Yow! tap got packet of unk type %d!\n", goafrh->mode_);
			abort();
			break;
		}
    }

    // Reactive Beaconing needs to take a look at pkts
    if (use_reactive_beacon_) {
		checkGoafrCondition(p);
    }

    /*
      LocService Tap for Evaluation
    */

    if (locservice_type_ == _GRID_) {
		locservice_->evaluatePacket(p);
    }
    
    if (locservice_type_ == _REACTIVE_) {
		// Use LocSRequest Packets for implicit beaconing
		// ReaLocService Requests are Broadcast Pakets, thus needing
		//  extra handling. Unicast Pakets are covered by GOAFR
		struct hdr_locs *locsh = HDR_LOCS(p);
		if (use_implicit_beacon_ && locsh->valid_ && (locsh->type_ == LOCS_REQUEST)) {
			beacon_proc(locsh->lasthop.id,
						locsh->lasthop.loc.x,
						locsh->lasthop.loc.y,
						locsh->lasthop.loc.z);
		}
	
		// Let LocService take a look at pkts passing by
		locservice_->evaluatePacket(p);
    } 

	if (locservice_type_ == _CELL_) {
		locservice_->evaluatePacket(p);
	}
    
#if GOAFR_ROUTE_VERBOSE >= 1
    // Each DATA arrival has to trigger a connectivity
    // trace for evaluation
	struct hdr_ip *iph = HDR_IP(p);
	bool arrival = ( (((goafrh->mode_ == GOAFRH_DATA_GREEDY) ||
					  (goafrh->mode_ == GOAFRH_DATA_PERI) ||
					  (goafrh->mode_ == GOAFRH_DATA_ADVANCE)) &&
					  (goafrh->port_ == hdr_goafr::GOAFR)) &&
					 (iph->daddr() == addr()) );
    if (arrival) {
      int analysis = God::instance()->path_analysis_;
      God::instance()->path_analysis_ = 1;

      int shortest = God::instance()->shortestPathLength(addr(),iph->saddr());
      if (shortest == UNREACHABLE) { shortest = 0; } 
	  int taken = 128 - iph->ttl() + 1;
#ifdef PING_TTL
	  taken = PING_TTL - iph->ttl() + 1;
#endif
	  trace("RTE: %.12f _%d_: RouteInfo %d (%d->%d) : %d %d",
			Scheduler::instance().clock(),addr(),
			HDR_CMN(p)->uid(), iph->saddr(), iph->daddr(), 
			taken, shortest);
	  
      God::instance()->path_analysis_ = analysis;
    }
#endif    

}

void
GOAFR_Agent::lost_link(Packet *p)
{
    // Give Locservice the chance to evaluate callbacks
    locservice_->callback(p);
    if (p==NULL) { return; }

    struct hdr_cmn *hdrc = HDR_CMN(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    GOAFRNeighbEnt *ne;
    
    if (use_mac_ == 0) {
		drop(p, DROP_RTR_MAC_CALLBACK);
		return;
    }

    trace ((char*)"VLL %.8f _%d_ %d (%d->%d->%d) [%d]",
		   Scheduler::instance().clock(),
		   mn_->address(),
		   hdrc->uid(),
		   HDR_IP(p)->saddr(),
		   hdrc->next_hop(),
		   HDR_IP(p)->daddr(),
		   hdrc->xmit_reason_);
    
    if (hdrc->addr_type_ == NS_AF_INET) {
		ne = ntab_->ent_finddst(hdrc->next_hop_);
		if (verbose_)
			trace ((char*)"VLP %.5f %d:%d->%d:%d lost at %d [hop %d]",
				   Scheduler::instance().clock(),
				   HDR_IP(p)->saddr(),
				   HDR_IP(p)->sport(),
				   HDR_IP(p)->daddr(),
				   HDR_IP(p)->dport(),
				   mn_->address(), 
				   hdrc->next_hop_);
      
		if (ne) {
			ne->dnt.force_cancel();
			deadneighb_callback(ne);
		}
    }
    
    // grab packets in the ifq bound for the same next hop
    Packet *r, *rh, *rt;
    rh = rt = p;
    // rh ist list head, initialize to 0
    rh->next_ = 0;
    // How many packets to be rerouted? (at least 1 (p))
    unsigned int pCount = 1;

#if 0
    PacketQueue * pq;
    Packet *pt, *pp;
    hdr_cmn *ch = HDR_CMN(p);
    char outBuf[4096];
    char uid[16];
    sprintf(outBuf, "VLLQE %.5f _%d_ ->%d [ ", 
			Scheduler::instance().clock(),
			mn_->address(),
			hdrc->next_hop_);
    if (ifq_ != NULL) {
      
		// get encapsulated Queue (OMG, this ist dirty!)
		pq = ifq_->q();

		for(pt = pq->head(); pt; pt = pt->next_) {
	
			ch = HDR_CMN(pt);
			if (ch->next_hop() == hdrc->next_hop_){
				sprintf(uid, "%d ", ch->uid());
				strcat(outBuf, uid);
			}
			pp = pt;
		}
      
		strcat(outBuf, "]");
		if (verbose_)
			trace(outBuf);
      
#endif

		if (ifq_ != NULL) {
			while ((r = ifq_->filter((nsaddr_t) hdrc->next_hop_))) {
				rt->next_ = r;
				r->next_ = 0;
				rt = r;
				++pCount;
			}
		}

#if 0     
    }else {
		// ifq_ invalid;
		trace ((char*)"VLLIV %.5f _%d_", 
			   Scheduler::instance().clock(),
			   mn_->address());
    }
#endif
    if (verbose_)
		trace ((char*)"VLLPC %.5f _%d_ %d", 
			   Scheduler::instance().clock(),
			   mn_->address(),
			   pCount);

    // retarget all packets we got to a new next hop
    while (rh) {
		rt = rh;
		rh = rh->next_;
      
		hdrc = HDR_CMN(rt);

		if (verbose_) {
			struct hdr_ip* iphdr = HDR_IP(rt);
			trace((char*)"VLLR: %.8f _%d_ %d (%d->%d->%d) [%d]",
				  Scheduler::instance().clock(),
				  mn_->address(),
				  hdrc->uid(),
				  iphdr->saddr(),
				  hdrc->next_hop(),
				  iphdr->daddr(),
				  hdrc->xmit_reason_);
		}
		goafrh = HDR_GOAFR(rt);
      
		if (hdrc->addr_type_ != NS_AF_INET) {
			drop(rt, DROP_RTR_MAC_CALLBACK);
			continue;
		}
      
		/*
		  Unfortunately we have no choice but to handle LocService Packets with
		  this extra code, because we have to use the Routing Port but don't
		  want GOAFR to handle the (and discard) the Packets
		*/
		if (goafrh->port_ == hdr_goafr::LOCS) {
			int tmp = use_planar_;
			switch (goafrh->mode_) {
			case GOAFRH_DATA_GREEDY: { forwardPacket(rt); break; }
			case GOAFRH_DATA_PERI:
		    case GOAFRH_DATA_ADVANCE:
				use_planar_ = 1;
				if (use_planar_) { forwardPacket(rt, 1); }
				else { drop(rt, DROP_RTR_NEXT_SRCRT_HOP); }
				use_planar_ = tmp;
				break;
			default: 
				fprintf(stderr, "yow! locs packet bounced by MAC and not handled !\n");
				abort();
				break;
			}
			continue;
		}

		// for GOAFR perimeter probes, chop off our own perimeter entry before
		// passing the probe back into the agent for reforwarding
		if (HDR_IP(rt)->dport() == RT_PORT) {
			if (goafrh->mode_ == GOAFRH_PPROBE) {
				if (goafrh->nhops_ == 1) {
					/* we originated it. the neighbor is gone, according to the MAC
					   layer. drop the probe--it was *only* meant for that neighbor. */
					drop(rt, DROP_RTR_NEXT_SRCRT_HOP);
					continue;
				}
				/* we were forwarding the probe, so instead try to recover by
				   forwarding it to a remaining appropriate next hop */
				goafrh->nhops_--;
				periIn(rt, goafrh, GOAFR_PPROBE_RTX);
			}
		} else {
			int tmp = use_planar_;
			switch (goafrh->mode_) {
			case GOAFRH_DATA_GREEDY:
				// give the packet another chance--exercise goafr's good recovery
				forwardPacket(rt);
				break;
			case GOAFRH_DATA_PERI:
			case GOAFRH_DATA_ADVANCE:
				use_planar_ = 1;
				if (use_planar_)
					// not src-routed; give it another chance via another neighbor
					forwardPacket(rt, 1);
				else
					// punt the packet; its chosen src-routed next hop is gone
					drop(rt, DROP_RTR_NEXT_SRCRT_HOP);
				use_planar_ = tmp;
				break;
			default:
				fprintf(stderr,
						"yow! non-data packet for non-GOAFR port bounced by MAC!\n");
				abort();
				break;
			}
		}
	  
    }   
}

static void
mac_callback(Packet * p, void *arg)
{
	((GOAFR_Agent *) arg)->lost_link(p);
}

void
GOAFR_Agent::planar_callback(void)
{
	// re-planarize graph
	if (use_planar_) {
		double myx, myy, myz;

		mn_->getLoc(&myx, &myy, &myz);
		ntab_->planarize(PLANARIZE_GABRIEL, mn_->address(), myx, myy, myz);
	}
	// reschedule us
	// XXX should make interval tunable!!!
	planar_timer_->resched(1.0);
}

void
GOAFR_Agent::lastperi_callback(void)
{
	GOAFRNeighbEnt *ne;
	GOAFRNeighbTableIter ni;

	// don't probe perimeters proactively anymore
	peri_proact_ = 0;
	// cancel all perimeter probe timers
	ni = ntab_->InitLoop();
	while ((ne = ntab_->NextLoop(&ni)))
		ne->ppt.force_cancel();
}

void
GOAFR_Agent::beacon_callback(void)
{
    
    sendBeacon();

    // schedule the next beacon generation event
    if ((use_beacon_)&&(!use_reactive_beacon_)) { BEACON_RESCHED; }
}

void
GOAFR_Agent::deadneighb_callback(GOAFRNeighbEnt *ne)
{
	Scheduler &s = Scheduler::instance();
	double now = s.clock ();

	if (verbose_)
		trace ((char*)"VTO %.5f _%d_ %d->%d", now, mn_->address(), mn_->address(),
			   ne->dst);
	// remove the neighbor entry from the table!
	ntab_->ent_delete(ne);
	// need to re-planarize, if option dictates
	if (use_planar_) {
		double myx, myy, myz;

		mn_->getLoc(&myx, &myy, &myz);
		ntab_->planarize(PLANARIZE_GABRIEL, mn_->address(), myx, myy, myz);
	}
}

void
GOAFR_Agent::periprobe_callback(GOAFRNeighbEnt *ne)
{
	Packet *p = allocpkt();
	struct hdr_ip *iph = HDR_IP(p);
	struct hdr_goafr *goafrh = HDR_GOAFR(p);
	struct hdr_cmn *ch = HDR_CMN(p);

	ch->next_hop_ = ne->dst;
	ch->addr_type_ = NS_AF_INET;
	iph->daddr() = Address::instance().create_ipaddr(ne->dst, RT_PORT);
	iph->dport() = RT_PORT;
	ch->ptype_ = PT_GOAFR;
	iph->ttl() = 128;
#ifdef PING_TTL
	iph->ttl() = PING_TTL;
#endif
	goafrh->hops_[0].ip = Address::instance().get_nodeaddr(addr());
	goafrh->nhops_ = 1;
	goafrh->mode_ = GOAFRH_PPROBE;
	if (use_congestion_control_)
		goafrh->load = getLoad();
	ch->size() = hdr_size(p);
	mn_->getLoc(&goafrh->hops_[0].x, &goafrh->hops_[0].y, &goafrh->hops_[0].z);

	// schedule probe transmission
	ch->xmit_failure_ = mac_callback;
	ch->xmit_failure_data_ = this;
	Scheduler::instance().schedule(target_, p, 0);
	if ((use_beacon_)&&(!use_reactive_beacon_)) {
		BEACON_RESCHED;
	}

	// schedule next probe timer
	ne->ppt.resched(pint_ +
					Random::uniform(2 * pdesync_ * pint_) - pdesync_ * pint_);
}

inline int
cross_segment(double x1, double y1, double x2, double y2,
						 double x3, double y3, double x4, double y4,
						 double *xi /*= 0*/, double *yi /*= 0*/)
{
	double dy[2], dx[2], m[2], b[2];
	double xint, yint;
	
	dy[0] = y2 - y1; // dsty - pty
	dx[0] = x2 - x1; // dstx - ptx
	dy[1] = y4 - y3; // ne->y - myy
	dx[1] = x4 - x3; // ne->x - myx
	m[0] = dy[0] / dx[0];
	m[1] = dy[1] / dx[1];
	b[0] = y1 - m[0] * x1;
	b[1] = y3 - m[1] * x3;
	if (m[0] != m[1]) {
		// slopes not equal, compute intercept
		xint = (b[0] - b[1]) / (m[1] - m[0]);
		yint = m[1] * xint + b[1];
		// is intercept in both line segments?
		if ((xint <= max(x1, x2)) && (xint >= min(x1, x2)) &&
			(yint <= max(y1, y2)) && (yint >= min(y1, y2)) &&
			(xint <= max(x3, x4)) && (xint >= min(x3, x4)) &&
			(yint <= max(y3, y4)) && (yint >= min(y3, y4))) {
			if (xi && yi) {
				*xi = xint;
				*yi = yint;
			}
			return 1;
		}
	}
	return 0;
}

int
GOAFR_Agent::crosses(GOAFRNeighbEnt *ne, hdr_goafr *goafrh)
{
	int i;

	// check all neighboring hops in perimeter thus far (through self)
	for (i = 0; i < (goafrh->nhops_ - 1); i++) {
		if ((goafrh->hops_[i].ip != ne->dst) &&
			(goafrh->hops_[i+1].ip != ne->dst) &&
			(goafrh->hops_[i].ip != goafrh->hops_[goafrh->nhops_-1].ip) &&
			(goafrh->hops_[i+1].ip != goafrh->hops_[goafrh->nhops_-1].ip) &&
			cross_segment(goafrh->hops_[i].x, goafrh->hops_[i].y,
						  goafrh->hops_[i+1].x, goafrh->hops_[i+1].y,
						  goafrh->hops_[goafrh->nhops_-1].x,
						  goafrh->hops_[goafrh->nhops_-1].y,
						  ne->x, ne->y))
			return 1;
	}
	return 0;
}

void
GOAFR_Agent::periIn(Packet *p, hdr_goafr *goafrh, int rtxflag /*= 0*/)
{
	double myx, myy, myz;
	GOAFRNeighbEnt *ne, *inne;

	// update neighbor record for previous hop
  
	// did I originate it?
	if (goafrh->hops_[0].ip == Address::instance().get_nodeaddr(addr())) {
		// cache the perimeter
		ne = ntab_->ent_finddst(goafrh->hops_[1].ip);
		if (!ne) {
			// apparently, neighbor we launched probe via is now gone
			Packet::free(p);
			return;
		}
#ifdef HDR_GOAFR_DYNAMIC
		if (ne->peri && (ne->maxlen < goafrh->maxhops_)) {
			// need to allocate more GoafrPeriEnt slots in ne
			delete[] ne->peri;
			ne->maxlen = ne->perilen = 0;
			ne->peri = NULL;
		}
#endif
		if (!ne->peri) {
#ifdef HDR_GOAFR_DYNAMIC
			ne->peri = new struct PeriEnt[goafrh->maxhops_];
			ne->maxlen = goafrh->maxhops_;
#else
			ne->peri = new struct PeriEnt[MAX_PERI_HOPS_STATIC];
			ne->maxlen = MAX_PERI_HOPS_STATIC;
#endif
		}
		bcopy(&goafrh->hops_[1], ne->peri,
			  (goafrh->nhops_ - 1) * sizeof(struct PeriEnt));
		ne->perilen = goafrh->nhops_ - 1;
		/* no timer work to do--perimeter probe timer is governed by
		   beacons/absence of beacons from a neighbor */
		// we consumed the packet; free it!
		Packet::free(p);
		return;
	}
	// add self to GOAFR header perimeter
	mn_->getLoc(&myx, &myy, &myz);
	goafrh->add_hop(Address::instance().get_nodeaddr(addr()), myx, myy, myz);
	if (use_congestion_control_)
		goafrh->load = getLoad();
	// compute candidate next hop: sweep ccw about self from ingress hop
	ne = inne = ntab_->ent_finddst(goafrh->hops_[goafrh->nhops_-2].ip);
	/* in theory, a perimeter probe received from an unknown neighbor should
	   serve as a beacon from that neighbor... */
	/* BUT, don't add the previous hop more than once when we retransmit a
	   peri probe--the prev hop information is stale in that case */
	if (!rtxflag && (ne == NULL)) {
		GOAFRNeighbEnt nne(this);

		nne.dst = goafrh->hops_[goafrh->nhops_-2].ip;
		nne.x = goafrh->hops_[goafrh->nhops_-2].x;
		nne.y = goafrh->hops_[goafrh->nhops_-2].y;
		nne.z = goafrh->hops_[goafrh->nhops_-2].z;

		if (use_congestion_control_)
			nne.load = goafrh->load;
		inne = ne = ntab_->ent_add(&nne);

		ne->dnt.sched(bexp_);
		// no perimeter probe is pending; launch one
		if (peri_proact_)
			ne->ppt.sched(pint_ +
						  Random::uniform(2 * pdesync_ * pint_) - pdesync_ * pint_);
	}
	else if (ne == NULL) {
		/* we're trying to retransmit a peri probe, but the ingress hop is gone.
		   drop it. */
		drop(p, DROP_RTR_MAC_CALLBACK);
		return;
	}
#ifndef KARP_PERI
	while ((ne = ntab_->ent_next_ccw(mn_, ne, use_planar_)) != inne) {
#else
	while((ne = ntab_->ent_findnext_onperi(mn_, goafrh->hops_[0].ip,
										   goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z,
										   use_planar_)) != inne){
		printf("ne->dst %d\n", ne->dst);
		
#endif
			// verify no crossing
			if (!crosses(ne, goafrh))
				break;			
	}
	// forward probe to ne

	struct hdr_cmn *cmh = HDR_CMN(p);
	struct hdr_ip *iph = HDR_IP(p);
	cmh->addr_type_ = NS_AF_INET;
	iph->daddr() = Address::instance().create_ipaddr(ne->dst, RT_PORT);
	cmh->size() += sizeof(struct PeriEnt);
	printf("Warning: This Packet Size change has not been modified, yet!\n");
	cmh->xmit_failure_ = mac_callback;
	cmh->xmit_failure_data_ = this;
	cmh->next_hop_ = ne->dst;
	cmh->direction() = hdr_cmn::DOWN;
	target_->recv(p, (Handler *)0);
	if ((use_beacon_)&&(!use_reactive_beacon_)) {
		BEACON_RESCHED;
	}
}

int
GOAFR_Agent::getLoad() {
	return (2 * ((Mac802_11 *)m)->getLoad() + ntab_->meanLoad()) / 3;
}

/***************************************************/
/* Forwarding Packet Function (or is it Monster ?) */
/***************************************************/

/* Added to GOAFR */
 int GOAFR_Agent::check_ellipse(Packet *p, int from_address, int to_address) {
	 
	// we find the current location 
	double myx, myy, myz;
    mn_->getLoc(&myx, &myy, &myz);

    if (!p->ellipse_->point_in_ellipsis(myx, myy)) {
	  
      if(p->ellipse_->hit_edge()) {
	    // This is the second time we hit the edge, then we should expand the ellipsis, and continue on
	    double major = p->ellipse_->get_major();
	    p->ellipse_->change_major(2 * major);
		return to_address;
	  } else {
	    // This is the first time we hit the edge, so we do not expand the ellipsis, but turn around
	   	ntab_->counter_clock = !ntab_->counter_clock;
		return from_address;		
	  }
	  
	}
	// The point is in the ellipsis, so we just continue
	return to_address;
}
	
 /*Stop added to GOAFR*/

void
GOAFR_Agent::forwardPacket(Packet *p, int rtxflag /*= 0*/) {
	struct hdr_ip *iph = HDR_IP(p);
    struct hdr_cmn *cmh = HDR_CMN(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);
    
    GOAFRNeighbEnt *ne=NULL;
    GOAFRNeighbEnt *logne;
    Scheduler &s = Scheduler::instance();
    double now = s.clock();

	// Added to support GOAFR
	
	if (p->ellipse_ == NULL) {
		int src_address = iph->saddr();
		int dst_address = iph->daddr();
		
		double srcPosX, srcPosY, srcPosZ, dstPosX, dstPosY, dstPosZ;
		// We find the locations
		God::instance()->getPosition(src_address, &srcPosX, &srcPosY, &srcPosZ);
		God::instance()->getPosition(dst_address, &dstPosX, &dstPosY, &dstPosZ);
		Point src_point = Point(srcPosX, srcPosY);
		Point dst_point = Point(dstPosX, dstPosY);
		p->ellipse_ = new Ellipsis(src_point, dst_point);
	}
	
    // End of what was added to support GOAFR
	
	switch(goafrh->mode_) {
	case GOAFRH_DATA_GREEDY: 
		
		// first of all, look if we're neighbor to dst
	    if ( (ne = ntab_->ent_finddst(iph->daddr())) != NULL) {
			cmh->next_hop_ = ne->dst;
			break;
	    }
	    
		// try to find the next best neighbor
	    if (use_congestion_control_)
			ne = ntab_->ent_findshortest_cc(mn_, iph->dx_, iph->dy_, iph->dz_, cc_alpha_);
	    else
			ne = ntab_->ent_findshortest(mn_, iph->dx_, iph->dy_, iph->dz_);

	    if (ne != NULL){

			// warn about possible ping-pong
 			// (wk: possibly problematic because if a newly born packet with next hop 0
			// arrives here, a possible ping-pong will be detected (ne->dst == 0 and ..hops[0]
			// is initialized with 0)
			if (ne->dst == goafrh->hops_[0].ip)			
				trace((char*)"VPPP %f _%d_ %d [%d -> %d]", now, mn_->address(), cmh->uid(), mn_->address(), ne->dst);

			// set next hop to best neighbor, given that we are still in greedy mode, we cannot go outside the ellipsis (or really, it doesen't matter if we do) 
			cmh->next_hop_ = ne->dst;
			break;

	    }
		
		else {
	        
			// there seems to be no greedy neighbor
			// we send a beacon request and delay the pkt the first time
			// should the new info be of no use, we'll process it further
			if (use_reactive_beacon_) {
				if (goafrh->retry < GOAFR_RBEACON_RETRIES) {
					goafrh->retry++;
					sendBeaconRequest();
					double delay = 2*GOAFR_RBEACON_JITTER;
					pd_timer->add(cmh->uid(), delay, (void*)p);
					return;
				}else{
					pd_timer->remove(cmh->uid()); // precaution
					goafrh->retry = 0;
				}
			}

			// try perimeter mode
			if ((use_peri_) && 
				// no perimeter mode for updates and handovers to avoid excessive traffic
				!((cmh->ptype() == PT_HLS) && // if NOT(HLS update or HLS handover)
				  ((HDR_HLS(p)->type_ == HLS_UPDATE)||
				   (HDR_HLS(p)->type_ == HLS_HANDOVER))))
			{
				if (verbose_) {
					double myx, myy, myz;
					mn_->getLoc(&myx, &myy, &myz);
					trace((char*)"VEPM %f _%d_ [%d -> %d] [%.2f/%.2f]",
						  now, mn_->address(), iph->saddr(), iph->daddr(),
						  myx, myy);
				}

				if (use_planar_) {
					// no proactive probes, so no peri_proact_ to worry about
					ne = ntab_->ent_findnext_onperi(mn_, iph->daddr(), iph->dx_, iph->dy_, iph->dz_, use_planar_);
					if (!ne) { // no face toward the destination
						if(goafrh->geoanycast)
							{
								// wk: forwardPacket: no better neigbhor on peri
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}
						TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());
						fprintf(stderr, "before drop\n");
						drop(p, DROP_RTR_NO_ROUTE);
						return;
					}
	
					// put packet in peri data mode, forward
					cmh->size() -= hdr_size(p); // strip data header
					goafrh->mode_ = GOAFRH_DATA_PERI;
					cmh->size() += hdr_size(p); // add peri header

					trace((char*)"Start Peri from node %d", mn_->address());

					// mark point of entry into peri data mode
					double myx, myy, myz;
					mn_->getLoc(&myx, &myy, &myz);

					// Add the ip adress
					goafrh->perips_.ip = mn_->address();
					goafrh->perips_.x = myx;
					goafrh->perips_.y = myy;
					goafrh->perips_.z = myz;


					// We make a note of the point we are starting from
					goafrh->peript_.ip = mn_->address();
					goafrh->peript_.x = myx;
					goafrh->peript_.y = myy;
					goafrh->peript_.z = myz;

					// Make it understand we are just starting out
					p->greedy_start = true;


					// We change face
					double closerx, closery;
					closerx = myx;
					closery = myy;
					while (ne->closer_pt(mn_->address(), myx, myy, myz,
					          			 goafrh->peript_.x, goafrh->peript_.y,
										 goafrh->periptip_[1], goafrh->periptip_[0],
										 iph->dx_, iph->dy_, &closerx, &closery)) {
					  
					  // re-use single-hop history
					  goafrh->hops_[goafrh->nhops_-1].ip = ne->dst;
					  goafrh->hops_[goafrh->nhops_-1].x = ne->x;
					  goafrh->hops_[goafrh->nhops_-1].y = ne->y;
					  goafrh->hops_[goafrh->nhops_-1].z = ne->z;

					  goafrh->perips_.x = closerx;
					  goafrh->perips_.y = closery;
					  goafrh->perips_.z = 0.0;
														
					  GOAFRNeighbEnt *ne_temp = ntab_->ent_findnext_onperi(mn_, ne->dst, ne->x, ne->y, ne->z, use_planar_);
				      if((ne_temp == NULL) || (ne_temp->dst == ne->dst))
				  		  break;
					  ne = ne_temp;
					}
					// mark ips of edge endpoints
					goafrh->periptip_[0] = goafrh->hops_[0].ip;        // prev edge on peri
					goafrh->periptip_[1] = mn_->address(); // myself
					goafrh->periptip_[2] = ne->dst;        // next edge on peri

					// N.B. first dst hop is hops_[1]
					// (leave room for hop-by-hop ip, position in hops_[0])!!
					goafrh->nhops_ = 1;
					goafrh->currhop_ = 1;
					goafrh->add_hop(mn_->address(), goafrh->peript_.x, goafrh->peript_.y, goafrh->peript_.z);
					cmh->next_hop_ = ne->dst;
					break;
				}

				if (peri_proact_) {

					// record we had a data packet that needed a perimeter
					if (lastperi_timer_) { lastperi_timer_->resched(lpexp_); }

					ne = ntab_->ent_findnext_onperi(mn_,
													goafrh->hops_[0].ip, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z,
													use_planar_);

					if (!ne) {

						// we're well and truly hung; nothing closer on a peri, either
						if (drop_debug_ && (cmh->opt_num_forwards_ != 16777215)) {
							GOAFRNeighbTableIter ni;
							ni = ntab_->InitLoop();
							while ((logne = ntab_->NextLoop(&ni))) {
								trace((char*)"VPER _%d_ (%.5f, %.5f):", logne->dst, logne->x, logne->y);
								for (int j = 0; j < logne->perilen; j++) {
									trace((char*)"VPER\t\t_%d_ (%.5f, %.5f)",
										  logne->peri[j].ip, logne->peri[j].x, logne->peri[j].y);
								}
							}
						}
 						if(goafrh->geoanycast)
							{
								// wk forwardPacket: we're hung
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

						TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());
						drop(p, DROP_RTR_NO_ROUTE);
						return;

					}else{
			    
						cmh->size() -= hdr_size(p); // strip data header 
						goafrh->mode_ = GOAFRH_DATA_PERI; // put packet in peri mode
						cmh->size() += hdr_size(p); // add peri header

						// N.B. first dst hop is hops_[1]
						// (leave room for hop-by-hop ip, position in hops_[0])!! 
						goafrh->nhops_ = 1;
						goafrh->currhop_ = 1;
						goafrh->hops_[1].ip = ne->dst;
						cmh->next_hop_ = goafrh->hops_[1].ip;
						trace((char*)"VSM->P %f _%d_ [%d -> %d]", now, mn_->address(), iph->saddr(), iph->daddr());
						break;
					}
				} // if(peri_proact)
			} // if(use_peri)
		
			// no closer neighbor ! unforwardable; drop it.
			
			/* 
			   someday, we may want to queue up packets for 
			   currently unforwardable destinations 
			*/
		
			// record we had a data packet that needed a perimeter
			if (lastperi_timer_) { lastperi_timer_->resched(lpexp_); }
			// we could have used a perimeter here--turn them on
			peri_proact_ = 1;

			if(goafrh->geoanycast)
				{
					// wk forwardPacket no closer  neighbor
					locservice_->dropPacketCallback(p);
					if (p==NULL) { return; }
				}
              
			TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());
			drop(p, DROP_RTR_NO_ROUTE);
			fprintf(stderr, "f-packet droped\n");
			
			char buf[1024];

			snprintf (buf, 1024, "f-packet droped\n");

			return;
		}
	    break;
	    /********************************************
	     *end greedy
	     *******************************************/

	case GOAFRH_DATA_PERI:
	    // first of all, look if we're neighbor to dst
	    if ( (ne = ntab_->ent_finddst(iph->daddr())) != NULL) {
			cmh->size() -= hdr_size(p); // strip data peri header
			goafrh->mode_ = GOAFRH_DATA_GREEDY;
			trace((char*)"Back to Greedy\n");
			cmh->size() += hdr_size(p); // strip data goafr header
			goafrh->nhops_ = 1;
			goafrh->currhop_ = 1;
			goafrh->hops_[1].ip = ne->dst;
			cmh->next_hop_ = ne->dst;
			break;
	    }

	    if (use_peri_) {
	      
			double myx, myy, myz;

			// non-source-routed perimeter forwarding rule
			mn_->getLoc(&myx, &myy, &myz);

			double bestx, besty, bestz;
			bestx = goafrh->perips_.x;
			besty = goafrh->perips_.y;
			bestz = goafrh->perips_.z;
			
			double old_distance = distance(bestx, besty, bestz, iph->dx_, iph->dy_, iph->dz_);
			double new_distance = distance(myx  , myy  , myz  , iph->dx_, iph->dy_, iph->dz_);
		
			// We record when we arrive at a node that is closer to the destination than the current best
			if (new_distance < old_distance) {	
				trace((char*)"Change best point from %d, %3.f to %d %3.f\n", goafrh->perips_.ip, old_distance, mn_->address(), new_distance);
				goafrh->perips_.ip = mn_->address();
				goafrh->perips_.x = myx;
				goafrh->perips_.y = myy;
				goafrh->perips_.z = myz;                
			}

			if(use_planar_){
				// forward along current face, or change faces where appropriate
				/* don't choose *any* edge--only consider edges on the
				   face we're forwarding on at the moment. */
				for(int i=0; i<goafrh->nhops_; i++)
					trace((char*)"Vne %.8f _%d_ <- %d", CURRTIME, mn_->address(), goafrh->hops_[i].ip);

				ne = ntab_->ent_finddst(goafrh->hops_[goafrh->nhops_-1].ip);
				if(ne){
					double fromx, fromy, fromz;
					ne = ntab_->ent_findnext_onperi(mn_,
													goafrh->hops_[0].ip, 
                                                    goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z,
													use_planar_);

					// At this point we have to check whether we have returend to the point where we entered the periferiy search
					mn_->getLoc(&fromx, &fromy, &fromz);					
					double distance_to_point = distance(fromx, fromy, fromz, goafrh->peript_.x, goafrh->peript_.y,  goafrh->peript_.z); 

					if (!p->greedy_start && (goafrh->peript_.ip == mn_->address() || distance_to_point < 2) && !p->reversed()) {
						// We have returned to the start point, and we have not reversed our direction, we then go forward -- GOAFR

						trace((char*)"Start the advance from %d to %d.", mn_->address(), goafrh->perips_.ip);
						goafrh->mode_ = GOAFRH_DATA_ADVANCE; // put packet in advance mode
						// Set the next hop
						goafrh->nhops_ = 1;
						goafrh->currhop_ = 1;
						goafrh->hops_[1].ip = mn_->address();
						cmh->next_hop_ = goafrh->hops_[1].ip; // we jump to this node and then we advance
						break; // get out of this and forward the node
						
					}
					p->greedy_start = false;
			
					// possible error
					// does the candidate next edge have a closer pt?
					if (!ne) {
						// no face toward the destination
						if(goafrh->geoanycast)
							{
								// wk forwardPaket
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

						drop(p, DROP_RTR_NO_ROUTE);
						return;
					}
					
				} /*end of if(ne): incoming node isn't in the neighbor-table!!! */
	    		  
				// forward to next ccw neighbor from ingress edge
				/* in theory, a data peri packet received from an unknown neighbor
				   should serve as a beacon from that neighbor... */
				/* BUT, don't add the previous hop more than once when we retransmit a
				   packet--the prev hop information is stale in that case */

				if (ne == NULL) {
					// XXX might we now be able to forward anyway?? know loc of prev hop.
					/* we're trying to retransmit a packet, but the ingress hop is
					   gone. drop it. */
					// a drop due to MAC_CALLBACK. For the moment, inform it
					if(goafrh->geoanycast)
						{
							// wk forwardPaket
							locservice_->dropPacketCallback(p);
							if (p==NULL) { return; }
						}
					// a drop due to MAC_CALLBACK. For the moment,don't inform it
					trace((char*)"VneNULL %.8f _%d_ <- %d", CURRTIME, mn_->address(), goafrh->hops_[goafrh->nhops_-1].ip);
					drop(p, DROP_RTR_MAC_CALLBACK);
					return;
				}
				cmh->next_hop_ =  check_ellipse(p, mn_->address(), ne->dst);
				
				if (cmh->next_hop_ == mn_->address()) {
					p->reverse_direction();
				}

				if (use_loop_detect_) {
					goafrh->add_hop(mn_->address(), myx, myy, myz);
					printf("Warning: This size change has not been modified, yet!\n");
					cmh->size() += 12;
				}
				else {
					goafrh->hops_[goafrh->nhops_-1].ip = mn_->address();
					goafrh->hops_[goafrh->nhops_-1].x = myx;
					goafrh->hops_[goafrh->nhops_-1].y = myy;
					goafrh->hops_[goafrh->nhops_-1].z = myz;
				}
			} // end if(use_planar_)
			else {
		
				// am I the right waypoint?
				if (goafrh->hops_[goafrh->currhop_].ip == mn_->address()) {
					// am I the final waypoint?
					if (goafrh->currhop_ == (goafrh->nhops_-1)) {
						// yes! return packet to goafr mode
						ntab_->counter_clock = true; // next peri -> route counterclockwise
						cmh->size() -= hdr_size(p); // strip data peri header
						goafrh->mode_ = GOAFRH_DATA_GREEDY;
						trace((char*)"Back to Greedy\n");
						cmh->size() += hdr_size(p); // strip data goafr header
						goafrh->currhop_ = 0;
						goafrh->nhops_ = 0;
						forwardPacket(p);
						return;
					}
					else {
						// forward using source route...
						ne = ntab_->ent_findnext_onperi(mn_,
														goafrh->hops_[0].ip, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z,
														use_planar_);

						if(!ne){
							if(goafrh->geoanycast)
							{
								// wk forwardPacket 
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

							drop(p, DROP_RTR_NO_ROUTE);
							return;
						}

						goafrh->currhop_++;
						goafrh->hops_[goafrh->currhop_].ip = ne->dst;
						cmh->next_hop_ = goafrh->hops_[goafrh->currhop_].ip;
			    
					}
				}
				else {
					if(goafrh->geoanycast)
							{
								// wk forwardPacket
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

					// topology must have changed; I'm not the right hop
					TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());

					drop(p, DROP_RTR_NO_ROUTE);
					return;
				}
			}
	    } // end if(use_peri_)
	    else {
			fprintf(stderr,
					"yow! got peri mode packet when not using perimeters!\n");
			abort();
	    }
	    break;
		
	case GOAFRH_DATA_ADVANCE:
		// The node will now advance to the node that I found to be the closest to the sink

		// first of all, look if we're neighbor to dst - just in case
	    if ( (ne = ntab_->ent_finddst(iph->daddr())) != NULL) {
			trace((char*)"Advanced to _%d_", goafrh->perips_.ip);
			cmh->size() -= hdr_size(p); // strip data peri header
			goafrh->mode_ = GOAFRH_DATA_GREEDY;
			trace((char*)"Back to Greedy\n");
			cmh->size() += hdr_size(p); // strip data goafr header
			goafrh->nhops_ = 1;
			goafrh->currhop_ = 1;
			goafrh->hops_[1].ip = ne->dst;
			cmh->next_hop_ = ne->dst;
			break;
	    }

	    if (use_peri_) {
	      

			// non-source-routed perimeter forwarding rule
			/* to resume goafr forwarding, this *node* must be closer than 
			the point where the packet entered peri mode. */ 
			double myx, myy, myz;
		    mn_->getLoc(&myx, &myy, &myz);
			double pt_distance = distance(goafrh->perips_.x, goafrh->perips_.y, goafrh->perips_.z,
									   myx              , myy              , myz              );

			// Cutoff function, when arrive at the closest node, so we go back to business as usual
			if (mn_->address() == goafrh->perips_.ip || pt_distance < 5) {
				trace((char*)"Advanced to _%d_", goafrh->perips_.ip);
				cmh->size() -= hdr_size(p); // strip data peri header
				goafrh->mode_ = GOAFRH_DATA_GREEDY;
				cmh->size() += hdr_size(p); // add data goafr header
				trace((char*)"Back to Greedy\n");

				/*
				 always add back (- - is +) 12 bytes: if use_implicit_beacon_,
				   src added 12 to size, don't re-add hops_[0]; otherwise,
				   still don't want to count hops_[0]. 
				*/
		
		        goafrh->currhop_ = 0;
				goafrh->nhops_ = 0;
				
				// recursive, but must call target_->recv in callee frame
				trace((char*)"VSM->G %f _%d_ [%d -> %d]", now, mn_->address(), iph->saddr(), iph->daddr());
		
				forwardPacket(p);
				return;
			}

			if(use_planar_){
				// forward along current face, or change faces where appropriate
				/* don't choose *any* edge--only consider edges on the
				   face we're forwarding on at the moment. */
		
		        for(int i=0; i<goafrh->nhops_; i++)
					trace((char*)"Vne %.8f _%d_ <- %d", CURRTIME, mn_->address(), goafrh->hops_[i].ip);

				ne = ntab_->ent_finddst(goafrh->hops_[goafrh->nhops_-1].ip);
				if(ne){
					ne = ntab_->ent_findnext_onperi(
                    mn_, goafrh->hops_[0].ip, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z, use_planar_);

					/** drop if we've looped on this perimeter:
						are about to revisit the first edge we took on it */
					// If we arrive here, then something has gone wrong, and we drop the message 
					// Ensure that this does not happen the instant we begin routing
					/*
         			if ((goafrh->periptip_[1] == mn_->address()) &&
						(goafrh->periptip_[2] == ne->dst)) {

						if(goafrh->geoanycast)
							{
								// wk forwardPacket, finished perimeter 
								// without finding target
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

						// The graph has changed
						TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());
						drop(p, DROP_RTR_NO_ROUTE);
						return;
					}
					*/
					// does the candidate next edge have a closer pt?
					if (!ne) {
						// no face toward the destination
						if(goafrh->geoanycast)
							{
								// wk forwardPaket
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}
						trace((char*)"Advance drop at node %d, best node is %d", mn_->address(), goafrh->perips_.ip);

						drop(p, DROP_RTR_NO_ROUTE);
						return;
					}

				} /*end of if(ne): incoming node isn't in the neighbor-table!!! */
	    		  
				// forward to next ccw neighbor from ingress edge
				/* in theory, a data peri packet received from an unknown neighbor
				   should serve as a beacon from that neighbor... */
				/* BUT, don't add the previous hop more than once when we retransmit a
				   packet--the prev hop information is stale in that case */

				if (ne == NULL) {
					// XXX might we now be able to forward anyway?? know loc of prev hop.
					/* we're trying to retransmit a packet,b ut the ingress hop is
					   gone. drop it. */
					// a drop due to MAC_CALLBACK. For the moment, inform it
					if(goafrh->geoanycast)
						{
							// wk forwardPaket
							locservice_->dropPacketCallback(p);
							if (p==NULL) { return; }
						}
					// a drop due to MAC_CALLBACK. For the moment,don't inform it
					trace((char*)"VneNULL %.8f _%d_ <- %d", CURRTIME, mn_->address(), goafrh->hops_[goafrh->nhops_-1].ip);
					drop(p, DROP_RTR_MAC_CALLBACK);
					return;
				}
				cmh->next_hop_ = ne->dst;
				if (use_loop_detect_) {
					goafrh->add_hop(mn_->address(), myx, myy, myz);
					printf("Warning: This size change has not been modified, yet!\n");
					cmh->size() += 12;
				}
				else {
					goafrh->hops_[goafrh->nhops_-1].ip = mn_->address();
					goafrh->hops_[goafrh->nhops_-1].x = myx;
					goafrh->hops_[goafrh->nhops_-1].y = myy;
					goafrh->hops_[goafrh->nhops_-1].z = myz;
				}
			} // end if(use_planar_)
			else {
		
  
				// am I the right waypoint?
				if (goafrh->hops_[goafrh->currhop_].ip == mn_->address() ||
					distance(goafrh->perips_.x, goafrh->perips_.y, goafrh->perips_.z,
							 myx              , myy              , myz              ) < 5) {
					// am I the final waypoint?
					if (goafrh->currhop_ == (goafrh->nhops_-1)) {
						trace((char*)"Advanced to _%d_", goafrh->perips_.ip);
						// yes! return packet to greedy mode
						ntab_->counter_clock = true; // next peri -> route counterclockwise
						cmh->size() -= hdr_size(p); // strip data peri header
						goafrh->mode_ = GOAFRH_DATA_GREEDY;
						cmh->size() += hdr_size(p); // strip data goafr header
						trace((char*)"Back to Greedy\n");

						goafrh->currhop_ = 0;
						goafrh->nhops_ = 0;
						forwardPacket(p);
						return;
					}
					else {
						// forward using source route...
						ne = ntab_->ent_findnext_onperi(mn_,
														goafrh->hops_[0].ip, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z,
														use_planar_);

						if(!ne){
							if(goafrh->geoanycast)
							{
								// wk forwardPacket 
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

							drop(p, DROP_RTR_NO_ROUTE);
							return;
						}

						goafrh->currhop_++;
						goafrh->hops_[goafrh->currhop_].ip = ne->dst;
						cmh->next_hop_ = goafrh->hops_[goafrh->currhop_].ip;
			    
					}
				}
				else {
					if(goafrh->geoanycast)
							{
								// wk forwardPacket
								locservice_->dropPacketCallback(p);
								if (p==NULL) { return; }
							}

					// topology must have changed; I'm not the right hop
					TRACE_CONN(p,addr(),addr(),HDR_IP(p)->daddr());

					drop(p, DROP_RTR_NO_ROUTE);
					return;
				}
			}
	    } // end if(use_peri_)
	    else {
			fprintf(stderr,
					"yow! got peri mode packet when not using perimeters!\n");
			abort();
	    }
        break;
	default:
	    fprintf(stderr, "yow! got non-data packet in forward_packet()!\n");
	    abort();
	    break;
	
    }
    
 finish_pkt:

    // pass along
    cmh->addr_type_ = NS_AF_INET;
    cmh->xmit_failure_ = mac_callback;
    cmh->xmit_failure_data_ = this;
    
    // point the packet *down* the stack
    cmh->direction() = hdr_cmn::DOWN;

    // data packet can serve as implicit beacon; put self in hops_[0]
    double myx, myy, myz;
    mn_->getLoc(&myx, &myy, &myz);
    /* the packet may *already* have hops stored; don't allocate with
       add_hop()! */
    goafrh->hops_[0].ip = mn_->address();
    goafrh->hops_[0].x = myx;
    goafrh->hops_[0].y = myy;
    goafrh->hops_[0].z = myz;

    // reactive beaconing requires, that a retried pkt
    // is marked as clean again, so it can be retried
    // at the next node
    if (use_reactive_beacon_) { goafrh->retry = 0; }
    
    if (use_congestion_control_)
		goafrh->load = getLoad();

    if (verbose_)
		trace ((char*)"VFP %.5f _%d -> %d_ %d:%d -> %d:%d", now, mn_->address(),
			   ne->dst,
			   Address::instance().get_nodeaddr(iph->saddr()),
			   iph->sport(),
			   Address::instance().get_nodeaddr(iph->daddr()),
			   iph->dport() );
    target_->recv(p, (Handler *)0);
 
    if (use_beacon_) {
		if ((use_implicit_beacon_)&&(!use_reactive_beacon_))
			BEACON_RESCHED;
    }
}

/***********/
/* Receive */
/***********/

void
GOAFR_Agent::recv(Packet *p, Handler *) {

    // Check if this node is awake
    if (!active_) {
		Packet::free(p);
		p = NULL;
		return;
    }

    // Check if Locservice is interested in this pkt
    locservice_->recv(p);
    if (p==NULL) { return; }
    /*
      Check if GOAFR is interested in this pkt
    */

    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_cmn *cmh = HDR_CMN(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    int src = Address::instance().get_nodeaddr(iph->saddr());

    // Filter pkts I originated
    if (src == mn_->address()) {

		if (cmh->num_forwards() == 0) {
	    
            // Fresh pkt that needs setting up
			if (iph->dport() != RT_PORT) { 
				goafrh->mode_ = GOAFRH_DATA_GREEDY; 
			} // non-route pkts
			goafrh->geoanycast = true;
			if (goafrh->port_ != hdr_goafr::LOCS) { cmh->size() += IP_HDR_LEN; }   // non-ls pkts
			cmh->size() += hdr_size(p);
			// HLS packets do their own TTL management
			if(cmh->ptype() != PT_HLS)
				{
					iph->ttl_ = 128;
#ifdef PING_TTL
					iph->ttl_ = PING_TTL;
#endif
				}

			// Lookup Position Information
			if (!locservice_->poslookup(p)){
				stickPacketInSendBuffer(p);
				return;
			}

		}else{

			if ((goafrh->port_ != hdr_goafr::LOCS) && (goafrh->mode_ == GOAFRH_DATA_GREEDY)) {
				// No real data pkt should visit its source twice 
				drop(p, DROP_RTR_ROUTE_LOOP);
				return;
			}
		}
    }
    
	// Check for expired TTL
    if ((src == mn_->address()) && (cmh->num_forwards() == 0)) {
		// Originating Packets should have their TTL
		// decreased for the first hop
    } else {
		if (--iph->ttl_ <= 0) {
			
			// only Requests and replies are routed in peri mode (where TTL
			// expiration is likely to occur) we can assume that there 
			// exists no route.
			
			if(goafrh->geoanycast)
				{
					// if there is a TTL problem, we treat at least the
					// requests 
					locservice_->dropPacketCallback(p);
					//printf("drop packet due to TTL\n");
					if (p==NULL) { return; }
				}
			
			drop(p, DROP_RTR_TTL);
			return;
		}
    }

    /*
      Forwarding Packet
    */
    
    // LOCS Packets
    if (goafrh->port_ == hdr_goafr::LOCS) {
		forwardPacket(p); 
		return; 
    }
    
    // GOAFR Packets (Routing Packets)
    if (iph->dport() == RT_PORT) {
		char *as;

		switch (goafrh->mode_) {
	    case GOAFRH_BEACON:
			if (src != mn_->address()) { recvBeacon(p); }
			break;
	    case GOAFRH_BEACON_REQ:
			if (src != mn_->address()) { recvBeaconReq(p); }
			break;
	    case GOAFRH_PPROBE:
			periIn(p, goafrh);
			break;
	    case GOAFRH_DATA_GREEDY:
			as = Address::instance().print_nodeaddr(addr());
			fprintf(stderr, "goafr data pkt @ %s:RT_PORT!\n",as); fflush(stderr);
			delete[] as;
			break;
	    case GOAFRH_DATA_PERI:
			as = Address::instance().print_nodeaddr(addr());
			fprintf(stderr, "peri data pkt @ %s:RT_PORT!\n",as); fflush(stderr);
			delete[] as;
			break;
        case GOAFRH_DATA_ADVANCE:
			as = Address::instance().print_nodeaddr(addr());
			fprintf(stderr, "advance data pkt @ %s:RT_PORT!\n",as); fflush(stderr);
			delete[] as;
			break;
	    default:
			as = Address::instance().print_nodeaddr(addr());
			fprintf(stderr, "unk pkt type %d @ %s:RT_PORT!\n", goafrh->mode_,as); fflush(stderr);
			delete[] as;
			break;
		}
		return;
    }

    // Everything else
    forwardPacket(p);
}

/****************************/
/* Startup/Setup Functions  */
/****************************/

int
GOAFR_Agent::command(int argc, const char *const *argv) {
    
    if (argc == 2) {
		if (strcmp(argv[1], "start-goafr") == 0) {
			init();
			return TCL_OK;
		}
		/* -> [HMF] */
		if (strcasecmp(argv[1], "resetSB") == 0) {
			Terminate();
			return TCL_OK;
		}
		if (strcmp(argv[1], "sleep") == 0){
			if (active_) { sleep(); }
			return TCL_OK;
		}
		if (strcmp(argv[1], "wake") == 0){
			if (!active_) { wake(); }
			return TCL_OK;
		}
		/* <- */
    }  
    if (argc == 3) {

		if (strcasecmp(argv[1], "test-query") == 0) {
			int dst = atoi(argv[2]);

			// Generate Dummy Packet
			Packet* pkt = allocpkt();
			struct hdr_ip* iph = HDR_IP(pkt);
			struct hdr_cmn* cmnh = HDR_CMN(pkt);

			iph->saddr() = addr();
			iph->daddr() = dst;
			iph->ttl() = 1;
			cmnh->ptype() = PT_PING;
			cmnh->addr_type_ = NS_AF_INET;
			cmnh->num_forwards() = 0;
			cmnh->next_hop_ = NO_NODE;
			cmnh->size() = size_;
			cmnh->xmit_failure_ = 0;
			cmnh->direction() = hdr_cmn::DOWN;

			double srcPosX, srcPosY, srcPosZ, dstPosX, dstPosY, dstPosZ;
			God::instance()->getPosition(addr(), &srcPosX, &srcPosY, &srcPosZ);
			God::instance()->getPosition(dst, &dstPosX, &dstPosY, &dstPosZ);			
			trace((char*)"TESTQ %.12f %d (%.2f %.2f) %d (%.2f %.2f)", 
				  Scheduler::instance().clock(), // timestamp
				  addr(), // source of the query
				  srcPosX,// source position x
				  srcPosY,// source position y
				  dst,    // target of the query
				  dstPosX,// destination position x 
				  dstPosY);// destination position y
			// Query Target
			locservice_->poslookup(pkt);

			// Delete Dummy Packet
			Packet::free(pkt);
			return TCL_OK;
		}

		TclObject *obj;
		if (strcasecmp(argv[1], "tracetarget") == 0) {
			if ((obj = TclObject::lookup(argv[2])) == 0) {
				fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1], argv[2]);
				return TCL_ERROR;
			}
			tracetarget = (Trace *) obj;
			locservice_->setTraceTarget((Trace *)obj);
			return TCL_OK;
		}
		if (strcmp(argv[1], "install-tap") == 0) {
			if ((obj = TclObject::lookup(argv[2])) == 0) {
				fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1], argv[2]);
				return TCL_ERROR;
			}
			m = (Mac *) obj;
			m->installTap(this);
			return TCL_OK;
		}
		if (strcmp(argv[1], "node") == 0) {
			if ((obj = TclObject::lookup(argv[2])) == 0) { return TCL_ERROR; }
			mn_ = (MobileNode *) obj;
			locservice_->setMobileNode((MobileNode *)obj);
			return TCL_OK;
		}
		if (strcmp(argv[1], "ldb") == 0) {
			if ((obj = TclObject::lookup(argv[2])) == 0) { return TCL_ERROR; }
			return TCL_OK;
		}
		if (strcmp(argv[1], "if-queue") == 0) {
			ifq_ = (PriQueue*) TclObject::lookup(argv[2]);
			if(ifq_ == 0) { return TCL_ERROR; }
			return TCL_OK;
		}
    }
    return (Agent::command (argc, argv));
}

void
GOAFR_Agent::init(void) {

    // Init LocService
    locservice_->init();
    locservice_->setTarget(target_);
  
    if (active_) {
		God::instance()->signOn(addr());
#ifdef GOAFR_TRACE_WAKESLEEP
		trace((char*)"VGI: %f %d", Scheduler::instance().clock(), mn_->address());
#endif

		if ((use_beacon_)&&(!use_reactive_beacon_)) {
			beacon_timer_->sched(Random::uniform(bint_)); 
		}
		if (!peri_proact_)    { lastperi_timer_->sched(lpexp_); }
		if (use_timed_plnrz_) { planar_timer_->sched(1.0); }
    
		// Init SendPermissions
		for (unsigned int i=0;i<GOAFR_PKT_TYPES;i++) {
			send_allowed[i] = true;
		}
	
		send_buf_timer.sched(sendbuf_interval());
    }
}

void
GOAFR_Agent::sleep() {
    
    assert(active_);
    
    // Lowlevel
    m->sleep();
    if (ifq_) { ((PriQueue *)ifq_)->clear(); }
    
#ifdef GOAFR_TRACE_WAKESLEEP
    trace((char*)"VGSLEEP %f _%d_ ", Scheduler::instance().clock(), mn_->address());
#endif

    // Stop SendPermissions
    for (unsigned int i=0;i<GOAFR_PKT_TYPES;i++) {
		send_allowed[i] = false;
    } 

    // Clean SendBuffer
    send_buf_timer.force_cancel();
    for (int c=0; c < SEND_BUF_SIZE; c++) {
		if (send_buf[c].p != NULL) {
			// wk sleep, not necessary to inform locservice, I cant receive the packet
			// due to sleep
			drop(send_buf[c].p, DROP_RTR_SLEEP);
			send_buf[c].p = NULL;
		}
    }

    if ((use_beacon_)&&(!use_reactive_beacon_)) { 
		beacon_timer_->force_cancel(); 
    }

    if (!peri_proact_)    { lastperi_timer_->force_cancel(); }
    if (use_timed_plnrz_) { planar_timer_->force_cancel(); }
    
    // Shutdown LocService
    locservice_->sleep();

    God::instance()->signOff(addr());
    active_ = false;
}

void
GOAFR_Agent::wake() {

    assert(!active_);


    active_ = true;
    God::instance()->signOn(addr());

    // Wakeup LocService
    locservice_->wake();

    // Setup SendBuffer
    send_buf_timer.sched(sendbuf_interval());

    // Init SendPermissions
    for (unsigned int i=0;i<GOAFR_PKT_TYPES;i++) {
		send_allowed[i] = true;
    }

#ifdef GOAFR_TRACE_WAKESLEEP    
    trace("VGWAKE %f _%d_", Scheduler::instance().clock(), mn_->address());
#endif

    if ((use_beacon_)&&(!use_reactive_beacon_)) {  
		beacon_timer_->sched(Random::uniform(bint_)); 
    }
    if (!peri_proact_)    { lastperi_timer_->sched(lpexp_); }
    if (use_timed_plnrz_) { planar_timer_->sched(1.0); }

    // LowLevel
    m->wakeup();
} 

/***********************/
/* Beaconing Functions */
/***********************/

void
GOAFR_Agent::beacon_proc(int src, double x, double y, double z, int load)
{
	GOAFRNeighbEnt *ne;
	GOAFRNeighbEnt nne(this);

	double now = Scheduler::instance().clock();
	nne.dst = src;
	nne.x = x; nne.y = y; nne.z = z;
	nne.ts = now;
	nne.load = load;

	ne = ntab_->ent_add(&nne);

	if (false) { trace((char*)"VBP %f _%d_ [%d/%.2f/%.2f]", now, mn_->address(), src, x, y); }
	{
		// entry wasn't in table before. need to planarize, if option dictates.
		ne->live = 1;
		if (use_planar_) {
			double myx, myy, myz;

			mn_->getLoc(&myx, &myy, &myz);
			
			if((bool)Random::uniform(1))
				ntab_->planarize(PLANARIZE_GABRIEL, mn_->address(), myx, myy, myz);
			
		}
	}
	ne->dnt.resched(bexp_);
}

void
GOAFR_Agent::recvBeacon(Packet *p) {

    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    int src = Address::instance().get_nodeaddr(iph->saddr());

    beacon_proc(src, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z, goafrh->load);
    Packet::free(p);
}

void
GOAFR_Agent::recvBeaconReq(Packet *p) {

    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    // Evaluate Beacon Req Information
    int src = Address::instance().get_nodeaddr(iph->saddr());
    beacon_proc(src, goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z, goafrh->load);

    // Answer Request with a Beacon
    double delay = Random::uniform(GOAFR_RBEACON_JITTER);
    sendBeacon(delay);

    // Discard Request
    Packet::free(p);
}

void
GOAFR_Agent::sendBeaconRequest() {
    
    assert(active_);

    if (!allowedToSend(GOAFRH_BEACON_REQ)) { return; }

    Packet *p = allocpkt();

    struct hdr_cmn *hdrc = HDR_CMN(p);
    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);
  
    // Set up Beacon Headers
    goafrh->mode_ = GOAFRH_BEACON_REQ;
    goafrh->nhops_ = 1;
    mn_->getLoc(&goafrh->hops_[0].x, &goafrh->hops_[0].y, &goafrh->hops_[0].z);

    hdrc->ptype_ = PT_GOAFR;
    hdrc->next_hop_ = IP_BROADCAST;
    hdrc->addr_type_ = NS_AF_INET;
    hdrc->size() = hdr_size(p);

    iph->daddr() = IP_BROADCAST << Address::instance().nodeshift();
    iph->dport() = RT_PORT;

    if (use_congestion_control_) {
		goafrh->load = getLoad();
    }
    
    beaconreq_delay_->resched(GOAFR_BEACON_REQ_DELAY);
    block(GOAFRH_BEACON_REQ);

    Scheduler::instance().schedule(target_, p, 0.0);
}

void
GOAFR_Agent::checkGoafrCondition(const Packet *p) {

    // If we announced ourselves not long ago, we
    //  don't need to do it again
    if (!allowedToSend(GOAFRH_BEACON)) { return; }

    struct hdr_ip *iph = HDR_IP(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    // Data Goafr Packets should be checked, to see
    //  if i'm on their goafr path. if so, we'll 
    //  send a beacon to announce our position

    if (goafrh->mode_ == GOAFRH_DATA_GREEDY) {
	
		double mydist, shortest, myx, myy, myz;

		mn_->getLoc(&myx, &myy, &myz);
		shortest = distance(goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z, 
							iph->dx_, iph->dy_, iph->dz_);
		mydist   = distance(goafrh->hops_[0].x, goafrh->hops_[0].y, goafrh->hops_[0].z, 
							myx, myy, myz);
		if (mydist < shortest) {
			double delay = Random::uniform(GOAFR_RBEACON_JITTER);
			sendBeacon(delay);
			beacon_delay_->resched(GOAFR_BEACON_DELAY);
			block(GOAFRH_BEACON);
		}
    }
}

void
GOAFR_Agent::sendBeacon(double delay) {

	assert(active_);

	Packet *p = allocpkt();

	struct hdr_cmn *hdrc = HDR_CMN(p);
	struct hdr_ip *iph = HDR_IP(p);
	struct hdr_goafr *goafrh = HDR_GOAFR(p);
	// Set up Beacon Headers
	goafrh->mode_ = GOAFRH_BEACON;
	goafrh->nhops_ = 1;
	mn_->getLoc(&goafrh->hops_[0].x, &goafrh->hops_[0].y, &goafrh->hops_[0].z);

	hdrc->ptype_ = PT_GOAFR;
	hdrc->next_hop_ = IP_BROADCAST;
	hdrc->addr_type_ = NS_AF_INET;
	hdrc->size() = hdr_size(p);

	iph->daddr() = IP_BROADCAST << Address::instance().nodeshift();
	iph->dport() = RT_PORT;

	if (use_congestion_control_) {
		goafrh->load = getLoad();
	}

	Scheduler::instance().schedule(target_, p, delay);
}

/********************/
/* PacketDelayTimer */
/********************/

void
GOAFRPacketDelayTimer::handle() {
    a->forwardPacket((Packet*)local_info);
}

void
GOAFRPacketDelayTimer::deleteInfo(void* info) {
    Packet::free((Packet*)info);
}

/**************************************/
/* Location Service Related Functions */
/**************************************/

void
GOAFR_Agent::Terminate()
{
    for (int c=0; c<SEND_BUF_SIZE; c++) {
		if (send_buf[c].p) {
			drop(send_buf[c].p, DROP_END_OF_SIMULATION);
			send_buf[c].p = NULL;
		}
    }
}

void
GOAFR_Agent::notifyPos(nsaddr_t id)
{
    struct hdr_ip *iph;
    struct hdr_cmn *cmnh;
    struct hdr_locs *locsh;
    
    for (int c=0; c<SEND_BUF_SIZE; c++) {
		if (send_buf[c].p == NULL) continue;
		iph = HDR_IP(send_buf[c].p); 
		if (iph->daddr() == id) {   
			if (locservice_->poslookup(send_buf[c].p)){
				if ((false)&&(iph->daddr()==id)) {
					cmnh = HDR_CMN(send_buf[c].p);
					locsh = HDR_LOCS(send_buf[c].p);
		    
					double dstx, dsty, dstz;
					God::instance()->getPosition(iph->daddr(), &dstx, &dsty, &dstz);
		    
					trace((char*)"SB %.5f _%d_ %d unusual send [%d %.4f %.2f %.2f] (%.2f %.2f)", 
						  Scheduler::instance().clock(), 
						  mn_->address(),
						  cmnh->uid(),
						  locsh->dst.id, locsh->dst.ts, locsh->dst.loc.x, locsh->dst.loc.y,
						  dstx, dsty);
				}
				// Maybe we shouldn't send all the Pakets at once, 
				// but rather schedule them one by one... ?
				forwardPacket(send_buf[c].p);
				send_buf[c].p = NULL;
			}
		}
    }
}

void
GOAFR_Agent::stickPacketInSendBuffer(Packet *p)
{
	double min = 99999.0; //initialize min to some big enough number
	int min_index = 0;

	struct hdr_ip *iph = HDR_IP(p); 

	if (verbose_)
		trace((char*)"SB %.5f _%d_ stuck into send buff %d -> %d", 
			  Scheduler::instance().clock(), 
			  mn_->address(), 
			  iph->saddr(),
			  iph->daddr());
  
	for (int c=0; c < SEND_BUF_SIZE; c++)
		if (send_buf[c].p == NULL) {
			send_buf[c].t = Scheduler::instance().clock();
			send_buf[c].p = p;
			return;
		}else if (send_buf[c].t < min) {
			min = send_buf[c].t;
			min_index = c;
		}
  
	// kill somebody
	if (verbose_) 
		trace((char*)"SB %.5f _%d_ dropped %d -> %d", 
			  Scheduler::instance().clock(), 
			  mn_->address(), 
			  iph->saddr(), 
			  iph->daddr());
	dropSendBuff(send_buf[min_index].p,DROP_SB_FULL);
	assert(send_buf[min_index].p == NULL);
	send_buf[min_index].t = Scheduler::instance().clock();
	send_buf[min_index].p = p;
}

void
GOAFR_Agent::dropSendBuff(Packet *&p, const char* reason)
{
    struct hdr_ip *iph = HDR_IP(p);

    if (verbose_) 
		trace((char*)"SB %.5f _%d_ dropped %d -> %d for %s", 
			  Scheduler::instance().clock(), 
			  mn_->address(),  
			  iph->saddr(), 
			  iph->daddr(),
			  reason);
    
    drop(p,reason);
    p = NULL;
}

void
GOAFR_Agent::sendBufferCheck()
{

    for (int c=0; c <SEND_BUF_SIZE; c++) {
		if (send_buf[c].p == NULL) continue;
		double elapsed = Scheduler::instance().clock() - send_buf[c].t;
		if (elapsed > SEND_TIMEOUT) {	
			dropSendBuff(send_buf[c].p,DROP_SB_TOUT);
			send_buf[c].p = NULL;
			continue;
		}
	
		// Retry Sending
		if (locservice_->poslookup(send_buf[c].p)){
			forwardPacket(send_buf[c].p);
			send_buf[c].p = NULL;
		}
    }
}

void
GOAFRSendBufferTimer::expire(Event *e)
{
	a_->sendBufferCheck();
	resched(a_->sendbuf_interval());
}

/*********************************************/
/* One more try to get a clean size handling */
/*********************************************/

int
GOAFR_Agent::hdr_size(Packet* p)
{
    struct hdr_cmn *cmnh = HDR_CMN(p);
    struct hdr_goafr *goafrh = HDR_GOAFR(p);

    unsigned int size = 0;

    // Defining Base Field Types in Bytes
    const unsigned int packetType = 1;
    const unsigned int id         = 4;
    const unsigned int locCoord   = 3;
    const unsigned int congestion = 1;
    const unsigned int position   = locCoord + locCoord;
    unsigned int imp_beacon =  id + position;

    if (use_congestion_control_)
		imp_beacon += congestion;
    
    if (!use_implicit_beacon_)
		imp_beacon = 0;

    if (cmnh->ptype() == PT_GOAFR) { // GOAFR Packet

		switch (goafrh->mode_) {
	    case GOAFRH_PPROBE:
			return (packetType + 2*id + 2*position + position + imp_beacon);
	    case GOAFRH_BEACON:
			return (packetType + imp_beacon); 
	    case GOAFRH_BEACON_REQ:
			return (packetType + imp_beacon); 
	    default:
			printf("Invalid GOAFR Packet wants to know it's size !\n");
			abort();
		}
    }

	// don't add the size for the geo-anycast flag here because at the
	// moment it is just used in the HLS. If it should be used by any 
	// other service, the bit used for it must be considered
    if (cmnh->ptype() == PT_LOCS) { // LOCS Packet
		size = packetType + imp_beacon;
    }
    
    if ((cmnh->ptype() != PT_GOAFR) && (cmnh->ptype() != PT_LOCS)) { // Data Packet

		switch (goafrh->mode_) {
	    case GOAFRH_DATA_GREEDY: 
			size = (packetType + position + imp_beacon);
			break;
	    case GOAFRH_DATA_PERI:
		case GOAFRH_DATA_ADVANCE:
			size = (packetType + position + 2*id + 2*position + position + imp_beacon); // last position for intersecting line
			break;
	    default:
			printf("Invalid DATA Packet wants to know it's size !\n");
			abort();
		}
    }

    size += locservice_->hdr_size(p);
    return size;
}

/*******************/
/* Timer Functions */
/*******************/

void GOAFR_DeadNeighbTimer::expire(Event *) { if (a->isActive()) a->deadneighb_callback(ne); }
void GOAFR_PeriProbeTimer::expire(Event *) { if (a->isActive()) a->periprobe_callback(ne); }
void GOAFR_BeaconTimer::expire(Event *) { if (a->isActive()) a->beacon_callback(); }
void GOAFR_LastPeriTimer::expire(Event *) { if (a->isActive()) a->lastperi_callback(); }
void GOAFR_PlanarTimer::expire(Event *) { if (a->isActive()) a->planar_callback(); }

void GOAFRBeaconReqDelayTimer::expire(Event *) { if (a->isActive()) a->allow(GOAFRH_BEACON_REQ); }
void GOAFRBeaconDelayTimer::expire(Event *) { if (a->isActive()) a->allow(GOAFRH_BEACON); }

