#!/usr/bin/python
#-*- coding: utf-8 -*-

# unified_test.py: Script to automaticly run several tests on the ns-2
#Copyright (C) 2011 Mikkel Kjær Jensen (kjmikkel@gmail.com)
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import os, multiprocessing
from multiprocessing import Pool

def do_test((j, i, algo, size)):
  nn = str(10 * (i + 1))

  filename = "../../../src/Traces/%s/%s-%s-%s-%s.tr" % (algo, algo, nn, size, j)

  if os.path.exists(filename):
    statinfo = os.stat(filename)
    if statinfo.st_size > 0:
      return
  
  output_name = "%s-%s-%s-%s.tr" % (algo, nn, size, j)
  if algo != "DSDV":
    tcl_do = """ #author: Thomas Ogilvie 
# sample tcl script showing the use of %s and HLS (hierarchical location service)

## %s Options
Agent/%s set bdesync_                0.5 ;# beacon desync random component
Agent/%s set bexp_                   [expr 3*([Agent/%s set bint_]+[Agent/%s set bdesync_]*[Agent/%s set bint_])] ;# beacon timeout interval
Agent/%s set pint_                   1.5 ;# peri probe interval
Agent/%s set pdesync_                0.5 ;# peri probe desync random component
Agent/%s set lpexp_                  8.0 ;# peris unused timeout interval
Agent/%s set drop_debug_             1   ;#
Agent/%s set peri_proact_            1 	 ;# proactively generate peri probes
Agent/%s set use_implicit_beacon_    1   ;# all packets act as beacons; promisc.
Agent/%s set use_timed_plnrz_        0   ;# replanarize periodically
Agent/%s set use_congestion_control_ 0
Agent/%s set use_reactive_beacon_    0   ;# only use reactive beaconing

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
set val(ifqlen)		512       ;# max packet in ifq
set val(seed)		1.0
set val(adhocRouting)	%s        ;# AdHoc Routing Protocol
set val(use_gk)		0	  ;# > 0: use GridKeeper with this radius
set val(zip)		0         ;# should trace files be zipped

set val(agttrc)         ON ;# Trace Agent
set val(rtrtrc)         ON ;# Trace Routing Agent
set val(mactrc)         ON ;# Trace MAC Layer 
set val(movtrc)         ON ;# Trace Movement 

set val(param)          \"../../../src/Motion/Parameters\ for\ Motion/GaussMarkov/GaussMarkov-%s-%s-%s.ns_params\"
set val(lt)	        \"\" 
set val(cp)		\"../../../src/Motion/Traffic/Trace/Traffic-%s-%s-%s.tcl\" 
set val(sc)		\"../../../src/Motion/Processed\ Motion/GaussMarkov/GaussMarkov-%s-%s-%s.ns_movements\"  
puts $val(cp)
set val(out)            \"../../../src/Traces/temp/%s\"

source $val(param)

Agent/%s set locservice_type_ 3

add-all-packet-headers
remove-all-packet-headers
add-packet-header Common Flags IP LL Mac Message %s LOCS SR RTP Ping HLS

Agent/%s set bint_                  $val(bint)
# Recalculating bexp_ here
Agent/%s set bexp_                 [expr 3*([Agent/%s set bint_]+[Agent/%s set bdesync_]*[Agent/%s set bint_])] ;# beacon timeout interval
Agent/%s set use_peri_              $val(use_peri)
Agent/%s set use_planar_            $val(use_planar)
Agent/%s set use_mac_               $val(use_mac)
Agent/%s set use_beacon_            $val(use_beacon)
Agent/%s set verbose_               $val(verbose)
Agent/%s set use_reactive_beacon_   $val(use_reactive)
Agent/%s set use_loop_detect_       $val(use_loop)

CMUTrace set aggregate_mac_           $val(agg_mac)
CMUTrace set aggregate_rtr_           $val(agg_rtr)

# seeding RNG
ns-random $val(seed)

# create simulator instance
set ns_		[new Simulator]

$ns_ use-newtrace

# Outputs nam traces 
set nf [open trace.nam w]
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
puts \"Configuring Nodes ($val(nn))\"
$ns_ node-config -adhocRouting $val(adhocRouting) \\
                 -llType $val(ll) \\
                 -macType $val(mac) \\
                 -ifqType $val(ifq) \\
                 -ifqLen $val(ifqlen) \\
                 -antType $val(ant) \\
                 -propType $val(prop) \\
                 -phyType $val(netif) \\
                 -channel $chanl \\
		 -topoInstance $topo \\
                 -wiredRouting OFF \\
		 -mobileIP OFF \\
		 -agentTrace $val(agttrc) \\
                 -routerTrace $val(rtrtrc) \\
                 -macTrace $val(mactrc) \\
                 -movementTrace $val(movtrc)

#
#  Create the specified number of nodes [$val(nn)] and \"attach\" them
#  to the channel. 
for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node]
    $node_($i) random-motion 0		;# disable random motion
	set ragent [$node_($i) set ragent_]
	$ragent install-tap [$node_($i) set mac_(0)]

    if { $val(mac) == \"Mac/802_11\" } {      
	## bind MAC load trace file
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
    $ns_ at $val(duration).0 \"$node_($i) reset\";
}

$ns_ at  $val(duration).0002 "puts \\\"NS EXITING... $val(out)\\\" ; $ns_ halt"

# A finish proc to flush traces and out call nam
proc finish {} {
        global ns nf
        $ns flush-trace
        close $nf
        exit 0
}

puts \"Starting Simulation...\"
$ns_ run
""" %(# Titles
      algo, algo, 
      # Agents
      algo, algo, algo, algo, algo, 
      algo, algo, algo, algo, algo, 
      algo, algo, algo, algo, 
      # Ad hoc Algorithm
      algo, 
      # Params
      nn, size, j,
      # Traffic
      nn, size, j,
      # Motion
      nn, size, j,
      #Output
      output_name,
      # Agents
      algo, algo, algo, algo, algo, 
      algo, algo, algo, algo, algo, 
      algo, algo, algo, algo)
  else:
    tcl_do = """ #author: Thomas Ogilvie 
set val(chan)		Channel/WirelessChannel
set val(prop)		Propagation/TwoRayGround
set val(netif)		Phy/WirelessPhy
set val(mac)		Mac/802_11
set val(ifq)		Queue/DropTail/PriQueue
set val(ll)		LL
set val(ant)		Antenna/OmniAntenna
set val(ifqlen)		512       ;# max packet in ifq
set val(seed)		1.0
set val(adhocRouting)	%s        ;# AdHoc Routing Protocol
set val(use_gk)		0	  ;# > 0: use GridKeeper with this radius
set val(zip)		0         ;# should trace files be zipped

set val(agttrc)         ON ;# Trace Agent
set val(rtrtrc)         ON ;# Trace Routing Agent
set val(mactrc)         ON ;# Trace MAC Layer 
set val(movtrc)         ON ;# Trace Movement 

set val(param)          \"../../../src/Motion/Parameters\ for\ Motion/GaussMarkov/GaussMarkov-%s-%s-%s.ns_params\"
set val(lt)	        \"\" 
set val(cp)		\"../../../src/Motion/Traffic/Trace/Traffic-%s-%s-%s.tcl\" 
set val(sc)		\"../../../src/Motion/Processed\ Motion/GaussMarkov/GaussMarkov-%s-%s-%s.ns_movements\"  
puts $val(cp)
set val(out)            \"../../../src/Traces/temp/%s\"

source $val(param)

# seeding RNG
ns-random $val(seed)

# create simulator instance
set ns_		[new Simulator]

$ns_ use-newtrace

# Outputs nam traces 
set nf [open trace.nam w]
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
puts \"Configuring Nodes ($val(nn))\"
$ns_ node-config -adhocRouting $val(adhocRouting) \\
                 -llType $val(ll) \\
                 -macType $val(mac) \\
                 -ifqType $val(ifq) \\
                 -ifqLen $val(ifqlen) \\
                 -antType $val(ant) \\
                 -propType $val(prop) \\
                 -phyType $val(netif) \\
                 -channel $chanl \\
		 -topoInstance $topo \\
                 -wiredRouting OFF \\
		 -mobileIP OFF \\
		 -agentTrace $val(agttrc) \\
                 -routerTrace $val(rtrtrc) \\
                 -macTrace $val(mactrc) \\
                 -movementTrace $val(movtrc)

#
#  Create the specified number of nodes [$val(nn)] and \"attach\" them
#  to the channel. 
for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node]
        $node_($i) random-motion 0		;# disable random motion
	

       # set ragent [$node_($i) set ragent_]
       # $ragent install-tap [$node_($i) set mac_(0)]

    if { $val(mac) == \"Mac/802_11\" } {      
	## bind MAC load trace file
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
    $ns_ at $val(duration).0 \"$node_($i) reset\";
}

$ns_ at  $val(duration).0002 "puts \\\"NS EXITING... $val(out)\\\" ; $ns_ halt"

# A finish proc to flush traces and out call nam
proc finish {} {
        global ns nf
        $ns flush-trace
        close $nf
        exit 0
}

puts \"Starting Simulation...\"
$ns_ run
""" %(# Ad hoc Algorithm
      algo, 
      # Params
      nn, size, j,
      # Traffic
      nn, size, j,
      # Motion
      nn, size, j,
      #Output
      output_name)

  filename = "test_%s_%s_%s-%s.tcl" % (i, algo, size, j)
  f = open(filename, "w")
  f.write(tcl_do)
  f.close()
      
  os.system("../ns %s" % filename)
  os.system("rm %s" % filename)
  os.system("mv ../../../src/Traces/temp/%s ../../../src/Traces/%s/%s"  % (output_name, algo, output_name))
 
sizes = ["500", "750"]
algos = ["GOAFR", "GREEDY", "GPSR", "DSDV"]
exp_size = [10]
time = "140"



pool = Pool(5)

for e_size in exp_size:    
  for j in xrange(10):
    list_param = []
    for i in xrange(e_size):
      for size in sizes:
        for algo in algos:
          list_param.append((j, i, algo, size))
     
    pool.map(do_test, list_param)



