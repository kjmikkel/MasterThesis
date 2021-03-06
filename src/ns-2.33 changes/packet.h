/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) $Header: /cvsroot/nsnam/ns-2/common/packet.h,v 1.102 2008/02/18 03:39:02 tom_henderson Exp $ (LBL)
 */

#ifndef ns_packet_h
#define ns_packet_h

#include <string.h>
#include <assert.h>

#include "config.h"
#include "scheduler.h"
#include "object.h"
#include "lib/bsd-list.h"
#include "packet-stamp.h"
#include "ns-process.h"
#include "ellipsis.h"

// Used by wireless routing code to attach routing agent
#define RT_PORT		255	/* port that all route msgs are sent to */

#define HDR_CMN(p)      (hdr_cmn::access(p))
#define HDR_ARP(p)      (hdr_arp::access(p))
#define HDR_MAC(p)      (hdr_mac::access(p))
#define HDR_MAC802_11(p) ((hdr_mac802_11 *)hdr_mac::access(p))
#define HDR_MAC_TDMA(p) ((hdr_mac_tdma *)hdr_mac::access(p))
#define HDR_SMAC(p)     ((hdr_smac *)hdr_mac::access(p))
#define HDR_LL(p)       (hdr_ll::access(p))
#define HDR_HDLC(p)     ((hdr_hdlc *)hdr_ll::access(p))
#define HDR_IP(p)       (hdr_ip::access(p))
#define HDR_RTP(p)      (hdr_rtp::access(p))
#define HDR_TCP(p)      (hdr_tcp::access(p))
#define HDR_SCTP(p)     (hdr_sctp::access(p))
#define HDR_SR(p)       (hdr_sr::access(p))
#define HDR_TFRC(p)     (hdr_tfrc::access(p))
#define HDR_TORA(p)     (hdr_tora::access(p))
#define HDR_IMEP(p)     (hdr_imep::access(p))
#define HDR_CDIFF(p)    (hdr_cdiff::access(p))  /* chalermak's diffusion*/
//#define HDR_DIFF(p)     (hdr_diff::access(p))  /* SCADD's diffusion ported into ns */
#define HDR_LMS(p)		(hdr_lms::access(p))

// ->
#define HDR_LOCS(p)	(hdr_locs::access(p))

// GPSR
#define HDR_GPSR(p)     (hdr_gpsr::access(p))
// GREEDY
#define HDR_GREEDY(p)   (hdr_greedy::access(p))
// GOAFR
#define HDR_GOAFR(p) (hdr_goafr::access(p))
// inserted - to

/* --------------------------------------------------------------------*/

/*
 * modified ns-2.33, adding support for dynamic libraries
 * 
 * packet_t is changed from enum to unsigned int in order to allow
 * dynamic definition of  new packet types within dynamic libraries.
 * Pre-defined packet types are implemented as static const.
 * 
 */

typedef unsigned int packet_t;

static const packet_t PT_TCP = 0;
static const packet_t PT_UDP = 1;
static const packet_t PT_CBR = 2;
static const packet_t PT_AUDIO = 3;
static const packet_t PT_VIDEO = 4;
static const packet_t PT_ACK = 5;
static const packet_t PT_START = 6;
static const packet_t PT_STOP = 7;
static const packet_t PT_PRUNE = 8;
static const packet_t PT_GRAFT = 9;
static const packet_t PT_GRAFTACK = 10;
static const packet_t PT_JOIN = 11;
static const packet_t PT_ASSERT = 12;
static const packet_t PT_MESSAGE = 13;
static const packet_t PT_RTCP = 14;
static const packet_t PT_RTP = 15;
static const packet_t PT_RTPROTO_DV = 16;
static const packet_t PT_CtrMcast_Encap = 17;
static const packet_t PT_CtrMcast_Decap = 18;
static const packet_t PT_SRM = 19;
        /* simple signalling messages */
