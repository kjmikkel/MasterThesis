# Copyrighi (c) 1999 Regents of the University of Southern California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by the Computer Systems
#      Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# wireless1.tcl
# $Id: run.tcl,v 1.50 2002/11/28 16:54:38 mikael Exp $

set PI 3.1415926535897

# ======================================================================
# Define Protocol Defaults
# ======================================================================

## GPSR Options
Agent/GPSR set bdesync_                0.5 ;# beacon desync random component
Agent/GPSR set bexp_                   [expr 3*([Agent/GPSR set bint_]+[Agent/GPSR set bdesync_]*[Agent/GPSR set bint_])] ;# beacon timeout interval
Agent/GPSR set pint_                   1.5 ;# peri probe interval
Agent/GPSR set pdesync_                0.5 ;# peri probe desync random component
Agent/GPSR set lpexp_                  8.0 ;# peris unused timeout interval
Agent/GPSR set drop_debug_             1   ;#
Agent/GPSR set peri_proact_            1 	 ;# proactively generate peri probes
Agent/GPSR set use_implicit_beacon_    1   ;# all packets act as beacons; promisc.
Agent/GPSR set use_timed_plnrz_        0   ;# replanarize periodically
Agent/GPSR set use_congestion_control_ 0
Agent/GPSR set use_reactive_beacon_    0   ;# only use reactive beaconing

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

# ======================================================================
# Define NS Object Defaults
# ======================================================================

# In case normal MovementTrace Option is a no-go
Node/MobileNode set movtrace_   1

# Should ARP Lookup be used in LL
LL set useARP_                  0

# Routing Protocol Prefered (might break Protocol)
Queue/DropTail/PriQueue set Prefer_Routing_Protocols  0

# ======================================================================
# Define Options
# ======================================================================

set val(chan)		Channel/WirelessChannel
set val(prop)		Propagation/TwoRayGround
set val(netif)		Phy/WirelessPhy
set val(mac)		Mac/802_11
set val(ifq)		Queue/DropTail/PriQueue
set val(ll)		LL
set val(ant)		Antenna/OmniAntenna
set val(x)		1000      ;# X dimension of the topography
set val(y)		1000      ;# Y dimension of the topography
set val(ifqlen)		512       ;# max packet in ifq
set val(seed)		1.0
set val(adhocRouting)	GPSR      ;# AdHoc Routing Protocol
set val(nn)		15       ;# how many nodes are simulated
set val(stop)		40.0     ;# simulation time
set val(use_gk)		0	  ;# > 0: use GridKeeper with this radius
set val(zip)		0         ;# should trace files be zipped
set val(bw)		""
set val(bs)		""
set val(rr)		""

set path                ./
set val(cp)             ""
set val(sc)             ""
set val(out)            ""
set val(on_off)		""
set val(pingLog)        ""
set val(lt)		"" ;# MAC load trace file

set val(cc)		"" ;# congestion control
set val(smooth_cc)	""

set val(agttrc)         ON ;# Trace Agent
set val(rtrtrc)         ON ;# Trace Routing Agent
set val(mactrc)         ON ;# Trace MAC Layer
set val(movtrc)         ON ;# Trace Movement

set val(mac_trace)      "" ;# dummy

set val(ed)             " "
set val(ve)             " "

