#!/usr/bin/perl

# -*- cperl -*-
# $Id: evaluate.pl,v 1.4 2003/01/29 18:41:05 kiess Exp $
# edited version by Mikkel Kjær Jensen to apply to new traces and Greedy, GPSR and GOAFR

use File::Basename;
use JSON::XS;
use strict;
use List::Util qw[min max];

# Switches - to check
my $miss_analysis = 0;
my $latency_analysis = 0;
my $bDatfile = 0;
my $bLatfile = 0;
my $noFiles = 0;
my $filemark = "";
my @filelist = ();
my $quant_step = 0.25; # Quantization step size for latency spectrum

my $gen_rte = 0; # Generate RTE Files only
my $write_file;
my $actfile = "";
my $processed = 0;

#################################
#
# Intermediate Hashes
#
my %PKT = ();
my %LAT = ();
my %LOOKUP = ();

#################################
#
# Compatibility
#
my $simulator_eval = 0;
my %COMPAT = ();

#################################
#
# Data Hashes
#
my %speed = ('max' => 0, 'avg' => 0, 'cnt' => 0);
my %duplicates = ('sent' => 0, 'recv' => 0);
my %collisions = ('valid' => 0, 'cols' => 0);
my %areausage = ('valid' => 0, 'sends' => 0, 'usage' => ());
my %packetflow = ();
my %reachability = ();
my %delivery = ();
my %drops = ();
my %bw = ();
my %stats = ();
my %latency = ();
my %latency_spectrum = ();
my %gls_update = ();
my %delivery_count = ();
# wk
my %lookup =('queries' => 0, 'cache_lookups' => 0, 'request_send' => 0, 
	     'reply_receive' => 0, 'request_drop' => 0, 'reply_drop' => 0, 
	     'ifq_drop_request' => 0, 'ttl_drop_request' => 0,'cbk_drop_request' => 0,
	     'ifq_drop_reply' => 0, 'ttl_drop_reply' => 0,'cbk_drop_reply' => 0,
	     'nrte_drop_request' => 0,'nrte_drop_reply' => 0,);
my %updates_send = ();
my %updates_received = ();
my %unanswered_requests = ();
my %request_first_locate_level = (); # intermediate hash for each file
my %request_first_locate_samecell = ();# intermediate hash for each file

my %request_locate_same_cell; # overall hash storing the summary of request locates
my %request_locate_not_same_cell;# overall hash storing the summary of request locates
my $request_locate_counter = 0;

my %cache_statistic = ('maxage' => 0,'minage' => 0,'totalage' => 0);
my %reply_statistic = ('maxage' => 0,'minage' => 0,'totalage' => 0);
my %request_u_distance = ();
my %request_drop = ();
my %update_holder = (); #the one who has the latest info about node x
# contains at position x the number and position of node y who has info
# about x
my %query_distance = ( 'max' => 0 , 'numberOfEntries' => 0);
my %cl_deviation   = ( 'max' => 0 , 'numberOfEntries' => 0);
my $query_distance_quant_step = 250;
my $deviation_quant_step = 50;

my %length = ();
my %time = ();

#for the handovers
my %handovers = ('send' => 0, 'receive' => 0, 'forcereceive' => 0);
my %handovers_number = ();
my %handover_send_distance = ( 'max' => 0 , 'numberOfEntries' => 0);
my $handover_distance_quant_step = 150;
# GOAFR - advance
my %advance = ('startAdvance' => 0, 'endAdvance' => 0, 'failAdvance' => 0);
my %statGreedy = ('endGreedy' => 0, 'backGreedy' => 0);
# for CICs
my %cic_send_number_of_neighbors = ();
my %cic_send = ('total' => 0);
my %request_drop_after_cic_send = ();

my %request_send_time = ();
my %request_time_hash = ();
my $radio_range = 250;
my $cellsize = sqrt(($radio_range*$radio_range)/2);

my %request_drop_lnr_deviation = ('info_still_exact' => 0, 
				  'target_node_reachable' => 0,
				  'info_not_exact_but_node_reachable' => 0);
my $request_drop_cas_deviation_still_exact = 0; # contains the number of CAS drops of
# requests with info still exact enough

my @lnr_conana;
my @lnr_gconana;
# for connectivity analysis, this variables always contain the last CONANA
# and GCONANAN value
my %conana =('time'=>0,'node'=>0,'packetNr'=>0,'hops'=>0);
my %gconana =('time'=>0,'node'=>0,'packetNr'=>0,'hops'=>0);
my @cbk_conana;
my @cbk_gconana;
# the following hash checks if all test queries have been answered
my %queries = ();

## erroranalyze; all necessary variables start with ea_
# for each request, we store in detail what happenen, then classes are build and
# the results are printed
my %ea_requests = ();
# contains the hierachy(the cells in the hierachy) for each node. When a node 
# sends an update, the hierachy is updated.
my %ea_actual_hierachy = ();
# used to hold the calculated values when switching between one file and
# another
my %ea_reach_stats =();
my %ea_cells_reached_in_request_sender_hierachy = ();
my %ea_drop_reasons =();
my %ea_correct_cells_reached = ();
#################################
#
# Data Variables
#
my $duration = 1; # Init value determines lowest runtime
my ($nn, $x, $y) = (0,0,0);
my $routing = "NONE";
my $mac = "NONE";

#################################
#
# Lookup Tables
#
# HLS
my @HLSTYPE  = ("empty", "UPDATE","REQUEST","REPLY","CCREQ","CCREPLY","CIREQU","HNDOVER");

# end HLS
my @LOCSTYPE = ("QUERY ", "REPLY ", "DATA  ", "UPDATE", "UPDACK", "BEACON", "BCNREQ");
my @GPSRTYPE =  ("GREEDY", "PERI  ",            "PROBE ", "BEACON", "BCNREQ");
my @GREEDYTYPE = ("GREEDY",                     "PROBE ", "BEACON", "BCNREQ");
my @GOAFRTYPE = ("GOAFR",  "PERI  ", "ADVANCE",	"PROBE ", "BEACON", "BCNREQ");
my @CBFTYPE  = ("DATA  ", "RCPT  ", "RTF   ", "CTF   ", "REC   ", "ACT   ");
my @DSRTYPE  = ("RTREQ ", "RTRPLY", "RTERR ", "RTRQER", "RTRPER");
my @PINGTYPE = ("Ping", "Echo", "TOTAL ");
my @SPEEDS   = (0,10,30,50);

#################################
#
# Main
#

parseCmdLine();