static const packet_t PT_REQUEST = 20;
static const packet_t PT_ACCEPT = 21;
static const packet_t PT_CONFIRM = 22;
static const packet_t PT_TEARDOWN = 23;
static const packet_t PT_LIVE = 24;   // packet from live network
static const packet_t PT_REJECT = 25;

static const packet_t PT_TELNET = 26; // not needed: telnet use TCP
static const packet_t PT_FTP = 27;
static const packet_t PT_PARETO = 28;
static const packet_t PT_EXP = 29;
static const packet_t PT_INVAL = 30;
static const packet_t PT_HTTP = 31;

        /* new encapsulator */
static const packet_t PT_ENCAPSULATED = 32;
static const packet_t PT_MFTP = 33;

        /* CMU/Monarch's extnsions */
static const packet_t PT_ARP = 34;
static const packet_t PT_MAC = 35;
static const packet_t PT_TORA = 36;
static const packet_t PT_DSR = 37;
static const packet_t PT_AODV = 38;
static const packet_t PT_IMEP = 39;
        
        // RAP packets
static const packet_t PT_RAP_DATA = 40;
static const packet_t PT_RAP_ACK = 41;
  
static const packet_t PT_TFRC = 42;
static const packet_t PT_TFRC_ACK = 43;
static const packet_t PT_PING = 44;
        
static const packet_t PT_PBC = 45;
        // Diffusion packets - Chalermek
static const packet_t PT_DIFF = 46;
        
        // LinkState routing update packets
static const packet_t PT_RTPROTO_LS = 47;
        
        // MPLS LDP header
static const packet_t PT_LDP = 48;
        
        // GAF packet
static const packet_t PT_GAF = 49;
        
        // ReadAudio traffic
static const packet_t PT_REALAUDIO = 50;
        
        // Pushback Messages
static const packet_t PT_PUSHBACK = 51;
  
  #ifdef HAVE_STL
        // Pragmatic General Multicast
static const packet_t PT_PGM = 52;
  #endif //STL
        // LMS packets
static const packet_t PT_LMS = 53;
static const packet_t PT_LMS_SETUP = 54;

static const packet_t PT_SCTP = 55;
static const packet_t PT_SCTP_APP1 = 56;

        // SMAC packet
static const packet_t PT_SMAC = 57;
        // XCP packet
static const packet_t PT_XCP = 58;

        // HDLC packet
static const packet_t PT_HDLC = 59;

        // Bell Labs Traffic Trace Type (PackMime OL)
static const packet_t PT_BLTRACE = 60;

        // insert new packet types here
// ->
static const packet_t PT_LOCS = 61;
static const packet_t PT_GPSR = 62; // GPSR
static const packet_t PT_HLS = 63; // HLS - wk
static const packet_t PT_GREEDY = 64; // GREEDY
static const packet_t PT_GOAFR = 65; // GOAFR
static packet_t       PT_NTYPE = 66; // This MUST be the LAST one
// insterted - to

enum packetClass
{
	UNCLASSIFIED,
	ROUTING,
	DATApkt
  };


/*
 * ns-2.33 adding support for dynamic libraries
 * 
 * The PacketClassifier class is needed to make
 * p_info::data_packet(packet_t) work also with dynamically defined
 * packet types.
 * 
 */
class PacketClassifier
{
	public:
		PacketClassifier(): next_(0){}
		virtual ~PacketClassifier() {}
		void setNext(PacketClassifier *next){next_ = next;}
		PacketClassifier *getNext(){return next_;}
		packetClass classify(packet_t type) 
		{
		        packetClass c = getClass(type);
		        if(c == UNCLASSIFIED && next_)
		                c = next_->classify(type);
		        return c;
		}

	protected:
		//return 0 if the packet is unknown
		virtual packetClass getClass(packet_t type) = 0;        
		PacketClassifier *next_;
};