# =====================================================================
# User defined Procedures
# ======================================================================
proc usage {} {
    global argv0
    puts "\nUsage: ns $argv0 -out tracefile\n"
    puts "    NS Options:"
    puts "     -nn           \[number of nodes\]"
    puts "     -stop         \[simulation duration in secs\]"
    puts "     -x / -y       \[dimension in meters\]"
    puts "     -adhocRouting \[routing protocol to use\]"
    puts "     -use_gk       \[radius for gridkeeper usage\]"
    puts "     -zip          \[(0/1) should tracefiles be zipped on-the-fly\]"
    puts "     -cc           \[alpha for congestion control ((MAC802_11 only)\]"
    puts "     -ifqlen       \[max packets in interface queue\]"
    puts ""
    puts "    File Options:"
    puts "     -cp      \[traffic pattern\]"
    puts "     -sc      \[scenario file\]"
    puts "     -on_off  \[wake/sleep pattern\]"
    puts "     -lt      \[load trace file (MAC802_11 only)\]"
    puts "     -pingLog \[log file for ping statistics (Ping Traffic only)\]"
    puts ""
    puts "    MAC Options:"
    puts "     -rr           \[radio range in meters\]"
    puts "     -bw           \[link/dataRate bandwidth in bits/sec\]"
    puts "     -bs           \[basicRate bandwidth in bits/sec\]"
    puts ""
    puts "    GPSR Options:"
    puts "     -bint         \[beacon interval (and beacon expiry)\]"
    puts "     -use_planar   \[(0/1) planarize graph\]"
    puts "     -use_peri     \[(0/1) use perimeter mode\]"
    puts "     -use_mac      \[(0/1) use mac callback\]"
    puts "     -verbose      \[(0/1) be verbose\]"
    puts "     -use_beacon   \[(0/1) use beacons at all (disable beacons with 0)\]"
    puts "     -use_reactive \[(0/1) use reactive beaconing\]"
    puts "     -locs         \[locservice to use (0-Omni/1-RLS/2-GLS/3-HLS)\]"
    puts "     -use_loop     \[(0/1) use loop detection\]"
    puts ""
    puts "     -ed           \[topology file (edges)\]"
    puts "     -ve           \[topology file (verteces)\]"
    puts ""
}

proc getopt {argc argv} {
    global val
    lappend optlist cp sc on_off om out pingLog nn stop x y adhocRouting mac_emu rr bw lt use_gk ifqlen ora
    # HGPS
    lappend optlist upd bint cval mgrid tper tqo
    # GPSR
    lappend optlist bint use_planar use_peri use_mac verbose use_beacon cc smooth_cc use_reactive use_loop
    # CBF & LOCS
    lappend optlist locs supt use_rec pkt_ret rev_ord use_la use_lazy use_uctf 
    lappend use_randa use_sdd agg_mac agg_rtr agg_trc
    # GSR
    lappend optlist ed ve
    lappend optlist seed mac_trace zip no_echo

    for {set i 0} {$i < $argc} {incr i} {
	set arg [lindex $argv $i]
	if {[string range $arg 0 0] != "-"} continue
	set name [string range $arg 1 end]
	set val($name) [lindex $argv [expr $i+1]]
    }
    if { $val(out) == "" } {
	usage
	exit
    }
}

proc printparams {} {
    global val  
    puts "\nParameterset:"
    puts "Tracefile: \"$val(out)\""
    puts "Protocol: $val(adhocRouting) nn: $val(nn) stop: $val(stop) x: $val(x) y: $val(y)"
    puts "Radio Range: $val(rr)"

    if { ($val(adhocRouting) == "GPSR") } {
	if { $val(locs) == "0" } {
	    puts "$val(adhocRouting)/OMNI: Omnipotent Location Service selected."
	} elseif { $val(locs) == "1" } {
	    puts "$val(adhocRouting)/RLS: Reactive Location Service selected."
	} elseif { $val(locs) == "2" } {
	    puts "$val(adhocRouting)/GLS: Grid Location Service selected."
	} elseif { $val(locs) == "3" } {
	    puts "$val(adhocRouting)/HLS: Cell Location Service selected."
	} else {
	    puts "$val(adhocRouting)/UKN: Unknown Location Service. Defaulting to Omnipotent Location Service."
	}
    }
    if { $val(cc) != "" } {
	puts "Using congestion control with alpha = $val(cc) ..."
    }
    puts ""
}

proc changeActiveState {nId on} {
    global node_ val
    if {$on == 0} {
	#puts "Turning off node $nId"
	if { ($val(adhocRouting) == "DSR")||($val(adhocRouting) == "GPSR")||($val(adhocRouting) == "AODV") } {
	    set r [$node_($nId) set ragent_]
	    $r sleep
	}
    } else {
	#puts "Turning on node $nId"
	if { ($val(adhocRouting) == "DSR")||($val(adhocRouting) == "GPSR")||($val(adhocRouting) == "AODV") } {
	    set r [$node_($nId) set ragent_]
	    $r wake
	}
    }
}