if ($noFiles == 0){ usage(); }else{

  #
  # Setup
  #
  printf ("Parser started for %i Files\n",$noFiles);

  for (my $i = 0; $i < $noFiles; $i++){

    if (($miss_analysis == 1) && ($i >= 1)) { last; }

    $actfile = $filelist[$i];
    print "Processing file $actfile...";

    # Reset Intermediate Hashes
    %PKT = (); %LAT = (); %LOOKUP = ();
    %COMPAT = ();
    %cic_send_number_of_neighbors = ();
    %request_first_locate_level = ();
    %request_first_locate_samecell = ();
    %ea_requests = ();
    %queries = ();
    $simulator_eval = 0;

    # Generate RTE File
    my $write_file = 0;

    if ($gen_rte) {
      my $basename = $actfile;
      $basename =~ s/\.gz//;
      $basename =~ s/\.rte//;
      $basename = "$basename.rte";
      if (-e $basename) {
	print "(rte exists) ";
      }else{
	print "(rte create) ";
	$write_file = open(RTEFILE, ">$basename");
	print RTEFILE "# This is the mangled route file for $actfile\n";
      }
    }

    # Zip or no zip
    my $zipped = 0;
    if ($actfile =~ /\.gz$/){
      $zipped = 1;
      if (not open(FILE, "zcat $actfile |")) { print " not found\n"; next; }
      else{ print " found\n"; }
    }elsif ($actfile =~ /\.bz$/){
      $zipped = 2;
      if (not open(FILE, "bzcat $actfile |")) { print " not found\n"; next; }
      else{ print " found\n"; }
    }else{
      if (not open(FILE, "< $actfile")) { print " not found\n"; next; }
      else{ print " found\n"; }
    }

    print "Parsing";
    my $filesize = (stat($actfile))[7];

    $processed++;

    my $parsedsize = 0;
    my $step = 0;
    if ($zipped) { $step = 0.9 * $filesize; }
    else { $step = 0.1 * $filesize; }
    my $mark = $step;

    #
    # Parse through Tracefile
    #
    while (my $line = <FILE>) {

      # Progress Indicator
      if ($parsedsize >= $mark) {
	printf(".");
	$mark += $step;
      }
      $parsedsize += length($line);

      next if $line =~ /^V/o;

      # Parameter
      if ($line =~ /^M \d+.\d+ (\d+) \((\d+.\d+), (\d+.\d+), \d+.\d+\), (\S+)/o) {
	$nn = max($nn, $1 + 1);
	$x = max($x, $2);
	$y = max($y, $3);
	$routing = $4;
      }

   if ($line =~ /^M \d+.\d+ prop.*mac (\S+)/o) {
	$mac = $1;
	next;
      }

      # Speeds
        if ($line =~ /^M \d+.\d+ \d+ \(\d+.\d+, \d+.\d+, \d+.\d+\), \(\d+.\d+, \d+.\d+\), (\d+.\d+)/o) {
	if ($1 > $speed{max}) {
	  $speed{max} = $1;
	}
	$speed{avg} += $1;
	$speed{cnt}++;
	next;
      }

      # Route Information
      if($line =~ /^CBFDBG: (\d+.\d+) _(\S+)_: RouteInfo (\d+) \((\d+)->(\d+)\) : (\d+) (\d+)/o) {
	my $time = $1; my $node = $2;
	my $pkt_uid  = $3;
	my $pkt_src  = $4;
	my $pkt_dst  = $5;
	my $taken    = $6;
	my $shortest = $7;

	$LOOKUP{$pkt_uid}->{taken} = $taken;
	$LOOKUP{$pkt_uid}->{shortest} = $shortest;
      }

      # Simulator Evaluation
      if ($line =~ /^## (\S+): (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)/o) {
	my ($protocol, $mactrans, $mactraf) = ($1,$2,$3);
	my @area = ($4,$5,$6);
	my ($shortest, $greedy, $drops) = ($7,$8,$9);

	# Remember the comment
	$simulator_eval = 1;

	# Bandwidth Consumption
	$bw{MAC}{$protocol}->{cnt} += $mactrans;
	$bw{MAC}{$protocol}->{bw} += $mactraf;
	$bw{MAC}->{cnt} += $mactrans;
	$bw{MAC}->{bw} += $mactraf;

	# Area Information 
	if ($protocol eq "Ping") {
	  my $sends = 0;
	  for (my $i=0; $i < 3; $i++) {
	    $areausage{usage}{$i} += $area[$i];
	    $sends += $area[$i];
	  }
	  $areausage{sends} += $sends;
	  $areausage{valid} = 1;
	}

	# Connectivity
	if (($protocol eq "Ping") || ($protocol eq "Echo")) {
	  $reachability{$protocol}{drops} += $drops;
	  $reachability{$protocol}{shortest} += $shortest;
	  $reachability{$protocol}{greedy} += $greedy;
	}

      }

      #HLS result parsing wk
      if($line =~ /^TESTQ \d+\.\d+ (\d+) \((\d+\.\d+) (\d+\.\d+)\) (\d+) \((\d+\.\d+) (\d+\.\d+)\)/o) {
	$lookup{queries}++;
	my $src = $1;
	my $srcx = $2;
	my $srcy = $3;
	my $dst = $4;
	my $dstx = $5;
	my $dsty = $6;
	$queries{$src."-".$dst} = 1;
	#print "$srcx, $srcy, $dstx, $dsty   +++ ";
	#print distance($srcx, $srcy, $dstx, $dsty)."\n";
	incrDistanceHash(\%query_distance, distance($srcx, $srcy, $dstx, $dsty),
			$query_distance_quant_step);
      }

      #parsing the connectivity traces
      if($line =~ /^CONANA: (\d+\.\d+) _(\d+)_ (\d+) \[\d+->\d+->-?\d+\] \[(\d+)\]/o){

	$conana{time} = $1;
	$conana{node} = $2;
	$conana{packetNr} = $3;
	$conana{hops} = $4;
      }
      if($line =~ /^GCONANA: (\d+\.\d+) _(\d+)_ (\d+) \[\d+->\d+->-?\d+\] \[(\d+)\]/o){

	$gconana{time} = $1;
	$gconana{node} = $2;
	$gconana{packetNr} = $3;
	$gconana{hops} = $4;
      }

      if($line =~ /^HLS_CL    (\d+\.\d+) (\d+) \[(\d+) (\d+\.\d+) (\d+\.\d+) (\d+\.\d+)\] {(\d+\.\d+)}/o) {
	$lookup{cache_lookups}++;
	my $src = $2;
	my $dst = $3;
	my $lookup_time = $1;
	my $entry_timestamp = $4;
	my $x = $5;
	my $y = $6;
	my $deviation = $7;
	$queries{$src."-".$dst} = 0;
	# now start calculating
	my $age = $lookup_time - $entry_timestamp;
	if($cache_statistic{maxage} <= $age){
	  $cache_statistic{maxage} = $age
	}
	if($cache_statistic{minage} >= $age){
	  $cache_statistic{minage} = $age
	}
	$cache_statistic{totalage} += $age;
	# deviation
	incrDistanceHash(\%cl_deviation, $deviation,
			 $deviation_quant_step);

      }

      if($line =~ /^HLS_REQ_s (\d+\.\d+) \((\d+_\d+)\) (\d+) ->(\d+) <(\d+)/o) {
	$lookup{request_send}++;
	my $time = $1;
	my $reqid = $2;
	my $source = $3;
	my $target_node = $4;
	my $target_cell = $5;
	$queries{$source."-".$target_node} = 0;
	incrHash(\%unanswered_requests, $reqid, 1);
	$request_send_time{$reqid} = $time;
	# EA
	$ea_requests{$reqid} = ();
	initializeRequestHash($reqid, $target_node, $target_cell);
	# end EA
      }

      # request next level
      if($line =~ /^HLS_REQ_n (\d+\.\d+) \((\d+_\d+)\) (\d+) ->(\d+) <(\d+) \d+\.\d+ \d+\.\d+ \((\d+)\)>/o) {
	my $time = $1;
	my $reqid = $2;
	my $source = $3;
	my $target_node = $4;
	my $target_cell = $5;
	my $level = $6;

	# storing the next cell to which we forward this request
	my $key = "rc" . $level . "_s";
	$ea_requests{$reqid}->{$key} = $target_cell;

	# storing the cell which is really responsible for the target_node
	# on the given level
	$key = "rc" . $level . "_t";
	$ea_requests{$reqid}->{$key} = $ea_actual_hierachy{$target_node}->{$level};
	$ea_requests{$reqid}->{level} = $level;
      }

      # request locate
    if($line =~ /^HLS_REQ_l \d+\.\d+ \((\d+_\d+)\) \d+ ->\d+ \[\d+\.\d+ \d+\.\d+ \d+\.\d+\] <(\d+) (\d+) \((\d+)\)>/o) {
	my $reqid = $1;
	my $myCell = $2;
	my $targetCell = $3;
	my $targetLevel = $4;
	if (!(exists $request_first_locate_level{$reqid})){
	  # there is no entry for this request, thus this is the first time we have
	  # a location information for this request.
	  #print "entering $reqid\n";
	  $request_first_locate_level{$reqid} = $targetLevel;
	  if($myCell == $targetCell) {
	    $request_first_locate_samecell{$reqid} = 1;
	  }
	  else {
	    
	    $request_first_locate_samecell{$reqid} = 0;
	  }
	}
      }
      # end request locate

    if(($line =~ /^HLS_REP_r (\d+\.\d+) \((\d+_\d+)\) \d+ <-\d+ \[(\d+\.\d+) (\d+\.\d+) (\d+\.\d+)\]/o) && 
	 ($ea_requests{$2}->{status} ne "REP_r")) #we haven't aleready received a reply for this request
	{
	my $lookup_time = $1;
	my $reqid = $2;
	my $entry_timestamp = $3;
	my $x = $4;
	my $y = $5;

	$lookup{reply_receive}++;
	$unanswered_requests{$2}--;
	# calculation
	my $age = $lookup_time - $entry_timestamp;
	if($reply_statistic{maxage} <= $age){
	  $reply_statistic{maxage} = $age
	}
	if($reply_statistic{minage} >= $age){
	  $reply_statistic{minage} = $age
	}
	$reply_statistic{totalage} += $age;

	my $overall_time = $lookup_time - $request_send_time{$reqid};
	incrQuantHash(\%request_time_hash, $overall_time, 1);

	#EA
	if($ea_requests{$reqid}->{status} ne "" ) {
	  print "###error : $reqid previous $ea_requests{$reqid}->{status} now REP_r\n";
	} 
	$ea_requests{$reqid}->{status} = "REP_r";
      }
      
      if($line =~ /^HLS_CIC_s \d+\.\d+ \((\d+_\d+)\).*</g) {
	my $counter = 0;
	my $reqid = $1;
	# count the number of cells (the no cell entries are -1, at
	# the end of the line and won't be matched
	while($line =~ /\G(\d+),/g) { $counter++;}

	$cic_send_number_of_neighbors{$reqid} = $counter;
	$cic_send{total}++;
	incrHash(\%cic_send, $counter, 1);
      }

    if($line =~ /^HLS_REQ_d \d+\.\d+ \((\d+_\d+)\) \d+ \((\d+)\) (\S+) idev (\d+.\d+) mydev (\d+.\d+) metonode (\d+.\d+)/o) {
	my $request = $1;
	my $level = $2;
	my $reason = $3;
	my $information_deviation = $4;
	my $my_distance_to_last_known_point = $5;
	my $actual_distance = $6;
#	print "REQ_d\n";
	$request_drop{$level . " " . $reason}++;
	$lookup{request_drop}++;
	$unanswered_requests{$1}--;

	if(exists $cic_send_number_of_neighbors{$request}) {
	  # we have a drop after a cic send, analyse how much neighbors the cell 
	  # had
	  incrHash(\%request_drop_after_cic_send, 
		   $cic_send_number_of_neighbors{$request}, 1);
	}

	if($reason eq "LNR") {
	  if($information_deviation < $radio_range) {
	    $request_drop_lnr_deviation{info_still_exact} += 1;
	    
	    if($actual_distance < $radio_range) {
	      $request_drop_lnr_deviation{target_node_reachable} += 1;
	    }
	  } else {
	    if($actual_distance < $radio_range) {
	      $request_drop_lnr_deviation{info_not_exact_but_node_reachable} += 1;
	    }
	  }

	  push(@lnr_gconana, $gconana{hops});
	  push(@lnr_conana, $conana{hops});
	  # we have "consumed" the information, thus setting it to
	  # an non occuring value to indicate that there is no information
	  $gconana{hops} = -1;
	  $conana{hops} = -1;
	}
	if($reason eq "CAS"){
	  if ($information_deviation < $radio_range) {
	    $request_drop_cas_deviation_still_exact += 1;
	  }
	}

	# ea
	if($ea_requests{$request}->{status} ne "") {
	  print "#error with reqid $request previous $ea_requests{$request}->{status} now $reason\n";
	}
	$ea_requests{$request}->{status} = $reason;
	if(($reason eq "CTO") || ($reason eq "CTO")) {
	  
	}
	  
	  
      }

      if($line =~ /^HLS_REP_d \d+\.\d+ \((\d+_\d+)\)/o) {
	$lookup{reply_drop}++;
	$unanswered_requests{$1}--;
	if( $ea_requests{$1}->{status} ne "") {
	  print "error with request $1, status is  $ea_requests{$1}->{status}, but a reply to this is dropped\n";
	}
      }
      
      if($line =~ /^HLS_CC_sa (\d+\.\d+) \((\d+_\d+)\) \d+ -> \d+ \[\d+ (\d+\.\d+) (\d+\.\d+) (\d+\.\d+)\]/o) {
	my $reqid = $2;
	$ea_requests{$reqid}->{cc_sa} = "true";
      }
      # trace the packet drops (IFQ, TTL, CBK)
      if($line =~ /^D \d+\.\d+ _\d+_ RTR  (\S+) \d+ HLS \d+ .*\] (\d+) \[.*\((\d+_\d+)\)/o) {
	my $reason = $1;
	my $packet_type = $2;
	# to avoid that drops of e.g. cellcast replies falsify the result, we test if 
	# it's the correct pkt type
	if(($HLSTYPE[$packet_type] eq "REQUEST") || ($HLSTYPE[$packet_type] eq "REPLY")) {
	  if($reason eq "IFQ") {
	    if($HLSTYPE[$packet_type] eq "REQUEST") {
	      $lookup{ifq_drop_request}++;}
	    if($HLSTYPE[$packet_type] eq "REPLY") {
	      $lookup{ifq_drop_reply}++;}
	  }
	  
	  if($reason eq "TTL") {
	    if($HLSTYPE[$packet_type] eq "REQUEST") {
	      $lookup{ttl_drop_request}++;}
	    if($HLSTYPE[$packet_type] eq "REPLY") {
	      $lookup{ttl_drop_reply}++;}
	  }
	  
	  if($reason eq "CBK") {
	    if($HLSTYPE[$packet_type] eq "REQUEST") {
	      $lookup{cbk_drop_request}++;}
	    if($HLSTYPE[$packet_type] eq "REPLY") {
	      $lookup{cbk_drop_reply}++;
	      push(@cbk_gconana, $gconana{hops});
	      push(@cbk_conana, $conana{hops});
	      # we have "consumed" the information, thus setting it to
	      # an non occuring value to indicate that there is no information
	      $gconana{hops} = -1;
	      $conana{hops} = -1;
	    }
	  }
	  
	  $unanswered_requests{$3}--;
	  if(($ea_requests{$3}->{status} ne "") && ($HLSTYPE[$packet_type] eq "REPLY")){
	    print "####### error reqid $3 reason $reason previous $ea_requests{$3}->{status}\n";
	  }
	  
	  $ea_requests{$3}->{status} = $reason;
	}
      }

     # trace NRTE drops
     if($line =~ /^D \d+\.\d+ _\d+_ RTR NRTE \d+ HLS \d+ .*\] (\d+) \[.*\((\d+_\d+)\)/o) {
	my $packet_type = $1;
	if($HLSTYPE[$packet_type] eq "REQUEST") {
	  $lookup{nrte_drop_request}++;}
	if($HLSTYPE[$packet_type] eq "REPLY") {
	  $lookup{nrte_drop_reply}++;}
      
	$unanswered_requests{$2}--;
      }

      # trace in which distance to a level 3 cell we were'nt able to forward
      # a request
      if($line =~ /HLS_REQ_u \d+\.\d+ \((\d+_\d+)\) \d+ \((\d+\.\d+) (\d+\.\d+)\) <\d+ (\d+\.\d+) (\d+\.\d+) \((\d+)\)>/o) {
	my $reqid = $1;
	my $nodex = $2;
	my $nodey = $3;
	my $cellx = $4;
	my $celly = $5;
	my $level = $6;
	#calculations
	my $distance = distance($nodex,$nodey, $cellx, $celly);
	$request_u_distance{$level} += $distance;
	$request_u_distance{$level."count"} += 1;

	#EA
	
	my $deltaX = normalize($nodex - $cellx);
	my $deltaY = normalize($nodey - $celly);
	my $key = "rc" . $level . "_v"; # means "responsible cell on level x visited
	if(($deltaX < $cellsize/2) && (($deltaY < $cellsize/2) )) {
	  # this means the REQ_u has been done by a node which was in the
	  # correct cell
	  
	  #$ea_requests{$reqid}->{$key} = "1";
	} else {
	  #$ea_requests{$reqid}->{$key} = "0";
	}
	
	# end EA
      }
      # parsing the cellcast send to determine which cell we've reached (just
      # for errorcases, if we reach a cell and the first node in this cell nows the
      # location, we get a LNR or CAS error in the failure case)
      if($line =~ /HLS_CC_sr (\d+\.\d+) \((\d+_\d+)\) (\d+) -\*(\d+)/o) {
	my $time = $1;
	my $reqid = $2;
	my $source = $3;
	my $target_node = $4;
	my $actual_level =  $ea_requests{$reqid}->{level};
	$ea_requests{$reqid}->{"rc".$actual_level."_v"} = "1";

	#if($actual_level eq "3") {
	#  print "$reqid\n";
	#}

	
      }

      # parsing the updates...
      if($line =~ /HLS_UD_(r |fr) (\d+\.\d+) (\d+)<-(\d+) \[\d+\.\d+ \d+\.\d+ <\d+>\] <\d+ (\d+\.\d+) (\d+\.\d+) \((\d+)\)>/o) {
	my $timestamp = $2;
	my $receiving_node = $3;
	my $sending_node = $4;
	my $cellx = $5;
	my $celly = $6;
	my $level = $7;
	# calculations...
	if($level == 3) {
	  $update_holder{$sending_node."x"} = $cellx;
	  $update_holder{$sending_node."y"} = $celly;
	  $update_holder{$sending_node."ts"} = $timestamp;
	}
      }
      

      # end of HLS result parsing
      # wk: hls update parsing
      
      # wk: end hls update parsing
      if($line =~ /^HLS_UPD_s \d+.\d+ (\d+) <(\d+) \d+.\d+ \d+.\d+ \((\d+)\)> (\S+)/o) {
	my $node = $1;
	my $cell = $2;
	my $level = $3;
	my $reason = $4;
	incrHash(\%updates_send, "$level $reason", 1); #counts level+reason
	incrHash(\%updates_send, "$level", 1); #only counts updates per level
	if (exists $ea_actual_hierachy{$node}) {
	  # there already exists an entry for this node, thus the HASH 
	  # has been created, do nothing
	} else {
	  # create the hash
	  $ea_actual_hierachy{$node} = ();
	}
	
	# update the hierachy
	$ea_actual_hierachy{$node}->{$level} = $cell;
      }

      if($line =~ /^HLS_UD_(r |fr) \d+.\d+ \d+<-\d+ \[\d+.\d+ \d+.\d+ <\d+>\] <\d+ \d+.\d+ \d+.\d+ \((\d+)\)>/o) {
	#	print "HLS_UD matches $1 $2\n";
	incrHash(\%updates_received, "$2 $1", 1);
      }
      # handovers parsing
      if($line =~ /^HLS_H_s   \d+\.\d+ \d+ <\d+ (\d+\.\d+) (\d+\.\d+)> -> <\d+ (\d+\.\d+) (\d+\.\d+)> (\d+)/o) {
	$handovers{send}++;
	my $srcx = $1;
	my $srcy = $2;
	my $dstx = $3;
	my $dsty = $4;
	my $numberOfInfos = $5;
	#incrHash(\%handovers_number, $numberOfInfos, 1);
	incrDistanceHash(\%handover_send_distance, distance($srcx, $srcy, $dstx, $dsty),
			 $handover_distance_quant_step);
      }

      if($line =~ /^HLS_H_rr  \d+\.\d+ \d+<-\d+ \[\d+\.\d+ \d+\.\d+ <\d+>\] <\d+ \d+\.\d+ \d+\.\d+> (\d+)/o) {
	$handovers{receive}++;
	my $numberOfInfos = $1;
	incrHash(\%handovers_number, $numberOfInfos, 1);
      }

      if($line =~ /^HLS_H_fr  \d+\.\d+ \d+<-\d+ \[\d+\.\d+ \d+\.\d+ <\d+>\] <\d+ \d+\.\d+ \d+\.\d+> (\d+)/o) {
	$handovers{forcereceive}++;

	my $numberOfInfos = $1;
	incrHash(\%handovers_number, $numberOfInfos, 1);
      }
      # Start GOAFR
      # Advance
      if($line =~ /^Start the advance from/o) {
	$advance{startAdvance}++
      }

      if($line =~ /^Advanced to/o) {
	$advance{endAdvance}++;
      }

      if($line =~ /^Advance drop/o) {
	$advance{failAdvance}++;
      }

      if($line =~/Start Peri from/o) {
	$statGreedy{endGreedy}++;
      }

      if($line =~ /^Back to Greedy/o) {
	$statGreedy{backGreedy}++;
      }
      # End GOAFR

      # GLS Update Packets need their arrival checked (for now)
      if($line =~ /^LSCLS: (\d+.\d+) _(\d+)_/o) {
	$stats{LOCS}{$LOCSTYPE[3]}{recv}++;
	next;
      }

      # MAC Collision Analysis
      if (($line =~ /^MACDBG: \d+.\d+ _\d+_: COL (\S+) (\S+) (\d+) <-> (\S+) (\S+) (\d+)/o) ||
	  ($line =~ /^MACCOL (\S+) (\S+) (\d+) <-> (\S+) (\S+) (\d+)/o) )
	{
	my ($ptype1, $psub1, $puid1, $ptype2, $psub2, $puid2) = ($1,$2,$3,$4,$5,$6);
	my ($cause1, $cause2);

	if ($psub1 == -1) { $cause1 = $ptype1; }
	else {
	  if (uc($ptype1) eq "CBF") { $cause1 = $CBFTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "GPSR") { $cause1 = $GPSRTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "GREEDY") {$cause1 = $GREEDYTYPE[$psub1];}
	  elsif (uc($ptype1) eq "GOAFR") {$cause1 = $GOAFRTYPE[$psub1];}
	  elsif (uc($ptype1) eq "LOCS") { $cause1 = $LOCSTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "HLS") { $cause1 = $HLSTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "PING") { $cause1 = $PINGTYPE[$psub1]; }
	  else { $cause1 = "UK/$ptype1"; }
	}
	if ($psub2 == -1) { $cause2 = $ptype2; }
	else {
	  if (uc($ptype2) eq "CBF") { $cause2 = $CBFTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "GPSR") { $cause2 = $GPSRTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "GREEDY") {$cause2 = $GREEDYTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "GOAFR") {$cause2 = $GOAFRTYPE[$psub2];}
	  elsif (uc($ptype2) eq "LOCS") { $cause2 = $LOCSTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "HLS") { $cause2 = $HLSTYPE[$psub2];}
	  elsif (uc($ptype2) eq "PING") { $cause2 = $PINGTYPE[$psub2]; }
	  else { $cause2 = "UK/$ptype2"; }
	}
	my $reverse = 0;
	if (exists $collisions{"$cause2<->$cause1"}) {
	  $collisions{"$cause2<->$cause1"}++;
	  $collisions{cols}++;
	  $collisions{valid} = 1;
	}else{
	  $collisions{"$cause1<->$cause2"}++;
	  $collisions{cols}++;
	  $collisions{valid} = 1;
	}
      }

      # Backwards Compatibility
      if($line =~ /^([sf]) \d+.\d+ _\d+_ (\w+)\s+\S+ \d+ (\S+) (\d+).*/o) {
	my ($op,$layer,$protocol,$size) = ($1,$2,$3,$4);

	if ($layer eq "MAC") {
	  $COMPAT{bw}{$layer}{$protocol}->{cnt} += 1;
	  $COMPAT{bw}{$layer}{$protocol}->{bw} += $size;
	}
      }
      
    if($line =~ /^[srfD] -t \d+.\d+ -Hs \d+ -Hd (\w+)\s+\S+ (\d+) (\S+) \d+ \[.*- \[.*\] \d+ \[(\d+)\] \d+.\d+/o) {
	my ($layer,$uid,$protocol,$type) = ($1,$2,$3,$4);

	if (($layer eq "AGT") && ($protocol eq "Ping") && ($type == 0))
	  { $COMPAT{lookup}{$uid}{ping} = 1; }
      }
      
      
      if (($line =~ /^CBFDBG: (\d+.\d+) _(\S+)_: Sending (\d+) to Area (\d+)/o) ||
	  ($line =~ /^CBFDBG: (\d+.\d+) _(\S+)_: Sent ACT \d+ for (\d+) to Area (\d+)/o))
	{
	  my ($time,$node,$uid,$area) = ($1,$2,$3,$4);

	  if (exists $COMPAT{lookup}{$uid}->{ping}) {
	    $COMPAT{area}{usage}{$area}++;
	    $COMPAT{area}{sends}++;
	    $COMPAT{area}{valid} = 1;
	  }
	}

      # Line analysis
#                    $1    -t     $2    -Hs   $3    -Hd    $4   -Ni     -Nx              -Ny              -Nz              -Ne                  -Nl  $5   -Nw  $6       -Ma      $7        -Md    $8     -Ms $9    -Mt $10   -Is $11

    if($line =~ /^([srfdD]) -t (\d+.\d+) -Hs (-?\d+) -Hd (-?\d+) -Ni \d+ -Nx \d+(?:.\d+)? -Ny \d+(?:.\d+)? -Nz \d+(?:.\d+)? -Ne [-]?\d+(?:.\d+)? -Nl (\w+) -Nw ([\w+-]+) -Ma (\d+(?:.\d+)?) -Md ([\d\w]+) -Ms (\d+) -Mt (\d+) -Is (-?\d+).\d+ -Id (-?\d+).\d+ -It (\w+) -Il (\d+) -If (\d+(?:.\d+)?) -Ii (\d+) -Iv ([\d\s]+) (\[[-\d\s]+\])?(.*)/o) {
#-Id   $12       -It  $13  -Il $14   -If  $15           -Ii  $16  -Iv     $17     $18          $19

 #                s         -t           -Hs         -Hd         -Ni 57 -Nx 32.50 -Ny 65.16 -Nz 0.00 -Ne -1.000000 -Nl RTR -Nw --- -Ma 0 -Md 0 -Ms 0 -Mt 0 -Is 57.255 -Id -1.255 -It message -Il 32 -If 0 -Ii 0 -Iv 32 

	my $op           	= $1;
	my $time         	= $2;
	my $node         	= $3;
	my $next_node    	= $4;
	my $layer        	= $5;
	my $drop_rsn       	= $6;
	my $MAC_duration 	= $7;
	my $fromhex 		= $8;
	my $from		= hex($8);
	my $MAC_src_address     = $9;
	my $MAC_type	 	= $10;
	my $pkt_src             = $11;
	my $pkt_dst             = $12;
	my $protocol            = $13;
	my $pkt_size            = $14;
	my $pkt_flow_id         = $15;
	my $pkt_uid             = $16;
	my $pkt_ttl             = $17;
	my $pkt_ttl_list        = $18;
	my $subline             = $19;

	# Update Timestamp
	if ($time > $duration) {
	  $duration = $time;
	}

        if ($pkt_dst == $next_node) {
	  $delivery_count{$protocol}++;
  	}

	# Measure Bandwidth Consumption
	if ((($layer eq "RTR")||($layer eq "AGT")) &&
	    (($op eq 's') || ($op eq 'f'))) {
	  if ($protocol ne "Ping") {
	    $bw{$layer}->{cnt} += 1;
	    $bw{$layer}->{bw} += $pkt_size;
	    $bw{$layer}{$protocol}->{cnt} += 1;
	    $bw{$layer}{$protocol}->{bw} += $pkt_size;
	  }
	}
	if (($protocol eq "HLS") &&  ($subline =~ /^(\d+)/o)) {
	  my $pkt_type     = $1;
	#  my $locs_src     = $2;
	 # my $locs_dst     = $3;
	 # my $locs_seqno   = $4;
	 # my $locs_maxhop  = $5;
	 # my $locs_updrsn  = $6;
	 # my $locs_cbk     = $7;
	 # my $locs_nxt     = $8;

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$HLSTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$HLSTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$HLSTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$HLSTYPE[$pkt_type]}{send}++; }
	  }


	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$HLSTYPE[$pkt_type]}++;
	  }
	  next;
	}

	## normal locservice
	if (($protocol eq "LOCS") && 
	    ($subline =~ /^(\d+) \[(\S+).*\]->\[(\S+).*\] : (\d+) (\d+) : (\w+) (\d+) \d+ \[.*\[.*\[(\S+)/o)) {
	  my $pkt_type     = $1-1;
	  my $locs_src     = $2;
	  my $locs_dst     = $3;
	  my $locs_seqno   = $4;
	  my $locs_maxhop  = $5;
	  my $locs_updrsn  = $6;
	  my $locs_cbk     = $7;
	  my $locs_nxt     = $8;

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$LOCSTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$LOCSTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$LOCSTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$LOCSTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 's') {
	    # GLS Update Reason
	    if ($LOCSTYPE[$pkt_type] eq "UPDATE"){
	      $gls_update{$locs_updrsn}++;
	      $gls_update{valid} = 1;
	    }
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$LOCSTYPE[$pkt_type]}++;
	  }
	  next;
	}

	if (($protocol eq "DSR") &&
	    ($subline =~ /^\d+ \[(\d+) (\d+)\] \[(\d+) \d+ \d+ (\d+)->(\d+)\] \[(\d+) \d+ (\d+) (\d+)->(\d+)\]/o)) {
	  my $dsr_rreq       = $1;
	  my $dsr_seqno      = $2;
	  my $dsr_rrepl      = $3;
	  my $dsr_rrepl_dst  = $4;
	  my $dsr_rrepl_src  = $5;
	  my $dsr_rerr       = $6;
	  my $dsr_rerr_dst   = $7;
	  my $dsr_rerr_blink = $8;

	  # Packet Statistics
	  my $pkt_type = 0;
	  if (($dsr_rreq == 1) && ($dsr_rerr == 1)) { $pkt_type = 3; }
	  elsif (($dsr_rrepl == 1) && ($dsr_rerr == 1)) { $pkt_type = 4; }
	  elsif ($dsr_rreq == 1) { $pkt_type = 0; }
	  elsif ($dsr_rrepl == 1) { $pkt_type = 1; }
	  elsif ($dsr_rerr == 1) { $pkt_type = 2; }
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$DSRTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$DSRTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$DSRTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$DSRTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    if ($dsr_rreq == 1)  { $drops{$reason}{$DSRTYPE[0]}++; }
	    if ($dsr_rrepl == 1) { $drops{$reason}{$DSRTYPE[1]}++; }
	    if ($dsr_rerr == 1)  { $drops{$reason}{$DSRTYPE[2]}++; }
	  }
	  next;
	}

         if (($protocol eq "GOAFR") && ($pkt_ttl =~ /^(\d+) [\d\s]+/o)) {
	  my $id = ($pkt_src, $pkt_dst, $pkt_uid);

	  my $pkt_type = $1;
          my $flowid = "$pkt_src->$pkt_dst/$pkt_uid";
	  # Packet Statistics

	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 's' || $op eq 'f') {
	    $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}{hops}++;
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$GOAFRTYPE[$pkt_type]}++;
	  }

	  if ($node == $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}->{dst}) {
		if ($PKT{$GOAFRTYPE[$pkt_type]}{$flowid}->{reached} == 0) {
		  print $LOOKUP{$pkt_uid}->{taken};
		  $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}->{reached}  = 1;
		  $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}->{taken}    = $LOOKUP{$pkt_uid}->{taken};
		  $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}->{shortest} = $LOOKUP{$pkt_uid}->{shortest};
		  $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}{end}    = $time;
		  $PKT{$GOAFRTYPE[$pkt_type]}{$flowid}{endTTL} = $pkt_ttl-1;
		}
	      }

	  next;
	}

	if (($protocol eq "GREEDY") && ($pkt_ttl =~ /^(\d+) [\d\s]+/o)) {
          
	  my $id = ($pkt_src, $pkt_dst, $pkt_uid);
	  
#	  print "in the zone: $op\n";

	  my $pkt_type = $1;
          my $flowid = "$pkt_src->$pkt_dst/$pkt_uid";
	
	  # Packet Statistics
	  if ($op eq 'd' || $op eq 'D') { $stats{$protocol}{$GREEDYTYPE[$pkt_type]}{drop}++; }
	  if ($op eq 'r') { $stats{$protocol}{$GREEDYTYPE[$pkt_type]}{recv}++; }
	  if ($op eq 'f') { $stats{$protocol}{$GREEDYTYPE[$pkt_type]}{forw}++; }
	  if ($op eq 's') { $stats{$protocol}{$GREEDYTYPE[$pkt_type]}{send}++; }
	
	  if ($op eq 's' || $op eq 'f') {
	    $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}{hops}++;
	  }


	  if ($op eq 'D' or $op eq 'd') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$GREEDYTYPE[$pkt_type]}++;
	  }

	  if ($node == $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}->{dst}) {
		if ($PKT{$GREEDYTYPE[$pkt_type]}{$flowid}->{reached} == 0) {
		  $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}->{reached}  = 1;
		  $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}->{taken}    = $LOOKUP{$pkt_uid}->{taken};
		  $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}->{shortest} = $LOOKUP{$pkt_uid}->{shortest};
		  $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}{end}    = $time;
		  $PKT{$GREEDYTYPE[$pkt_type]}{$flowid}{endTTL} = $pkt_ttl-1;
		}
	      }

	  next;
	}
	
	if (($protocol eq "GPSR") && ($pkt_ttl =~ /^(\d+) [\d\s]+/o)) {
	  my $pkt_type     = $1;
	  my $flowid = "$pkt_src->$pkt_dst/$pkt_uid";
	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 's' || $op eq 'f') {
	    $PKT{$GPSRTYPE[$pkt_type]}{$flowid}{hops}++;
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$GPSRTYPE[$pkt_type]}++;
	  }


	  if ($node == $PKT{$GPSRTYPE[$pkt_type]}{$flowid}->{dst}) {
	    if ($PKT{$GPSRTYPE[$pkt_type]}{$flowid}->{reached} == 0) {
		  $PKT{$GPSRTYPE[$pkt_type]}{$flowid}->{reached}  = 1;
		  $PKT{$GPSRTYPE[$pkt_type]}{$flowid}->{taken}    = $LOOKUP{$pkt_uid}->{taken};
		  $PKT{$GPSRTYPE[$pkt_type]}{$flowid}->{shortest} = $LOOKUP{$pkt_uid}->{shortest};
		  $PKT{$GPSRTYPE[$pkt_type]}{$flowid}{end}    = $time;
		  $PKT{$GPSRTYPE[$pkt_type]}{$flowid}{endTTL} = $pkt_ttl-1;
		}
	      } 
	  elsif ($op == 's' and $node == $pkt_src) {
	    $PKT{$GPSRTYPE[$pkt_type]}{$flowid}{reached} = 0;
	  }

	  next;
	}

	if ($protocol eq "message") {
	  my $pkt_type     = $1;
	  my $flowid = "$pkt_src->$pkt_dst/$pkt_uid";
	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{"transport"}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{"transport"}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{"transport"}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{"transport"}{send}++; }
	  }

	  if ($op eq 's' || $op eq 'f') {
	    $PKT{"transport"}{$flowid}{hops}++;
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{"transport"}++;
	  }


	  if ($node == $PKT{"transport"}{$flowid}->{dst}) {
	    if ($PKT{"transport"}{$flowid}->{reached} == 0) {
		  $PKT{"transport"}{$flowid}->{reached}  = 1;
		  $PKT{"transport"}{$flowid}->{taken}    = $LOOKUP{$pkt_uid}->{taken};
		  $PKT{"transport"}{$flowid}->{shortest} = $LOOKUP{$pkt_uid}->{shortest};
		  $PKT{"transport"}{$flowid}{end}    = $time;
		  $PKT{"transport"}{$flowid}{endTTL} = $pkt_ttl-1;
		}
	      } 
	  elsif ($op == 's' and $node == $pkt_src) {
	    $PKT{"transport"}{$flowid}{reached} = 0;
	  }

	  next;
	}

	if (($protocol eq "CBF") && 
	    ($subline =~ /^(\d+) \[(\d+)\] \[(\S+) (\S+) (\S+)\] \[.*\]->\[.*\] \[.*\]/o)) {
	  my $pkt_type     = $1;
	  my $cbf_retries  = $2;
	  my $cbf_pid      = $3;
	  my $cbf_area     = $4;
	  my $cbf_sid      = $5;

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$CBFTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$CBFTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$CBFTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$CBFTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$CBFTYPE[$pkt_type]}++;
	  }
	  next;
	}

	if (($protocol eq "Ping") && ($subline =~ /^(\d+) \[(\d+)\] \d+.\d+/o)) {
	  my $ping_seqno = $1; # seqno
	  my $ping_type  = $2; # type
	  my $flowid = "$pkt_src->$pkt_dst/$pkt_uid/$ping_seqno";

	  # Bandwidth Consumption
	  if ((($layer eq "RTR")||($layer eq "AGT")) &&
	      (($op eq 's') || ($op eq 'f'))) {
	    $bw{$layer}->{cnt} += 1;
	    $bw{$layer}->{bw} += $pkt_size;
	    $bw{$layer}{$PINGTYPE[$ping_type]}->{cnt} += 1;
	    $bw{$layer}{$PINGTYPE[$ping_type]}->{bw} += $pkt_size;
	  }

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$PINGTYPE[$ping_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$PINGTYPE[$ping_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$PINGTYPE[$ping_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$PINGTYPE[$ping_type]}{send}++; }
	  }

	  # Write RTE File
	  if ( (($layer eq 'RTR') && (($op eq 's')||($op eq 'f')||($op eq 'D'))) ||
	       (($layer eq 'AGT') && (($op eq 's')||($op eq 'r'))) ) {
	    if ($write_file){ print RTEFILE "$op $time _$node\_ $layer $pkt_uid $protocol [x x $fromhex - [$pkt_src:x $pkt_dst:x] $ping_seqno [$ping_type] \n"; }
	  }

	  # Get Type for Lookup Table
	  if ($ping_type == 0) { $LOOKUP{$pkt_uid}->{ping} = 1; }

	  if ($layer eq "AGT") {
	    if ($op eq 's') {

	      if ($ping_type == 0) { $duplicates{sent}++; }

	      if (not exists $PKT{$PINGTYPE[$ping_type]}{$flowid}) {
		# Build Packet Tree
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{sends}    = 0;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{bw}       = 0;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{hops}     = 0;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{taken}    = 0;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{shortest} = 0;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{src}      = $pkt_src;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{dst}      = $pkt_dst;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{uid}      = $pkt_uid;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{start}    = $time;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{startTTL} = $pkt_ttl;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}{reached}  = 0;
	      }

	      my $key = "$pkt_src/$pkt_dst";
	      if (($ping_type == 0) && (not exists $LAT{$key}{start})) {
		$LAT{$key}{start} = $time;
	      }

	      if ($noFiles == 1) {
		$delivery_count{$pkt_uid} = 0;
	      }

	    } elsif ($op eq 'r') {

	      if ($ping_type == 0) { $duplicates{recv}++; }

	      # Build Route Tree
	      if ($node == $PKT{$PINGTYPE[$ping_type]}{$flowid}->{dst}) {
		if ($PKT{$PINGTYPE[$ping_type]}{$flowid}->{reached} == 0) {
		  $PKT{$PINGTYPE[$ping_type]}{$flowid}->{reached}  = 1;
		  $PKT{$PINGTYPE[$ping_type]}{$flowid}->{taken}    = $LOOKUP{$pkt_uid}->{taken};
		  $PKT{$PINGTYPE[$ping_type]}{$flowid}->{shortest} = $LOOKUP{$pkt_uid}->{shortest};
		  $PKT{$PINGTYPE[$ping_type]}{$flowid}{end}    = $time;
		  $PKT{$PINGTYPE[$ping_type]}{$flowid}{endTTL} = $pkt_ttl-1;
		}
	      }

	      my $key = "$pkt_src/$pkt_dst";
	      if (($ping_type == 0) && (not exists $LAT{$key}{end})) {
		$LAT{$key}{end} = $time;
	      }

	      if ($noFiles == 1) {
		$delivery_count{$pkt_uid}++;
	      }
	    }
	  }

	  print "**** $layer ****\n";
	  if ($layer eq "RTR") {
	    print "rtr\n";
	    if (($ping_type == 0) && ($op eq 'D')) { $duplicates{recv}++; }

	    # Build Packet Tree
	    if (($op eq 's')||($op eq 'f')) {
	      $PKT{$PINGTYPE[$ping_type]}{$flowid}->{sends}++;
	      $PKT{$PINGTYPE[$ping_type]}{$flowid}->{bw} += $pkt_size;
	      if (not exists $PKT{$PINGTYPE[$ping_type]}{$flowid}->{route}->{$node}) {
		$PKT{$PINGTYPE[$ping_type]}{$flowid}->{hops}++;
		$PKT{$PINGTYPE[$ping_type]}{$flowid}->{route}->{$node} = 1;
	      }
	    }

	    if ($op eq 'D') {
	      my $reason = "$layer/$drop_rsn";
	      $drops{$reason}{$PINGTYPE[$ping_type]}++;
	    }
	  }
	  next;
	    }

	# MAC layer packets should only be added to
	# the drop statistic
	if ($layer eq "MAC") {
	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$protocol}++;
	  }
	  next;
	}

	# IFQ packets should only be added to
	# the drop statistic
	if ($layer eq "IFQ") {
	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$protocol}++;
	  }
	  next;
	}

	# Catch unknown protocols
	my $unknown = "UKN/$protocol";

	# Packet Statistics
	if ($layer eq "RTR") {
	  if ($op eq 'D') { $stats{$protocol}{$unknown}{drop}++; }
	  if ($op eq 'r') { $stats{$protocol}{$unknown}{recv}++; }
	  if ($op eq 'f') { $stats{$protocol}{$unknown}{forw}++; }
	  if ($op eq 's') { $stats{$protocol}{$unknown}{send}++; }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$unknown}++;
	  }
	}

	# Debug
	#print "Ignored: $line\n";
      }
    }
    close(FILE);

    if ($write_file) { close(RTEFILE); }
    print " done\n";

    # Compatibility Evals
    if ($simulator_eval == 0) {
      foreach my $protocol (keys %{$COMPAT{bw}{MAC}}) {
	$bw{MAC}{$protocol}->{cnt} += $COMPAT{bw}{MAC}{$protocol}->{cnt};
	$bw{MAC}{$protocol}->{bw} += $COMPAT{bw}{MAC}{$protocol}->{bw};
	$bw{MAC}->{cnt} += $COMPAT{bw}{MAC}{$protocol}->{cnt};
	$bw{MAC}->{bw} += $COMPAT{bw}{MAC}{$protocol}->{bw};
      }
      my $sends = 0;
      for (my $i=0; $i < 3; $i++) {
	$areausage{usage}{$i} += $COMPAT{area}{usage}{$i};
      }
      $areausage{sends} += $COMPAT{area}{sends}++;
      if (exists $COMPAT{area}{valid}) {
	$areausage{valid} = 1;
      }
    }else{
      # Reset Variable for next file
      $simulator_eval = 0;
    }

    # Intermediate Hash Evaluation
    evalPacketFlows();
    evalDelivery();
    evalLatency();
    # Intermediate Hash Evaluation HLS
    evalRequestLocate();
    calculateEA();
  }

  # Fill Hashes
  padHash(\%latency_spectrum, $quant_step);

  # Data Display
  if (($latency_analysis == 0) && ($processed > 0)) {
    printStatistics();
    writeStatistics();
  }
}