class p_info {
public:
	p_info()
	{
		initName();
	}
	const char* name(packet_t p) const { 
		if ( p <= p_info::nPkt_ ) return name_[p];
		return 0;
	}
	static bool data_packet(packet_t type) {
		return ( (type) == PT_TCP || \
		         (type) == PT_TELNET || \
		         (type) == PT_CBR || \
		         (type) == PT_AUDIO || \
		         (type) == PT_VIDEO || \
		         (type) == PT_ACK || \
		         (type) == PT_SCTP || \
		         (type) == PT_SCTP_APP1 || \
		         (type) == PT_HDLC \
		        );
	}
	static packetClass classify(packet_t type) {		
		if (type == PT_DSR || 
		    type == PT_MESSAGE || 
		    type == PT_TORA || 
		    type == PT_AODV ||
                    type == PT_GPSR ||
                    type == PT_GREEDY ||
                    type == PT_GOAFR)
			return ROUTING;		
		if (type == PT_TCP || 
		    type == PT_TELNET || 
		    type == PT_CBR || 
		    type == PT_AUDIO || 
		    type == PT_VIDEO || 
		    type == PT_ACK || 
		    type == PT_SCTP || 
		    type == PT_SCTP_APP1 || 
		    type == PT_HDLC)
			return DATApkt;
		if (pc_)
			return pc_->classify(type);
		return UNCLASSIFIED;
	}
	static void addPacketClassifier(PacketClassifier *pc)
	{
		if(!pc)
		        return;
		pc->setNext(pc_);
		pc_ = pc;
	}       
	static void initName()
	{
		if(nPkt_ >= PT_NTYPE+1)
		        return;
		char **nameNew = new char*[PT_NTYPE+1];
		for(unsigned int i = (unsigned int)PT_SMAC+1; i < nPkt_; i++)
		{
		        nameNew[i] = name_[i];
		}
		if(!nPkt_)
		        delete [] name_;
		name_ = nameNew;
		nPkt_ = PT_NTYPE+1;
		

		name_[PT_TCP]= (char*)"tcp";
		name_[PT_UDP]= (char*)"udp";
		name_[PT_CBR]= (char*)"cbr";
		name_[PT_AUDIO]= (char*)"audio";
		name_[PT_VIDEO]= (char*)"video";
		name_[PT_ACK]= (char*)"ack";
		name_[PT_START]= (char*)"start";
		name_[PT_STOP]= (char*)"stop";
		name_[PT_PRUNE]= (char*)"prune";
		name_[PT_GRAFT]= (char*)"graft";
		name_[PT_GRAFTACK]= (char*)"graftAck";
		name_[PT_JOIN]= (char*)"join";
		name_[PT_ASSERT]= (char*)"assert";
		name_[PT_MESSAGE]= (char*)"message";
		name_[PT_RTCP]= (char*)"rtcp";
		name_[PT_RTP]= (char*)"rtp";
		name_[PT_RTPROTO_DV]= (char*)"rtProtoDV";
		name_[PT_CtrMcast_Encap]= (char*)"CtrMcast_Encap";
		name_[PT_CtrMcast_Decap]= (char*)"CtrMcast_Decap";
		name_[PT_SRM]= (char*)"SRM";
	
		name_[PT_REQUEST]= (char*)"sa_req";	
		name_[PT_ACCEPT]= (char*)"sa_accept";
		name_[PT_CONFIRM]= (char*)"sa_conf";
		name_[PT_TEARDOWN]= (char*)"sa_teardown";
		name_[PT_LIVE]= (char*)"live"; 
		name_[PT_REJECT]= (char*)"sa_reject";
	
		name_[PT_TELNET]= (char*)"telnet";
		name_[PT_FTP]= (char*)"ftp";
		name_[PT_PARETO]= (char*)"pareto";
		name_[PT_EXP]= (char*)"exp";
		name_[PT_INVAL]= (char*)"httpInval";
		name_[PT_HTTP]= (char*)"http";
		name_[PT_ENCAPSULATED]= (char*)"encap";
		name_[PT_MFTP]= (char*)"mftp";
		name_[PT_ARP]= (char*)"ARP";
		name_[PT_MAC]= (char*)"MAC";
		name_[PT_TORA]= (char*)"TORA";
		name_[PT_DSR]= (char*)"DSR";
		name_[PT_AODV]= (char*)"AODV";
		name_[PT_IMEP]= (char*)"IMEP";

		name_[PT_RAP_DATA] = (char*)"rap_data";
		name_[PT_RAP_ACK] = (char*)"rap_ack";

 		name_[PT_TFRC]= (char*)"tcpFriend";
		name_[PT_TFRC_ACK]= (char*)"tcpFriendCtl";
		name_[PT_PING]=(char*)"ping";
	
		name_[PT_PBC] = (char*)"PBC";

	 	/* For diffusion : Chalermek */
 		name_[PT_DIFF] = (char*)"diffusion";

		// Link state routing updates
		name_[PT_RTPROTO_LS] = (char*)"rtProtoLS";

		// MPLS LDP packets
		name_[PT_LDP] = (char*)"LDP";

		// for GAF
                name_[PT_GAF] = (char*)"gaf";      

		// RealAudio packets
		name_[PT_REALAUDIO] = (char*)"ra";

		//pushback 
		name_[PT_PUSHBACK] = (char*)"pushback";

#ifdef HAVE_STL
		// for PGM
		name_[PT_PGM] = (char*)"PGM";
#endif //STL

		// LMS entries
		name_[PT_LMS]=(char*)"LMS";
		name_[PT_LMS_SETUP]=(char*)"LMS_SETUP";

		name_[PT_SCTP]= (char*)"sctp";
 		name_[PT_SCTP_APP1] = (char*)"sctp_app1";
		
		// smac
		name_[PT_SMAC]=(char*)"smac";

		// HDLC
		name_[PT_HDLC]=(char*)"HDLC";

		// XCP
		name_[PT_XCP]=(char*)"xcp";

		// Bell Labs (PackMime OL)
		name_[PT_BLTRACE]=(char*)"BellLabsTrace";

// ->
		name_[PT_LOCS]=(char*)"LOCS";

		// HLS - wk
		name_[PT_HLS]= (char*)"HLS";

		// GPSR
		name_[PT_GPSR]= (char*)"GPSR";

		// GREEDY
		name_[PT_GREEDY]= (char*)"GREEDY";

		// GOAFR
		name_[PT_GOAFR]= (char*)"GOAFR";
// insterted - to
		
		name_[PT_NTYPE]= (char*)"undefined";
	}
	static int addPacket(char *name);
	static packet_t getType(const char *name)
	{
		for(unsigned int i = 0; i < nPkt_; i++)
		{
		        if(strcmp(name, name_[i]) == 0)
		                return i;
		}
		return PT_NTYPE;

	}
private:
	static char** name_;
	static unsigned int nPkt_;
	static PacketClassifier *pc_;
};
extern p_info packet_info; /* map PT_* to string name */
//extern char* p_info::name_[];