proc estimEnd {startTime simTime simEndTime} {
    set now         [clock seconds]
    set realGone    [expr $now - $startTime]
    set simToGo     [expr $simEndTime - $simTime]
    set percSimGone [expr ($simTime / $simEndTime) * 100]
    set percSimToGo [expr 100 - $percSimGone]
    if {$percSimGone == 0} {
	set ete 0
	set eteString "unknown"
    } else {
	set ete [expr $startTime + ($realGone / $percSimGone) * 100]
    }
    set eteString   [clock format [expr round($ete)]]
    set sTimeString [clock format $startTime]
    if {$ete != 0} {
	puts "$simTime\tRun: $realGone ETE:\t$eteString"
    } else {
	puts "$simTime\tBeginn: $sTimeString!"
    }
}

proc instEstim {startTime simEndTime step} {
    global ns_
    for {set t 1} {$t < $simEndTime } { set t [expr  $t + $step]} {
	$ns_ at $t "estimEnd $startTime $t $simEndTime"
    }
}

proc create_gridkeeper {} {
    global gkeeper val node_
 
    set gkeeper [new GridKeeper]
 
    puts "Initializing GridKeeper with radius $val(use_gk) ..."
    #initialize the gridkeeper
 
    $gkeeper dimension $val(x) $val(y)
 
    #
    # add mobile node into the gridkeeper, must be added after
    # scenario file
    #

    for {set i 0} {$i < $val(nn) } {incr i} {
        $gkeeper addnode $node_($i)
 
        $node_($i) radius $val(use_gk)
    }
 
}

# =====================================================================
# Main Program
# ======================================================================
getopt $argc $argv

if { $val(adhocRouting) == "GPSR" } {
    Agent/GPSR set locservice_type_ $val(locs)
}

# create trace object for ping
if { $val(pingLog) != "" } {
    set pingLog  [open $val(pingLog) w] 
} else {
    set pingLog  $val(pingLog)
}

# create trace object for MAC load
if { $val(mac) == "Mac/802_11" } {
    if { $val(lt) != "" } {
	set loadTrace  [open $val(lt) w]
        puts $loadTrace "# x=$val(x), y=$val(y), n=$val(nn), stop=$val(stop)"
    } else {
	set loadTrace  $val(lt)
    }
}

# set up MAC load scanning
if { $val(cc) != "" || $val(lt) != "" } {
    Mac/802_11 set scan_int_	0.001	;# scanning interval
    Mac/802_11 set scan_len_	200	;# scan count for each probe
    if { $val(smooth_cc) == "1" } {
	Mac/802_11 set smooth_scan_ 1	;# smooth the scanned values
    }
}

# set up congestion control
if { $val(cc) != "" } {
    Agent/GPSR set use_congestion_control_ 1
    Agent/GPSR set cc_alpha_ $val(cc)
} else {
    Agent/GPSR set cc_alpha_ 0
}

# set up headers as needed to save on memory
add-all-packet-headers
remove-all-packet-headers
add-packet-header Common Flags IP LL Mac Message GPSR  LOCS SR RTP Ping HLS
# PKT Types of special Interest: 
# ARP TCP GPSR LOCS HGPS SR DSDV AODV TORA IMEP Message Ping RTP 
puts "\n !Warning! Don't forget to check header-inclusion "
puts "           (Not needed for GPSR/DSR & CBR/Ping)\n"

# set dynamic options
if { $val(mac_emu) == "1" } {
    set val(mac)        Mac/Emu
    set val(netif)      Phy/EmuPhy
    if { $val(rr) != "" } {
	God set rrange_ $val(rr)
    } else {
	set val(rr) [God set rrange_]
    }
    if { $val(bw) != "" } {
	God set bandwidth_ $val(bw)
    } else {
	set val(bw) [God set bandwidth_]
    }
} else {
    if { $val(bw) != "" } {
	Phy/WirelessPhy set bandwidth_ $val(bw)
	Mac/802_11 set dataRate_ $val(bw)
    }
    if { $val(bs) != "" } {
	Mac/802_11 set basicRate_ $val(bs)
    }
    if { $val(rr) != "" } {
	God set rrange_ $val(rr)
	Mac/802_11 set rrange_ $val(rr)
	if { $val(rr) >= [expr 9 * $PI * [Phy/WirelessPhy set freq_] / 3e8] } {
	    Phy/WirelessPhy set Pt_ [expr [Phy/WirelessPhy set RXThresh_] * $val(rr)*$val(rr)*$val(rr)*$val(rr) / 5.0625]
	} else {
	    Phy/WirelessPhy set Pt_ [expr [Phy/WirelessPhy set RXThresh_] * 16 * $PI*$PI * $val(rr)*$val(rr) * [Phy/WirelessPhy set freq_]*[Phy/WirelessPhy set freq_] / 9e16]
	}
    } else {
	set val(rr) 250			;# (Pt/Pr)^0.25 * 1.5
    }
}