################################################################


#################################
#
# Packet Delivery
#
sub evalDelivery {

  my $debug = 0;
  foreach my $type (keys %PKT) {
    my ($sends,$recv) = (0,0);
    foreach my $flowid (keys %{$PKT{$type}}) {
      $delivery{$type}{sends}++;
      $sends++;
      
      if ($PKT{$type}{$flowid}->{reached}) {
	$delivery{$type}{recv}++;
	$recv++;
      }
    }

    # Sample Delivery of this File
    $delivery{$type}{dels}{$delivery{$type}{cnt}} = calcPercent($sends,$recv);

    if ($debug) {
      print "sends: $sends / $delivery{$type}{sends}\n";
      print "recv: $recv / $delivery{$type}{recv}\n";
      print "smaldel $delivery{$type}{cnt}: $delivery{$type}{dels}{$delivery{$type}{cnt}} %\n";
    }

    $delivery{$type}{cnt}++;
  }
}

sub printDelivery {

  my $debug = 0;

  print "Delivery Statistics:\n\n";

  foreach my $type (keys %delivery) {

    my ($var,$cnt) = (0,0);
    my $average = calcPercent($delivery{$type}{sends},$delivery{$type}{recv});
    if ($debug) { print "Average is: $average %\n"; }

    foreach my $no (keys %{$delivery{$type}{dels}}) {
      if ($debug) { printf("$no: $delivery{$type}{dels}{$no} %s -> %f\n","%",($average - $delivery{$type}{dels}{$no})); }
      $var += (($average - $delivery{$type}{dels}{$no}) * ($average - $delivery{$type}{dels}{$no}));
      $cnt++;
    }
    if ($cnt > 1) { $var /= $cnt-1; }
    else { $var = 0; }
    my $deviation = sqrt($var);
    if ($debug) { print "Deviation is: $deviation %\n"; }

    printf("$type Packet Delivery: %i / %i -> %.2f %s (+/- %.2f)\n", 
	   $delivery{$type}{recv}/$noFiles, $delivery{$type}{sends}/$noFiles, $average, "%", $deviation);
  }

  print "\n";
}