#define DATA_PACKET(type) ( (type) == PT_TCP || \
                            (type) == PT_TELNET || \
                            (type) == PT_CBR || \
                            (type) == PT_AUDIO || \
                            (type) == PT_VIDEO || \
                            (type) == PT_ACK || \
                            (type) == PT_SCTP || \
                            (type) == PT_SCTP_APP1 \
                            )

//#define OFFSET(type, field)	((long) &((type *)0)->field)
#define OFFSET(type, field) ( (char *)&( ((type *)256)->field )  - (char *)256)

class PacketData : public AppData {
public:
	PacketData(int sz) : AppData(PACKET_DATA) {
		datalen_ = sz;
		if (datalen_ > 0)
			data_ = new unsigned char[datalen_];
		else
			data_ = NULL;
	}
	PacketData(PacketData& d) : AppData(d) {
		datalen_ = d.datalen_;
		if (datalen_ > 0) {
			data_ = new unsigned char[datalen_];
			memcpy(data_, d.data_, datalen_);
		} else
			data_ = NULL;
	}
	virtual ~PacketData() { 
		if (data_ != NULL) 
			delete []data_; 
	}
	unsigned char* data() { return data_; }

	virtual int size() const { return datalen_; }
	virtual AppData* copy() { return new PacketData(*this); }
private:
	unsigned char* data_;
	int datalen_;
};

