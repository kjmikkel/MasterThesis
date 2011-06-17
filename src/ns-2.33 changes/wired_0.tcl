# author: Thomas Ogilvie
# sample tcl script showing the use of GPSR and HLS (hierarchical location service)


## GREEDY Options
Agent/GREEDY set bdesync_                0.5 ;# beacon desync random component
Agent/GREEDY set bexp_                   [expr 3*([Agent/GREEDY set bint_]+[Agent/GREEDY set bdesync_]*[Agent/GREEDY set bint_])] ;# beacon timeout interval
Agent/GREEDY set pint_                   1.5 ;# peri probe interval
Agent/GREEDY set pdesync_                0.5 ;# peri probe desync random component
Agent/GREEDY set lpexp_                  8.0 ;# peris unused timeout interval
Agent/GREEDY set drop_debug_             1   ;#
Agent/GREEDY set peri_proact_            1 	 ;# proactively generate peri probes
Agent/GREEDY set use_implicit_beacon_    1   ;# all packets act as beacons; promisc.
Agent/GREEDY set use_timed_plnrz_        0   ;# replanarize periodically
Agent/GREEDY set use_congestion_control_ 0
Agent/GREEDY set use_reactive_beacon_    0   ;# only use reactive beaconing

set val(bint)           0.5  ;# beacon interval
set val(use_mac)        1    ;# use link breakage feedback from MAC
set val(use_peri)       1    ;# probe and use perimeters
set val(use_planar)     1    ;# planarize graph
set val(verbose)        1    ;#
set val(use_beacon)     1    ;# use beacons at all
set val(use_reactive)   0    ;# use reactive beaconing
set val(locs)           0    ;# default to OmniLS
set val(use_loop)       0    ;# look for unexpected loops in peris

set val(agg_mac)          1 ;# Aggregate MAC Traces
set val(agg_rtr)          0 ;# Aggregate RTR Traces
set val(agg_trc)          0 ;# Shorten Trace File


set val(chan)		Channel/WirelessChannel
set val(prop)		Propagation/TwoRayGround
set val(netif)		Phy/WirelessPhy
set val(mac)		Mac/802_11
set val(ifq)		Queue/DropTail/PriQueue
set val(ll)		LL
set val(ant)		Antenna/OmniAntenna
set val(x)		2000      ;# X dimension of the topography
set val(y)		2000      ;# Y dimension of the topography
set val(ifqlen)		512       ;# max packet in ifq
set val(seed)		1.0
set val(adhocRouting)	GREEDY      ;# AdHoc Routing Protocol
set val(nn)		10       ;# how many nodes are simulated
set val(stop)		40.0     ;# simulation time
set val(use_gk)		0	  ;# > 0: use GridKeeper with this radius
set val(zip)		0         ;# should trace files be zipped

set val(agttrc)         ON ;# Trace Agent
set val(rtrtrc)         ON ;# Trace Routing Agent
set val(mactrc)         ON ;# Trace MAC Layer
set val(movtrc)         ON ;# Trace Movement


set val(lt)		""
set val(cp)		"cp-n40-a40-t40-c4-m0"
set val(sc)		"sc-x2000-y2000-n40-s25-t40"

set val(out)            "mikkel_test.tr"

Agent/GREEDY set locservice_type_ 3

add-all-packet-headers
remove-all-packet-headers
add-packet-header Common Flags IP LL Mac Message GREEDY  LOCS SR RTP Ping HLS

Agent/GREEDY set bint_                  $val(bint)
# Recalculating bexp_ here
Agent/GREEDY set bexp_                 [expr 3*([Agent/GREEDY set bint_]+[Agent/GREEDY set bdesync_]*[Agent/GREEDY set bint_])] ;# beacon timeout interval
Agent/GREEDY set use_peri_              $val(use_peri)
Agent/GREEDY set use_planar_            $val(use_planar)
Agent/GREEDY set use_mac_               $val(use_mac)
Agent/GREEDY set use_beacon_            $val(use_beacon)
Agent/GREEDY set verbose_               $val(verbose)
Agent/GREEDY set use_reactive_beacon_   $val(use_reactive)
Agent/GREEDY set use_loop_detect_       $val(use_loop)

CMUTrace set aggregate_mac_           $val(agg_mac)
CMUTrace set aggregate_rtr_           $val(agg_rtr)

# seeding RNG
ns-random $val(seed)

# create simulator instance
set ns_		[new Simulator]

$ns_ use-newtrace

# Outputs nam traces
set nf [open out.nam w]
$ns_ namtrace-all $nf

set loadTrace  $val(lt)

set topo	[new Topography]
$topo load_flatgrid $val(x) $val(y)

set tracefd	[open $val(out) w]

$ns_ trace-all $tracefd

set chanl [new $val(chan)]

# Create God
set god_ [create-god $val(nn)]

# Attach Trace to God
set T [new Trace/Generic]
$T attach $tracefd
$T set src_ -5
$god_ tracetarget $T

#
# Define Nodes
#
puts "Configuring Nodes ($val(nn))"
$ns_ node-config -adhocRouting $val(adhocRouting) \
                 -llType $val(ll) \
                 -macType $val(mac) \
                 -ifqType $val(ifq) \
                 -ifqLen $val(ifqlen) \
                 -antType $val(ant) \
                 -propType $val(prop) \
                 -phyType $val(netif) \
                 -channel $chanl \
		 -topoInstance $topo \
                 -wiredRouting OFF \
		 -mobileIP OFF \
		 -agentTrace $val(agttrc) \
                 -routerTrace $val(rtrtrc) \
                 -macTrace $val(mactrc) \
                 -movementTrace $val(movtrc)

#
#  Create the specified number of nodes [$val(nn)] and "attach" them
#  to the channel. 
for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node]
    $node_($i) random-motion 0		;# disable random motion
	set ragent [$node_($i) set ragent_]
	$ragent install-tap [$node_($i) set mac_(0)]

    if { $val(mac) == "Mac/802_11" } {      
	# bind MAC load trace file
	[$node_($i) set mac_(0)] load-trace $loadTrace
    }

    # Bring Nodes to God's Attention
    $god_ new_node $node_($i)
}

source $val(sc)

source $val(cp)

#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $val(stop).0 "$node_($i) reset";
}

$ns_ at  $val(stop).0002 "puts \"NS EXITING... $val(out)\" ; $ns_ halt"

# A finish proc to flush traces and out call nam
proc finish {} {
        global ns nf
        $ns flush-trace
        close $nf

        puts "running nam..."
        exec nam out.nam &
        exit 0
}

puts "Starting Simulation..."
$ns_ run