sub printDeliveryCount {

  print "Packet Receives:\n\n";
  foreach my $uid (sort keys %delivery_count) {
    print "\t$uid \t:\t$delivery_count{$uid}\n";
  }
  print "\n";
}

# wk
sub printHLS {
  my $key;
  my $value;

  print "HLS statistics:\n\n";
  print "Queries         : $lookup{queries}\n";
  my $avgage = $cache_statistic{totalage} / $lookup{cache_lookups};
  print "Cache Lookups   : $lookup{cache_lookups} min age $cache_statistic{minage}, max age $cache_statistic{maxage}, avg age $avgage\n";
  print "Requests send   : $lookup{request_send}\n";
  $avgage = $reply_statistic{totalage} / $lookup{reply_receive};
  print "Replies received: $lookup{reply_receive} min age $reply_statistic{minage}, max age $reply_statistic{maxage}, avg age $avgage\n";
  # requests sropped
  print "Requests dropped: $lookup{request_drop} \n";
  foreach $key (sort(keys %request_drop)) {
    print "       $key : $request_drop{$key}\n"; 
  }
 
  print "Replies dropped : $lookup{reply_drop}\n";
  my $total = $lookup{ifq_drop_request} + $lookup{ifq_drop_reply};
  print "IFQ drops       : $total (req $lookup{ifq_drop_request}, reply $lookup{ifq_drop_reply})\n";
  $total = $lookup{ttl_drop_request} + $lookup{ttl_drop_reply};
  print "TTL drops       : $total (req $lookup{ttl_drop_request}, reply $lookup{ttl_drop_reply})\n";
  $total = $lookup{cbk_drop_request} + $lookup{cbk_drop_reply};
  print "CBK drops       : $total (req $lookup{cbk_drop_request}, reply $lookup{cbk_drop_reply})\n";
  $total = $lookup{nrte_drop_request} + $lookup{nrte_drop_reply};
  print "NRTE drops      : $total (req $lookup{nrte_drop_request}, reply $lookup{nrte_drop_reply})\n";
  my $failure_rate = ($lookup{queries} - $lookup{cache_lookups} - $lookup{reply_receive}) / $lookup{queries};
  # following code rounds $failure_rate to two decimals behind commma
  $failure_rate = $failure_rate * 10000;
  $failure_rate = int($failure_rate + 0.5);
  $failure_rate = $failure_rate / 100;
  # end rounding
  print "failure rate    : $failure_rate %\n"; 
  print "\n";
  
  printRequestDropLnrDeviation();
  print "CAS info still exact : $request_drop_cas_deviation_still_exact\n";
  print "Requests unforwordable distance to RC:\n";
  my $index = 1;
  my $cumulatet_distance;
  my $number;
  while($cumulatet_distance = $request_u_distance{$index}) {
    $number = $request_u_distance{$index."count"};
    my $avg = $cumulatet_distance / $number;

    print "level $index avg distance to cell : $avg\n";
    $index++;
  }
  print "Unanswered requests : \n";

  my $counter = 0;
  while (($key,$value) = each %unanswered_requests) {
    if($unanswered_requests{$key} > 0) {
      $counter++;
      print "$key, ";
    }
  }

  print "test-queries without cl or req_s : ";
  while (($key,$value) = each %queries) {
    if($queries{$key} > 0) {
      print "$key, ";
    }
  }
  print "\n";
  
  print "\nMissing replies: $counter";
  print "\n\n";
  #printConnectivityAnalisis();
  printRequestTravelTime();
  print "\n";
  printRequestFirstLocate();
  print "\n";
  printCic();
  print "\n";
  printUpdateStats();
  print "\n";
  print "Query Source - Destination Distance : \n";
  printDistanceHash(\%query_distance, $query_distance_quant_step);
  print "\n";
  print "Cache Lookup Deviation : \n";
  my $cache_entries_above_rr = printCacheLookupHash(\%cl_deviation, $deviation_quant_step);
  my $percentage_of_all_queries = ($cache_entries_above_rr / $lookup{queries})*100;
  printf("This corresponds to %.2f percent of all queries\n", $percentage_of_all_queries);
  print "\n";
  printHandoverStats();
  print "\n";
  printEA();
  print "\n------ HLS statistics end --------------------------------------\n";
}