//Monarch ext
typedef void (*FailureCallback)(Packet *,void *);

class Packet : public Event {
private:
	unsigned char* bits_;	// header bits
//	unsigned char* data_;	// variable size buffer for 'data'
//  	unsigned int datalen_;	// length of variable size buffer
	AppData* data_;		// variable size buffer for 'data'
	static void init(Packet*);     // initialize pkt hdr 
	bool fflag_;
	// Added to support GOAFR
	bool reversed_direction;  // Not to be confused with counter clockwise
	// End of added to support for GOAFR
protected:
	static Packet* free_;	// packet free list
	int	ref_count_;	// free the pkt until count to 0
public:
	Packet* next_;		// for queues and the free list
	static int hdrlen_;

	// Support added to GOAFR
        Ellipsis* ellipse_; 	// The ellipsis that the packet has to keep inside
	bool greedy_start;
	// End support added to GOAFR

	Packet() : bits_(0), data_(0), ref_count_(0), next_(0), ellipse_(0), reversed_direction(0), greedy_start(0) { }
	inline unsigned char* const bits() { return (bits_); }
	inline Packet* copy() const;
	inline Packet* refcopy() { ++ref_count_; return this; }
	inline int& ref_count() { return (ref_count_); }
	static inline Packet* alloc();
	static inline Packet* alloc(int);
	inline void allocdata(int);
	// dirty hack for diffusion data
	inline void initdata() { data_  = 0;}
	static inline void free(Packet*);
	inline unsigned char* access(int off) const {
		if (off < 0)
			abort();
		return (&bits_[off]);
	}
	// This is used for backward compatibility, i.e., assuming user data
	// is PacketData and return its pointer.
	inline unsigned char* accessdata() const { 
		if (data_ == 0)
			return 0;
		assert(data_->type() == PACKET_DATA);
		return (((PacketData*)data_)->data()); 
	}
	// This is used to access application-specific data, not limited 
	// to PacketData.
	inline AppData* userdata() const {
		return data_;
	}
	inline void setdata(AppData* d) { 
		if (data_ != NULL)
			delete data_;
		data_ = d; 
	}
	inline int datalen() const { return data_ ? data_->size() : 0; }

	inline void reverse_direction() {
		reversed_direction = !reversed_direction;
	}

	inline bool reversed() {
		return reversed_direction;
	}

	// Monarch extn

	static void dump_header(Packet *p, int offset, int length);

	// the pkt stamp carries all info about how/where the pkt
        // was sent needed for a receiver to determine if it correctly
        // receives the pkt
        PacketStamp	txinfo_;  

	/*
         * According to cmu code:
	 * This flag is set by the MAC layer on an incoming packet
         * and is cleared by the link layer.  It is an ugly hack, but
         * there's really no other way because NS always calls
         * the recv() function of an object.
	 * 
         */
        u_int8_t        incoming;

	//monarch extns end;
};

/* 
 * static constant associations between interface special (negative) 
 * values and their c-string representations that are used from tcl
 */
class iface_literal {
public:
	enum iface_constant { 
		UNKN_IFACE= -1, /* 
				 * iface value fo½r locally originated packets 
				 */
		ANY_IFACE= -2   /* 
				 * hashnode with iif == ANY_IFACE_   
				 * matches any pkt iface (imported from TCL);
				 * this value should be different from 
				 * hdr_cmn::UNKN_IFACE (packet.h)
				 */ 
	};
	iface_literal(const iface_constant i, const char * const n) : 
		value_(i), name_(n) {}
	inline int value() const { return value_; }
	inline const char * const name() const { return name_; }
private:
	const iface_constant value_;
	/* strings used in TCL to access those special values */
	const char * const name_; 
};

