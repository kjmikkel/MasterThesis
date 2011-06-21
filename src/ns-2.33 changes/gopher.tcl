# GOPHER.tcl -- TCL-world configuration of GOPHER routing for ns-2

#
#
#    Copyright (C) 2000 President and Fellows of Harvard College
#
#    All rights reserved.
#
#    NOTICE: This software is provided "as is", without any warranty,
#    including any implied warranty for merchantability or fitness for a
#    particular purpose.  Under no circumstances shall Harvard University
#    or its faculty, staff, students or agents be liable for any use of,
#    misuse of, or inability to use this software, including incidental
#    and consequential damages.
#
#    License is hereby given to use, modify, and redistribute this
#    software, in whole or in part, for any commercial or non-commercial
#    purpose, provided that the user agrees to the terms of this
#    copyright notice, including disclaimer of warranty, and provided
#    that this copyright notice, including disclaimer of warranty, is
#    preserved in the source code and documentation of anything derived
#    from this software.  Any redistributor of this software or anything
#    derived from this software assumes responsibility for ensuring that
#    any parties to whom such a redistribution is made are fully aware of
#    the terms of this license and disclaimer.
#
#    Author: Brad Karp, Harvard University EECS, July, 1999
#

# ======================================================================
# Default Script Options
# ======================================================================
Agent/GOPHER set sport_        0
Agent/GOPHER set dport_        0
Agent/GOPHER set bint_         0.5 ;# beacon interval
Agent/GOPHER set bdesync_      0.5 ;# beacon desync random component
Agent/GOPHER set bexp_         [expr 3*([Agent/GOPHER set bint_]+[Agent/GOPHER set bdesync_]*[Agent/GOPHER set bint_])] ;# beacon timeout interval
Agent/GOPHER set pint_         1.5 ;# peri probe interval
Agent/GOPHER set pdesync_      0.5 ;# peri probe desync random component
Agent/GOPHER set lpexp_        8.0 ;# peris unused timeout interval
Agent/GOPHER set use_mac_      0        ;# use link breakage feedback from MAC
Agent/GOPHER set use_peri_     0	      ;# probe and use perimeters
Agent/GOPHER set verbose_      0        ;# 
Agent/GOPHER set drop_debug_   0        ;#
Agent/GOPHER set peri_proact_  1	      ;# proactively generate peri probes
Agent/GOPHER set use_implicit_beacon_ 0 ;# all packets act as beacons; promisc.
Agent/GOPHER set use_planar_   0        ;# planarize graph
Agent/GOPHER set use_loop_detect_ 0     ;# look for unexpected loops in peris
Agent/GOPHER set use_timed_plnrz_ 0     ;# replanarize periodically

# ->
set opt(ragent)         Agent/GOPHER
# <- inserted - mk
set opt(pos)		NONE			;# Box or NONE

if { $opt(pos) == "Box" } {
	puts "*** GOPHER using Box configuration..."
}

# ======================================================================
Agent instproc init args {
    eval $self next $args
}       

Agent/GOPHER instproc init args {
    eval $self next $args
}       

# ===== Get rid of the warnings in bind ================================

# ======================================================================

proc create-gopher-routing-agent { node id } {
    global ns_ ragent_ tracefd opt

    #
    #  Create the Routing Agent and attach it to port 255.
    #
    #set ragent_($id) [new $opt(ragent) $id]
    set ragent_($id) [new $opt(ragent)]
    set ragent $ragent_($id)

    ## setup address (supports hier-addr) for dsdv agent 
    ## and mobilenode
    set addr [$node node-addr]
    
    #$ragent addr $addr
    #$ragent node $node

    $ragent node $node
    if [Simulator set mobile_ip_] {
	$ragent port-dmux [$node set dmux_]
    }
    $node addr $addr
    $node set ragent_ $ragent
    
    # ->
    #$node attach $ragent 255
    $node attach $ragent [Node set rtagent_port_]
    # <- replaced - mk

    ##$ragent set target_ [$node set ifq_(0)]	;# ifq between LL and MAC
        
    # XXX FIX ME XXX
    # Where's the DSR stuff?
    #$ragent ll-queue [$node get-queue 0]    ;# ugly filter-queue hack
    $ns_ at 0.0 "$ragent_($id) start-gopher"	;# start updates

    #
    # Drop Target (always on regardless of other tracing)
    #
    set drpT [cmu-trace Drop "RTR" $node]
    $ragent drop-target $drpT
    
    #
    # Log Target
    #
    set T [new Trace/Generic]
    $T target [$ns_ set nullAgent_]
    $T attach $tracefd
    $T set src_ $id
    $ragent tracetarget $T

    # ifq
    $ragent add-ifq [$node set ifq_(0)]
}


proc gopher-create-mobile-node { id args } {
    global ns ns_ chan prop topo tracefd opt node_
    
    set ns_ [Simulator instance]
    # ->
    #if {[Simulator set EnableHierRt_]} {
    if [Simulator hier-addr?] {
    # <- replaced - mk
	if [Simulator set mobile_ip_] {
	    set node_($id) [new MobileNode/MIPMH $args
	} else {
	    set node_($id) [new Node/MobileNode/BaseStationNode $args]
	}
    } else {
	set node_($id) [new Node/MobileNode]
    }
    set node $node_($id)
    $node random-motion 0		;# disable random motion
    $node topography $topo
    
    # ->
    # XXX Activate energy model so that we can use sleep, etc. But put on 
    # a very large initial energy so it'll never run out of it.
    if [info exists opt(energy)] {
    	$node addenergymodel [new $opt(energy) $node 1000 0.5 0.2]
    }
    # <- inserted - mk

    #
    # This Trace Target is used to log changes in direction
    # and velocity for the mobile node.
    #
    set T [new Trace/Generic]
    $T target [$ns_ set nullAgent_]
    $T attach $tracefd
    $T set src_ $id
    $node log-target $T

    # ->
    if ![info exist opt(err)] {
	set opt(err) ""
    }
    # ->
    if ![info exist opt(outerr)] {
	set opt(outerr) ""
    }
    # <- inserted - to
    if ![info exist opt(fec)] {
	set opt(fec) ""
    }
    # <- inserted - mk

    # ->
    #$node add-interface $chan $prop $opt(ll) $opt(mac)	\
	#    $opt(ifq) $opt(ifqlen) $opt(netif) $opt(ant)
#    $node add-interface $chan $prop $opt(ll) $opt(mac)	\
#	    $opt(ifq) $opt(ifqlen) $opt(netif) $opt(ant) $opt(err) $opt(fec)
#    # <- replaced - mk
    # ->
    $node add-interface $chan $prop $opt(ll) $opt(mac)	\
	    $opt(ifq) $opt(ifqlen) $opt(netif) $opt(ant) $topo $opt(err) $opt(outerr) $opt(fec)
    # <- replaced - to
    #
    # Create a Routing Agent for the Node
    #
    create-$opt(rp)-routing-agent $node $id
    
    # ============================================================
    
	if { $opt(pos) == "Box" } {
		#
		# Box Configuration
		#
		set spacing 200
		set maxrow 7
		set col [expr ($id - 1) % $maxrow]
		set row [expr ($id - 1) / $maxrow]
		$node set X_ [expr $col * $spacing]
		$node set Y_ [expr $row * $spacing]
		$node set Z_ 0.0
		$node set speed_ 0.0

		$ns_ at 0.0 "$node_($id) start"
	}
	return $node
}