# EA
sub initializeRequestHash {
  my $reqid = shift();
  my $target_node = shift();
  my $target_cell = shift();
  # the rc value will be set to 1 when a REQ_u in the correct cell is found
  $ea_requests{$reqid}->{rc1_v} = "0"; # responsible cell level 1 visited
  $ea_requests{$reqid}->{rc2_v} = "0"; # responsible cell level 2 visited
  $ea_requests{$reqid}->{rc3_v} = "0"; # responsible cell level 3 visited
  $ea_requests{$reqid}->{status} = "";
  $ea_requests{$reqid}->{cc_sa} = "false"; # set to true if at least one
  # cellcast answer was sent for this request
  
  # responsible cell level 1 seen from sender
  $ea_requests{$reqid}->{rc1_s} = $target_cell;
  # responsible cell level 1 seen from target node
  $ea_requests{$reqid}->{rc1_t} = $ea_actual_hierachy{$target_node}->{1};

  $ea_requests{$reqid}->{level} = 1;
}

sub calculateEA {
  # loop through the requests
  # we only want to analyze the CTO drops here !
  foreach my $key (keys(%ea_requests)) {
    if($ea_requests{$key}->{status} eq "CTO")
       {
	 #print "status : $ea_requests{$key}->{status} \n";

	 # if an cellcast answer has been send (but not received)
	 if($ea_requests{$key}->{cc_sa} eq "true") {
	   incrHash(\%ea_drop_reasons, "ctoaftercc_sa", 1);
	 } else {
	   # no cc answer send
	   my $reachability = 
	     "$ea_requests{$key}->{rc3_v}$ea_requests{$key}->{rc2_v}$ea_requests{$key}->{rc1_v}";
	   incrHash(\%ea_reach_stats, $reachability, 1 );
	   my $cells_reached = $ea_requests{$key}->{rc3_v} + $ea_requests{$key}->{rc2_v} +
	     $ea_requests{$key}->{rc1_v};
	   incrHash(\%ea_cells_reached_in_request_sender_hierachy, $cells_reached, 1);
	 }
       }
  }

  # loop through the requests
  foreach my $reqid (keys(%ea_requests)) {
    my $cells_reached = 0;
    for (my $i=1; $i < 4; $i++) {
      # if both cells are equal and the correct cell is reached, increment
      if($ea_requests{$reqid}->{"rc$i"."_s"} == $ea_requests{$reqid}->{"rc$i"."_t"}) {
	if($ea_requests{$reqid}->{"rc$i"."_s"} ne "") {
	  #print "$reqid s $ea_requests{$reqid}->{\"rc$i\".\"_s\"} t $ea_requests{$reqid}->{\"rc$i\".\"_t\"} v $ea_requests{$reqid}->{\"rc$i\".\"_v\"} cc_sa: $ea_requests{$reqid}->{cc_sa}\n";
	}
	# same cell
	if($ea_requests{$reqid}->{"rc$i"."_v"} eq "1") {
	  $cells_reached = $cells_reached + 1;
	}
      }
    }
    # the reachability is only correct for CTO at the moment
    if(($ea_requests{$reqid}->{status} eq "CTO") && !($ea_requests{$reqid}->{cc_sa} eq "true")){
      incrHash(\%ea_correct_cells_reached, $cells_reached, 1);
      #if($cells_reached == 0) {
      #	print "0 cells : $reqid\n";
      #}
    }
  }
}

