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

#ifndef _LocationService_h
#define _LocationService_h

#include <ip.h>
#include <god.h>
#include <trace.h>
#include <mobilenode.h>
#include <agent.h>

class LocationService {

 public:
    LocationService(class Agent *p) {
      parent = p;
      active_ = true;
      tracetarget_ = NULL;
      mn_ = NULL;
    }
    virtual ~LocationService() {}
    virtual void recv(Packet* &p) = 0;
    virtual bool poslookup(Packet *p) = 0;
    virtual void evaluatePacket(const Packet *p) { return; }
    virtual void setTarget(NsObject *t) {
      target_ = t;
    }
    virtual void init() { return; }
    virtual void wake() { active_ = true; }
    virtual void sleep() { active_ = false; }
    virtual void callback(Packet* &p) { return; }
    virtual void dropPacketCallback(Packet* &p) { return; }
    virtual int hdr_size(Packet* p) { return(0); }
    inline bool active() { return active_; }

    inline void setTraceTarget(Trace* target) { tracetarget_ = target; }
    inline void setMobileNode(MobileNode* mn) { mn_ = mn; }
    inline nsaddr_t addr() { 
	//if (parent == NULL) {abort();}
	return parent->addr(); 
    }

    virtual void trace(char *fmt,...) {
	va_list ap;
	if (!tracetarget_)
	    return;
	va_start(ap, fmt);
	vsprintf(tracetarget_->pt_->buffer(), fmt, ap);
	tracetarget_->pt_->dump();
	va_end(ap);
    }
    
 protected:
    class Agent *parent;
    NsObject *target_;
    Trace *tracetarget_;
    MobileNode *mn_;
    bool active_;

    // All LS based routing agents must be listed here - mk
    //friend class GPSR_Agent;
    friend class PBRAgent;
    friend class CBF_Agent;
};
#endif // _LocationService_h