Agent/GPSR set bint_                  $val(bint)
# Recalculating bexp_ here
Agent/GPSR set bexp_                 [expr 3*([Agent/GPSR set bint_]+[Agent/GPSR set bdesync_]*[Agent/GPSR set bint_])] ;# beacon timeout interval
Agent/GPSR set use_peri_              $val(use_peri)
Agent/GPSR set use_planar_            $val(use_planar)
Agent/GPSR set use_mac_               $val(use_mac)
Agent/GPSR set use_beacon_            $val(use_beacon)
Agent/GPSR set verbose_               $val(verbose)
Agent/GPSR set use_reactive_beacon_   $val(use_reactive)
Agent/GPSR set use_loop_detect_       $val(use_loop)

CMUTrace set aggregate_mac_           $val(agg_mac)
CMUTrace set aggregate_rtr_           $val(agg_rtr)
God set shorten_trace_                $val(agg_trc)

if { $val(movtrc) == "OFF" || $val(agg_trc) == 1} {
Node/MobileNode set movtrace_ 0
}

# seeding RNG
ns-random $val(seed)

# set MACTRACE option
if { $val(mac_trace) != "" } {
    set val(mactrc)         $val(mac_trace)
    puts "MAC trace is $val(mactrc)"
}

# create simulator instance
set ns_		[new Simulator]

# setup topography object
set topo	[new Topography]
$topo load_flatgrid $val(x) $val(y)

# create trace object for ns and nam
if { $val(zip) == "1" } {
    set tracefd [open "|gzip -9c > $val(out).gz" w]
} else {
    set tracefd	[open $val(out) w]
}
$ns_ trace-all $tracefd

# create channel
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

    if { $val(adhocRouting) == "GPSR" || $val(adhocRouting) == "GSR" || $val(adhocRouting) == "CBF" || $val(adhocRouting) == "AODV" } {
	set ragent [$node_($i) set ragent_]
	$ragent install-tap [$node_($i) set mac_(0)]
    }

    if { $val(mac) == "Mac/802_11" } {      
	# bind MAC load trace file
	[$node_($i) set mac_(0)] load-trace $loadTrace
    }

    # Bring Nodes to God's Attention
    $god_ new_node $node_($i)
}


# 
# Define node movement model
#
puts "Loading scenario file ($val(sc))..."
if {$val(sc) == ""} {
    puts "  no scenario file specified"
    exit
} else {
    source $val(sc)
}

# 
# Define traffic model
#
puts "Loading connection pattern ($val(cp))..."
if {$val(cp) == ""} {
    puts "  no connection pattern specified"
} else {
    source $val(cp)
}

#
# Define inactive pattern 
#
puts "Loading inactive pattern ($val(on_off))..."
if { $val(on_off) == "" } {
    puts "  no inactive pattern specified"
} else {
    source $val(on_off)
}

#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $val(stop).0 "$node_($i) reset";
}

$ns_ at  $val(stop).0002 "puts \"NS EXITING... $val(out)\" ; $ns_ halt"

# Print Parameterset
printparams

# start GridKeeper
if { $val(use_gk) > 0 } {
    create_gridkeeper
}

set startTime [clock seconds]
puts "Installing Time Estimator ($startTime)!"
instEstim $startTime $val(stop) 2.5

puts $tracefd "M 0.0 nn $val(nn) x $val(x) y $val(y) rp $val(adhocRouting)"
puts $tracefd "M 0.0 sc $val(sc) cp $val(cp) seed $val(seed)"
puts $tracefd "M 0.0 prop $val(prop) ant $val(ant) mac $val(mac)"
puts $tracefd "M 0.0 on_off $val(on_off)"

puts "Starting Simulation..."
$ns_ run