sub printEA {
  # loop through the cell reachability stats
  print "Cells reached in given hierachy (seen from request sender)\n";
  print "321 level\n";
  foreach my $key (sort((keys(%ea_reach_stats)))) {
    # the value in brackets shows the value for the successfull requests
    print "$key : $ea_reach_stats{$key}\n";
  }  
  print "How much of the imagined hierachy has been reached:\n";
  foreach my $key (sort((keys(%ea_cells_reached_in_request_sender_hierachy)))) {
    print "$key : $ea_cells_reached_in_request_sender_hierachy{$key}\n";
  }
  
  print "\n";
  print "How much of the real hierachy has been reached:\n";
  
  for (my $i=0; $i < 4; $i++) {  
    print "$i cells $ea_correct_cells_reached{$i}\n";
  }
  print "\n";
  
  print "Detailed drop reasons\n";
  print " CTO after CC_sa : $ea_drop_reasons{ctoaftercc_sa}\n";
}
# end EA

sub printRequestDropLnrDeviation {
  print "Info still exact enough           : $request_drop_lnr_deviation{info_still_exact}\n";
  print "Node reachable with exact info    : $request_drop_lnr_deviation{target_node_reachable}\n";
  print "Node reachable without exact info : $request_drop_lnr_deviation{info_not_exact_but_node_reachable}\n";
  print "Unnecessary LNR-drops             : ";
  print  $request_drop_lnr_deviation{target_node_reachable} + 
    $request_drop_lnr_deviation{info_not_exact_but_node_reachable} . "\n";
}

sub printRequestTravelTime {
  my $key;
  print "Request Travel Time : \n";
  foreach $key (sort keys %request_time_hash) {
    printf("$key s: %.2f % \n", calcPercent( $lookup{reply_receive},
					     $request_time_hash{$key}));
  }
}


sub printConnectivityAnalisis {
  my $element;
  my $counter = 0;
  print "CBK reply drops connectivity : overall (greedy)\n";
  foreach $element(@cbk_conana) {
    print "$cbk_conana[$counter] ($cbk_gconana[$counter]), ";
    $counter++;
  }
  print "\n";

  print "LNR reply drops connectivity : overall (greedy)\n";
  foreach $element(@lnr_conana) {
    print "$lnr_conana[$counter] ($lnr_gconana[$counter]), ";
    $counter++;
  }
  print "\n";
  
}

sub printHandoverStats {
  my $key;
  my $totalnumber = 0;
  my $value = 0;
  print "Handovers: \n";
  print "Send           : $handovers{send}\n";
  print "Real receive   : $handovers{receive}\n";
  print "Forced receive : $handovers{forcereceive}\n";
  print "Handover Send Distance : \n";
  printDistanceHash(\%handover_send_distance, $handover_distance_quant_step);
  print "\n";

  print "Handovers number of infos per Packet \n";
  foreach $key (sort keys %handovers_number) {
    print "$key     : $handovers_number{$key}\n";
    $totalnumber += $handovers_number{$key};
    $value += ($key * $handovers_number{$key});
  }
  printf("Average of %.2f infos per handover\n",  round($value/$totalnumber));
  #print "P.S.: I know there is no plural of information...\n";
}

sub printRequestFirstLocate {
  my $key;
  print "Request first locate: \n";
  print "Same cell \n";
  foreach $key (keys %request_locate_same_cell) {
    printf("Level $key : %.2f % \n", calcPercent($request_locate_counter, $request_locate_same_cell{$key}));
  }

  print "Not Same cell \n";
  foreach $key (keys %request_locate_not_same_cell) {
    printf("Level $key : %.2f % \n", calcPercent($request_locate_counter, $request_locate_not_same_cell{$key}));
  }

sub printCic {
  my $key;
  print "CIC results\n";
  print "total cics : $cic_send{total}\n";
  foreach $key (sort keys %cic_send) {
    if($key != "total") {
      print "Cell with $key neighbors: $cic_send{$key}\n";
    }
  }
  print "\n";

  print "CIC drops with respect to number of neighbors\n";
  foreach $key (sort keys %request_drop_after_cic_send) {
     print "Cell with $key neighbors: Dropping $request_drop_after_cic_send{$key} requests\n";
  }
  
}

sub printUpdateStats {
  print "Update statistics :\n";

  # updates send
  print "send: \n";
  foreach my $key (sort(keys %updates_send)) {
    # contain the updates send by level + those by level and reason
    print "$key : $updates_send{$key}\n"; 
  }
  print "\n";
  print "received : \n";
  my $send_key;
  #updates received
  foreach my $key (sort(keys %updates_received)) {
    # parsing the level to calculate the percentage
    if($key =~ /(\d+)/o) {
      $send_key = $1;
    } else {
      $send_key = "0";
    }
    print "$key : $updates_received{$key} ("; 
    print round(calcPercent($updates_send{$send_key}, $updates_received{$key})) . " %)\n";
  }
}
  
sub evalRequestLocate {
  my $key;
  my $value; 
  # counting the total number of locates
  # accumulate our knowledge about the first locate of a request
  # (we accumulate it so that we can say: on level 1, 20 % of locates in
  # correct cell, 80 % not in correct cell)
  foreach $key (keys %request_first_locate_level) {
    $request_locate_counter++;
    if($request_first_locate_samecell{$key} == 1) {
      # the locate was in the correct cell
      incrHash(\%request_locate_same_cell, $request_first_locate_level{$key}, 1);
    }
    else {
      incrHash(\%request_locate_not_same_cell, $request_first_locate_level{$key}, 1);
    }
  }
}


}

sub distance {
  my $x1 = shift();
  my $y1 = shift();
  my $x2 = shift();
  my $y2 = shift();
  my $deltax = $x2 - $x1;
  my $deltay = $y2 - $y1;
  my $distance = sqrt(($deltax*$deltax)+($deltay*$deltay));
  return $distance;
}

sub normalize {
  my $value = shift();
  if($value < 0) {
    return -$value;
  }
  return $value;
    
}

sub incrDistanceHash {  
  my $hRef = shift();
  my $hKey = shift();
  my $step = shift();
  my $tmpQuantStep = $quant_step;
  $quant_step = $step;
  incrQuantHash($hRef, $hKey, 1);
  # store the maximum (distance)
  if($hRef->{max} < $hKey) {
    $hRef->{max} = $hKey;
  }
  # remember the total number of elements in the hash
  $hRef->{numberOfEntries}++;

  $quant_step = $tmpQuantStep;
}

sub printCacheLookupHash {
  my $hRef = shift();
  my $step = shift();
  my $value;
  my $total = $hRef->{numberOfEntries};
  my $max = $hRef->{max};
  my $actual = 0;
  my $next;
  my $radiorange = 250;
  my $lookup_abover_rr = 0;
  while ($actual < $max) {
    $value = $hRef->{$actual};
    $next = $actual + $step;
    print "$actual to $next m : " ;
    print round(calcPercent($total, $value)) . " %\n";
    $actual = $next;
    if($next > $radiorange) {
      $lookup_abover_rr += $value;
    }
  }
  print "Entries above radiorange: " .
    round(calcPercent($total, $lookup_abover_rr)) . " %\n";
  return $lookup_abover_rr;
}