static const iface_literal UNKN_IFACE(iface_literal::UNKN_IFACE, "?");
static const iface_literal ANY_IFACE(iface_literal::ANY_IFACE, "*");

/*
 * Note that NS_AF_* doesn't necessarily correspond with
 * the constants used in your system (because many
 * systems don't have NONE or ILINK).
 */
enum ns_af_enum { NS_AF_NONE, NS_AF_ILINK, NS_AF_INET };

enum ModulationScheme {BPSK = 0, QPSK = 1, QAM16 = 2, QAM64 = 3};

struct hdr_cmn {
	enum dir_t { DOWN= -1, NONE= 0, UP= 1 };
	packet_t ptype_;	// packet type (see above)
	int	size_;		// simulated packet size
	int	uid_;		// unique id
	int	error_;		// error flag
	int     errbitcnt_;     // # of corrupted bits jahn
	int     fecsize_;
	double	ts_;		// timestamp: for q-delay measurement
	int	iface_;		// receiving interface (label)
	dir_t	direction_;	// direction: 0=none, 1=up, -1=down
	// source routing 
        char src_rt_valid;
	double ts_arr_; // Required by Marker of JOBS 

	//Monarch extn begins
	nsaddr_t prev_hop_;     // IP addr of forwarding hop
	nsaddr_t next_hop_;	// next hop for this packet
	int      addr_type_;    // type of next_hop_ addr
	nsaddr_t last_hop_;     // for tracing on multi-user channels

        // called if pkt can't obtain media or isn't ack'd. not called if
        // droped by a queue
        FailureCallback xmit_failure_; 
        void *xmit_failure_data_;

        /*
         * MONARCH wants to know if the MAC layer is passing this back because
         * it could not get the RTS through or because it did not receive
         * an ACK.
         */
        int     xmit_reason_;
#define XMIT_REASON_RTS 0x01
#define XMIT_REASON_ACK 0x02

        // filled in by GOD on first transmission, used for trace analysis
        int num_forwards_;	// how many times this pkt was forwarded
        int opt_num_forwards_;   // optimal #forwards
	// Monarch extn ends;

	// tx time for this packet in sec
	double txtime_;
	inline double& txtime() { return(txtime_); }

	static int offset_;	// offset for this header
	inline static int& offset() { return offset_; }
	inline static hdr_cmn* access(const Packet* p) {
		return (hdr_cmn*) p->access(offset_);
	}
	
        /* per-field member functions */
	inline packet_t& ptype() { return (ptype_); }
	inline int& size() { return (size_); }
	inline int& uid() { return (uid_); }
	inline int& error() { return error_; }
	inline int& errbitcnt() {return errbitcnt_; }
	inline int& fecsize() {return fecsize_; }
	inline double& timestamp() { return (ts_); }
	inline int& iface() { return (iface_); }
	inline dir_t& direction() { return (direction_); }
	// monarch_begin
	inline nsaddr_t& next_hop() { return (next_hop_); }
	inline int& addr_type() { return (addr_type_); }
	inline int& num_forwards() { return (num_forwards_); }
	inline int& opt_num_forwards() { return (opt_num_forwards_); }
        //monarch_end

	ModulationScheme mod_scheme_;
	inline ModulationScheme& mod_scheme() { return (mod_scheme_); }
};


class PacketHeaderClass : public TclClass {
protected:
	PacketHeaderClass(const char* classname, int hdrsize);
	virtual int method(int argc, const char*const* argv);
	void field_offset(const char* fieldname, int offset);
	inline void bind_offset(int* off) { offset_ = off; }
	inline void offset(int* off) {offset_= off;}
	int hdrlen_;		// # of bytes for this header
	int* offset_;		// offset for this header
public:
	virtual void bind();
	virtual void export_offsets();
	TclObject* create(int argc, const char*const* argv);
};