sub printDistanceHash {
  my $hRef = shift();
  my $step = shift();
  my $value;
  my $total = $hRef->{numberOfEntries};
  my $max = $hRef->{max};
  my $actual = 0;
  my $next;
  while ($actual < $max) {
    $value = $hRef->{$actual};
    $next = $actual + $step;
    print "$actual to $next m : " ;
    print round(calcPercent($total, $value)) . " %\n";
    $actual = $next;
  }
}

sub round {
  my $input = shift();
   $input = $input * 100;
  $input = int($input + 0.5);
  $input = $input / 100;
}
#end wk

#################################
#
# Packet Statistics
#
sub printPktStats {

  print "Packet Statistics:\n\n";

  print"\t\t\t    Sent       Forw       Recv       Drop\n";

  foreach my $protocol (sort keys %stats) {
    printf("\tRTR/$protocol\n");
    foreach my $type (sort keys %{$stats{$protocol}}) {
      printf("\t\t$type\t%8i   %8i   %8i   %8i\n",
	     $stats{$protocol}{$type}{send}/$noFiles,
	     $stats{$protocol}{$type}{forw}/$noFiles,
	     $stats{$protocol}{$type}{recv}/$noFiles,
	     $stats{$protocol}{$type}{drop}/$noFiles);
    }
  }
  print "\n";
}

#################################
#
# Packet Drops
#
sub printDrops {

  printf("Packet Drops:\n\n");
  foreach my $reason (sort keys %drops) {
    printf("\t$reason:\n");
    foreach my $type (sort keys %{$drops{$reason}}) {
      printf("\t\t$type\t: %.2f\n",$drops{$reason}{$type}/$noFiles);
    }
  }
  print("\n");
}

#################################
#
# Reachability Information
#
sub printReachability {

  printf("No-Route Drop Reachability:\n\n");
  foreach my $proto (sort keys %reachability) {
    printf("\t$proto:\n");
    printf("\t   Dijkstra Reachable\t: %.2f / %.2f -> %.2f %s\n",
	   $reachability{$proto}{shortest}/$noFiles,
	   $reachability{$proto}{drops}/$noFiles,
	   calcPercent($reachability{$proto}{drops},$reachability{$proto}{shortest}),
	  "%");
    printf("\t   Greedy Reachable\t: %i / %i -> %.2f %s\n",
	   $reachability{$proto}{greedy}/$noFiles,
	   $reachability{$proto}{drops}/$noFiles,
	   calcPercent($reachability{$proto}{drops},$reachability{$proto}{greedy}),
	   "%");
  }
  printf("\n");

}

#################################
#
# Bandwidth
#
sub printBW {

  my $bwfac = 1;
  my $fac = 1 / $noFiles;
  my $unit = shift();

  if ($unit eq "kbps") { # Kilobit/sec
    $bwfac = 8 / (1024 * $duration * $noFiles);
  }elsif ($unit eq "mbps") { # Megabit/sec
    $bwfac = 8 / (1024 * 1024 * $duration * $noFiles);
  }else{ # Default: Kilobyte/run
    $unit = "kB/Run";
    $bwfac = 1 / (1024 * $noFiles);
  }

  printf("Bandwidth Consumption ($unit):\n\n");

  foreach my $l (sort keys %bw) {
    printf("\t$l\t:\t%.2f $unit\t(%.2f s/f)\n",$bwfac*$bw{$l}->{bw},$fac*$bw{$l}->{cnt});
    foreach my $p (sort keys %{$bw{$l}}) {
      if (($p ne "bw") && ($p ne "cnt")) {
	printf("\t  $p\t:\t%.2f $unit\t(%.2f s/f)\n", $bwfac*$bw{$l}->{$p}->{bw},$fac*$bw{$l}->{$p}->{cnt});
      }
    }
  }
  printf("\n");
}

#################################
#
# Packet Flow Information
#
sub evalPacketFlows {

  my $debug = 0;

  foreach my $type (keys %PKT) {

    foreach my $flowid (keys %{$PKT{$type}}) {

      if ($PKT{$type}{$flowid}->{reached}) {

	$packetflow{$type}{reached}{cnt}++;

	# Judge Route Quality
	$packetflow{$type}{taken} += $PKT{$type}{$flowid}->{taken};
	$packetflow{$type}{shortest} += $PKT{$type}{$flowid}->{shortest};

	# Aquire optimal route
	my $optimal;
	if ($PKT{$type}{$flowid}->{taken} < $PKT{$type}{$flowid}->{shortest}) {
	  $optimal = $PKT{$type}{$flowid}->{taken};
	}else{
	  $optimal = $PKT{$type}{$flowid}->{shortest};
	}

	# Count All
	$packetflow{$type}{reached}{sends} += $PKT{$type}{$flowid}->{sends};
	$packetflow{$type}{reached}{bw} += $PKT{$type}{$flowid}->{bw};

	my $sendbw = 0;
	if ($PKT{$type}{$flowid}->{sends} != 0) {
	  $sendbw = $PKT{$type}{$flowid}->{bw} / $PKT{$type}{$flowid}->{sends};
	}

	# Necessary is: Optimal Route with 1 Send/Hop
	$packetflow{$type}{nec}{sends} += $optimal;
	$packetflow{$type}{nec}{bw} += $optimal * $sendbw;

	# Redundant is: Everything that is not necessary
	$packetflow{$type}{red}{sends} += $PKT{$type}{$flowid}->{sends} - $optimal;
	$packetflow{$type}{red}{bw} += ($PKT{$type}{$flowid}->{sends} - $optimal) * $sendbw;
      }else{
	$packetflow{$type}{dropped}{cnt}++;
	# Count All
	$packetflow{$type}{dropped}{sends} += $PKT{$type}{$flowid}->{sends};
	$packetflow{$type}{dropped}{bw} += $PKT{$type}{$flowid}->{bw};
      }

      # Calculate Load
      $packetflow{$type}{flows}++;
      $packetflow{$type}{sends} += $PKT{$type}{$flowid}->{sends};
      $packetflow{$type}{bw} += $PKT{$type}{$flowid}->{bw};
    }
  }
}

sub printPacketFlows {

  my $bwfac = 1;
  my $fac = 1 / $noFiles;
  my $unit = shift();

  if ($unit eq "kbps") { # Kilobit/sec
    $bwfac = 8 / (1024 * $duration * $noFiles);
  }elsif ($unit eq "mbps") { # Megabit/sec
    $bwfac = 8 / (1024 * 1024 * $duration * $noFiles);
  }else{ # Default: Kilobyte/run
    $unit = "kB/Run";
    $bwfac = 1 / (1024 * $noFiles);
  }

  foreach my $type (sort keys %packetflow) {

    printf("$type Flow Information:\n\n");

    if ($packetflow{$type}{reached}{cnt} != 0) {
      printf("\tAvg Taken Route (Reached)   :\t%.2f\n",$packetflow{$type}{taken}/$packetflow{$type}{reached}{cnt});
      printf("\tAvg Shortest Route (Reached):\t%.2f\n",$packetflow{$type}{shortest}/$packetflow{$type}{reached}{cnt});
    }
    printf("\n");

    printf("\tDropped Targets         :\t%.2f \n",$packetflow{$type}{dropped}{cnt}*$fac);
    printf("\tDropped Transmissions   :\t%.2f \n",($packetflow{$type}{dropped}{sends}*$fac));
    printf("\tDropped Traffic         :\t%.2f $unit\n",($packetflow{$type}{dropped}{bw}*$bwfac));
    printf("\tReached Targets         :\t%.2f \n",$packetflow{$type}{reached}{cnt}*$fac);
    printf("\tReached Transmissions   :\t%.2f \n",($packetflow{$type}{reached}{sends}*$fac));
    printf("\tReached Traffic         :\t%.2f $unit\n",($packetflow{$type}{reached}{bw}*$bwfac));
    printf("\t   Nec. Transmissions   :\t%.2f \n",($packetflow{$type}{nec}{sends}*$fac));
    printf("\t   Nec. Traffic         :\t%.2f $unit\n",($packetflow{$type}{nec}{bw}*$bwfac));
    printf("\t   Red. Transmissions   :\t%.2f \n",($packetflow{$type}{red}{sends}*$fac));
    printf("\t   Red. Traffic         :\t%.2f $unit\n",($packetflow{$type}{red}{bw}*$bwfac));
    printf("\n");
    printf("\tTotal Targets           :\t%.2f \n",$packetflow{$type}{flows}*$fac);
    printf("\tTotal Load Transmissions:\t%.2f \n",($packetflow{$type}{sends}*$fac));
    printf("\tTotal Load Traffic      :\t%.2f $unit\n",($packetflow{$type}{bw}*$bwfac));
    printf("\n");
  }

}

#################################
#
# GOAFR
#

sub printAdvance {
  printf("Advances:\n\n");
  printf("Advances started:\t%d\n", $advance{startAdvance});
  printf("Advanced finished:\t%d\n", $advance{endAdvance});
  printf("Advance drops:\t\t%d\n\n", $advance{failAdvance});

  printf("Greedy:\n\n");
  printf("Leaves Greedy:\t%d\n", $statGreedy{endGreedy});
  printf("Goes back:\t%d\n\n", $statGreedy{backGreedy});
}

#################################
#
# Latency
#
sub printLatency {

  printf("Data Delivery Latency:\n\n");
  foreach my $type (keys %latency) {
    printf("\tAvg Hop Latency ($type)    \t: %.7f secs\n",$latency{$type}{shop}/$noFiles);
    printf("\tAvg Pkt Latency ($type)    \t: %.7f secs\n",$latency{$type}{avg}/$noFiles);
    if (exists $latency{$type}{fp}) {
      printf("\tAvg 1st Pkt Latency ($type)\t: %.7f secs\n",$latency{$type}{fp}/$noFiles);
    }
  }
  printf("\n");
}

sub evalLatency {

  foreach my $type (keys %PKT) {
    my ($avg,$avg_cnt) = (0,0);
    my ($shop,$shop_cnt) = (0,0);
    my ($rlen,$rlen_cnt) = (0,0);

    if ($latency_analysis == 1) { print "$type:\n"; }

    foreach my $flowid (keys %{$PKT{$type}}) {

      if ($PKT{$type}{$flowid}{reached} == 1) {

	my $lat = $PKT{$type}{$flowid}{end} - $PKT{$type}{$flowid}{start};

	if ($type eq $PINGTYPE[0]) {
	  incrQuantHash(\%latency_spectrum, $lat, 1);
	}

	$avg += $lat; $avg_cnt++;

	my $len = ($PKT{$type}{$flowid}{startTTL} - $PKT{$type}{$flowid}{endTTL});
	if ($len != 0) {
	  $shop += $lat / $len; $shop_cnt++;
	}

	## for now we'll update the old route length vals
	$rlen += $len; $rlen_cnt++;

	if ($latency_analysis == 1) {
	  printf (" $PKT{$type}{$flowid}{end} - $PKT{$type}{$flowid}{start} / $PKT{$type}{$flowid}{endTTL} - $PKT{$type}{$flowid}{startTTL} evals to %.2f\n", $lat/$len);
	}

      }
    }
    if ($avg_cnt != 0) {
      $latency{$type}{avg} += $avg/$avg_cnt;
    }
    if ($shop_cnt != 0) {
      $latency{$type}{shop} += $shop/$shop_cnt;
    }
  }

  my ($fp,$fp_cnt) = (0,0);
  foreach my $key (keys %LAT) {
    $fp += $LAT{$key}{end} - $LAT{$key}{start};
    $fp_cnt++;
  }
  if ($fp_cnt != 0) {
    $latency{$PINGTYPE[0]}{fp} += $fp/$fp_cnt;
  }
}

sub printLatencySpectrum {

  print "Ping Latency Spectrum (Quantization: $quant_step):\n\n";
  foreach my $delay (sort keys %latency_spectrum) {
    if ($delay eq "valid") { next; }
    printf("\t$delay\t: %.2f\n",$latency_spectrum{$delay}/$noFiles);
  }
  print "\n";
}

#################################
#
# MAC Collisions
#
sub printCollisions {

  if ($collisions{valid}) {
    printf("MAC Collision Statistics:\n\n");

    printf("\tMAC Collisions\t: %.2f\n",$collisions{cols}/$noFiles);
    foreach my $type (sort keys %collisions) {
      if (($type eq "valid") || ($type eq "cols")) { next; }
      printf("\t$type\t: %.2f %\n",calcPercent($collisions{cols},$collisions{$type}));
    }
    printf("\n");
  }
}

#################################
#
# CBF Specials
#
sub printAreaUsage {

  if ($areausage{valid}) {

    printf("CBF Area Usage:\n\n");
    printf("\tCounted Sends  :\t%.2f\n",$areausage{sends}/$noFiles);
    foreach my $area (sort keys %{$areausage{usage}}) {
      printf("\t  Usage Area $area :\t%.2f %\n",calcPercent($areausage{sends},$areausage{usage}{$area}));
    }
    printf("\n");
  }
}

sub printDuplication {

  printf("Ping Duplication Information:\n\n");
  printf("\tDuplicates/Run    :\t%.2f\n",($duplicates{recv}-$duplicates{sent})/$noFiles);
  if ($duplicates{sent} != 0) {
    printf("\tAvg Dupes/Pkt     :\t%.2f\n",($duplicates{recv}-$duplicates{sent})/$duplicates{sent});
  }
  printf("\n");

}

#################################
#
# LOCS Specials
#
sub printUpdateReasons {

  if ($gls_update{valid} == 1) {
    print "Update Packets (Reason Keyed):\n\n";
    foreach my $reason (sort keys %gls_update) {
      if ($reason eq "valid") { next; }
      printf("\t$reason\t: %.2f\n",$gls_update{$reason}/$noFiles);
      print "\n";
    }
  }
}

#################################
#
# Useful Functions / Generics
#
sub calcPercent {
  my $total = shift();
  my $fraction = shift();
  my $rate;

  if($total > 0){
    $rate = ($fraction*100)/$total;
  }else{
    $rate = 0;
  }

  return $rate;
}

sub incrQuantHash {
  my $hRef = shift();
  my $hKey = shift();
  my $incrValue = shift();

  # Quantize Key
  #print "got key: $hKey ->";
  my $index = $quant_step;
  for ($index = $quant_step; ($hKey / $index) >= 1; $index += $quant_step) {}
  $hKey = $index - $quant_step;
  #print " selected bucket: $hKey\n";

  if (exists $hRef->{$hKey}){
    $hRef->{$hKey} += $incrValue;
    #print " $hKey : $hRef->{$hKey}\n";
  }else{
    $hRef->{$hKey} = $incrValue;
  }
}

sub incrHash {
  my $hRef = shift();
  my $hKey = shift();
  my $incrValue = shift();
  if (ref($hRef) ne 'HASH'){
    die "expected a Hash-Ref!\n";
  }
  if (exists $hRef->{$hKey}){
    $hRef->{$hKey} += $incrValue;
  }else{
    $hRef->{$hKey} = $incrValue;
  }
}

sub padHash {
  my $hRef = shift();
  my $step = shift();
  my $biggest = 0;

  foreach my $k (sort keys %$hRef){
    if ($k > $biggest) {
      $biggest = $k;
    }
  }

  for (my $i = 0; $i < $biggest; $i += $step) {
    if (not exists $hRef->{$i}){
      $hRef->{$i} = 0;
    }
  }
}


#################################
#
# Usage
#
sub usage {
  print "\nUsage: evaluate.pl [Options] -f files\n\n";
  print "  Options are:\n\n";
  print "  -tag/-t [String]   - Identifer tag for .lat and .dat files\n";
  print "  -quant/-q [Value]  - Quantization step size for latency spectrum\n";
  print "  -help/-h           - Display this usage message\n";
  print "  -dat/-d            - Write/Append dat file X.dat\n";
  print "                       (X is TAG or the ROUTING protocol read from the file/s)\n";
  print "  -lat/-l            - Write Latency Spectrum to lat file X-NN-SP.lat\n";
  print "                       (X is TAG or the ROUTING protocol read from the file/s)\n";
  print "                       (NN is the number of nodes as read from the file/s)\n";
  print "                       (SP is the max movement speed as read from the file/s)\n";
  print "  -latana/-la        - Show details of single hop latency calculation\n";
  print "  -missana/-ma       - Activate packet analysis (Parses only the first file)\n";
  print "  -genrte/-rte       - Generate Route Files for Traces that don't have 'em yet\n";
  print "\n";
  exit;
}

#################################
#
# Command Line Parsing
#
sub parseCmdLine {

  for (my $i = 0; $i <= $#ARGV; $i++) {

    if ((($ARGV[$i] eq "-tag") || ($ARGV[$i] eq "-t")) &&
	($ARGV[$i+1] !~ /^-/o) && ($ARGV[$i+1] ne "")) 
      {
	$filemark = $ARGV[$i+1];
	$i++;
	next;
      }

    if ((($ARGV[$i] eq "-quant") || ($ARGV[$i] eq "-q")) &&
	($ARGV[$i+1] !~ /^-/o) && ($ARGV[$i+1] ne "")) 
      {
	if (($ARGV[$i+1] =~ /\d+.\d+/o) || ($ARGV[$i+1] =~ /\d+/o)) {
	  $quant_step = $ARGV[$i+1];
	  $i++;
	}
	next;
      }

    if (($ARGV[$i] eq "-files") || ($ARGV[$i] eq "-f")) {
      while (($ARGV[$i+1] !~ /^-/o) && ($ARGV[$i+1] ne "")) {
		$filelist[$noFiles] = $ARGV[$i+1];
		$noFiles++;
		$i++;
      }
      next;
    }

    if (($ARGV[$i] eq "-help") || ($ARGV[$i] eq "-h")) { usage(); exit; }
    if (($ARGV[$i] eq "-dat") || ($ARGV[$i] eq "-d")) { $bDatfile = 1; next; }
    if (($ARGV[$i] eq "-lat") || ($ARGV[$i] eq "-l")) { $bLatfile = 1; next; }
    if (($ARGV[$i] eq "-missana") || ($ARGV[$i] eq "-ma")) { $miss_analysis = 1; next; }
    if (($ARGV[$i] eq "-latana") || ($ARGV[$i] eq "-la")) { $latency_analysis = 1; next; }
    if (($ARGV[$i] eq "-genrte") || ($ARGV[$i] eq "-rte")) { $gen_rte = 1; next; }
  }
}

#################################
#
# File I/O
#
sub writeStatistics {

  if ($filemark eq "") { $filemark = "$routing"; }

  my $proto = lc($filemark);

  if ($bLatfile == 1) {
    my $specfile = "$proto-$nn-$speed{max}.lat";
    open(LATSPEC, "> $specfile") or die "\tCan not open $specfile\n";
    foreach my $k (sort keys %latency_spectrum){
      printf(LATSPEC "$k %.2f\n",$latency_spectrum{$k}/$noFiles);
    }
    close(LATSPEC);
  }

  # Direct-to-File Output should be rwritten someday

  #if ($bDatfile == 1) {
  #  my $datfile = "$proto.dat";
  #  open(OUT, ">> $datfile") or die "\tCan not open $datfile\n";
  #  # NN SP PDEL EDEL PHLAT EHLAT PALAT EALAT FPLAT BO ABW PRLEN ERLEN NRTEREC
  #  # 
  #  close(OUT);
  #}
}

#################################
#
# Header / Structuring
#
sub printSpeed {

  if ($speed{cnt} != 0) {
    $speed{avg} /= $speed{cnt};
  }
  my $s = 0;
  for (my $j = $#SPEEDS; $j >= 0; $j--) {
    if ($speed{max} <= $SPEEDS[$j]) {
      $s = $SPEEDS[$j];
    }
  }
  $speed{max} = $s;

  printf ("Nodes moved at max. %i m/s (Average: %7.3f m/s)\n",$speed{max},$speed{avg});
}

sub do_tests {
  # $GOAFR = ();
  # $GPSR = ();
  # $GREEDY = ();
  # $DSR = ();

  my $directory = '/tmp';
  opendir (DIR, $directory) or die $!;
#  get_all_files(DIR);
}

sub get_all_files {
  my $DIR = $_[0];

}

sub do_individual_tests {
  my $filename = $_[0];
  
  $filename =~ /^(\d+)-(\d+).tr/o;
  
  my $nn = $0;
  my $size = $1;
  

  my $num_nodes = 0;
#  my $send = $delivery{$type}{sends};
 # my $recv = $delivery{$type}{recv};
}

sub printStatistics {

  print "\nStatistics:\n";
  print "----------\n";
  print "$noFiles $routing Runs evaluated ($mac)\n";
  printf ("%i Nodes in an Area of %ix%i sqm for %7.3f secs\n",$nn, $x, $y, $duration);
  printSpeed();
  print "----------\n\n";

  #wk
  #printHLS();
  #end wk
#  printAdvance();
  printDelivery();
  printPktStats();
  printDrops();
  printReachability();
  printBW();
  printPacketFlows();
  printLatency();
  
  printCollisions();

  # Specials
  printAreaUsage();
  printDuplication();
  printUpdateReasons();

  # Enduring Plots
  printLatencySpectrum();
  printDeliveryCount();

  save_results();
}


sub save_results {
  my $send = 0;
  my $recv = 0;
  
  my ($filename, $direct, $suf) = fileparse($actfile, (".tr"));

  $filename =~ /^(\w+)-/o;
  $filename = "$1_json/$filename";

  foreach my $type (keys %delivery) {
    $send += $delivery{$type}{sends};
    $recv += $delivery{$type}{recv};
  }
 
  my @percent = [$recv, $send];

  my @hops_taken = ();
  my @time_taken = ();

  foreach my $type (sort keys %PKT){
    foreach my $flow_id (sort keys %{$PKT{$type}}) {
      if ($PKT{$type}{$flow_id}->{reached} == 1) {
      
	# The number of hops required for the message to arrive
	my $hops = $PKT{$type}{$flow_id}->{hops};
	push(@hops_taken, $hops);
	
	# Time required for sending the message
	my $start = $PKT{$type}{$flow_id}->{start};
	my $end   = $PKT{$type}{$flow_id}->{end};
	my $time = $end - $start;
	push(@time_taken, $time);

      }
    }
  }

  my @data = ([@hops_taken], [@time_taken], @percent);
 
  my $json_result = encode_json [@data];
  
  open FILE, ">$filename.json" or die $!;
  print FILE $json_result;
  close FILE;

}