inline void Packet::init(Packet* p)
{
	bzero(p->bits_, hdrlen_);
}

inline Packet* Packet::alloc()
{
	Packet* p = free_;
	if (p != 0) {
		assert(p->fflag_ == FALSE);
		free_ = p->next_;
		assert(p->data_ == 0);
		p->uid_ = 0;
		p->time_ = 0;
	} else {
		p = new Packet;
		p->ellipse_ = NULL;
		p->bits_ = new unsigned char[hdrlen_];
		if (p == 0 || p->bits_ == 0)
			abort();
	}
	init(p); // Initialize bits_[]
	(HDR_CMN(p))->next_hop_ = -2; // -1 reserved for IP_BROADCAST
	(HDR_CMN(p))->last_hop_ = -2; // -1 reserved for IP_BROADCAST
	p->fflag_ = TRUE;
	(HDR_CMN(p))->direction() = hdr_cmn::DOWN;
	/* setting all direction of pkts to be downward as default; 
	   until channel changes it to +1 (upward) */
	p->next_ = 0;
	return (p);
}

/* 
 * Allocate an n byte data buffer to an existing packet 
 * 
 * To set application-specific AppData, use Packet::setdata()
 */
inline void Packet::allocdata(int n)
{
	assert(data_ == 0);
	data_ = new PacketData(n);
	if (data_ == 0)
		abort();
}

/* allocate a packet with an n byte data buffer */
inline Packet* Packet::alloc(int n)
{
	Packet* p = alloc();
	if (n > 0) 
		p->allocdata(n);
	return (p);
}


inline void Packet::free(Packet* p)
{
	if (p->fflag_) {
		if (p->ref_count_ == 0) {
			/*
			 * A packet's uid may be < 0 (out of a event queue), or
			 * == 0 (newed but never gets into the event queue.
			 */
			assert(p->uid_ <= 0);
			// Delete user data because we won't need it any more.
			if (p->data_ != 0) {
				delete p->data_;
				p->data_ = 0;
			}

			// Added to support GOAFR - delete the ellipse
			if (p->ellipse_ != NULL) {
				delete p->ellipse_;
				p->ellipse_ = NULL;
			}
			
			init(p);
			p->next_ = free_;
			free_ = p;
			p->fflag_ = FALSE;
		} else {
			--p->ref_count_;
		}
	}
}

inline Packet* Packet::copy() const
{
	
	Packet* p = alloc();
	memcpy(p->bits(), bits_, hdrlen_);
	if (data_) 
		p->data_ = data_->copy();
	p->txinfo_.init(&txinfo_);
        
        // Added to support GOAFR
        if (ellipse_ != NULL) {
		p->ellipse_ = ellipse_->copy();
	}
	// End added to support GOAFR
 
	return (p);
}

inline void
Packet::dump_header(Packet *p, int offset, int length)
{
        assert(offset + length <= p->hdrlen_);
        struct hdr_cmn *ch = HDR_CMN(p);

        fprintf(stderr, "\nPacket ID: %d\n", ch->uid());

        for(int i = 0; i < length ; i+=16) {
                fprintf(stderr, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                        p->bits_[offset + i],     p->bits_[offset + i + 1],
                        p->bits_[offset + i + 2], p->bits_[offset + i + 3],
                        p->bits_[offset + i + 4], p->bits_[offset + i + 5],
                        p->bits_[offset + i + 6], p->bits_[offset + i + 7],
                        p->bits_[offset + i + 8], p->bits_[offset + i + 9],
                        p->bits_[offset + i + 10], p->bits_[offset + i + 11],
                        p->bits_[offset + i + 12], p->bits_[offset + i + 13],
                        p->bits_[offset + i + 14], p->bits_[offset + i + 15]);
        }
}

#endif
