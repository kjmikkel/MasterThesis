#!/usr/bin/perl
# -*- cperl -*-
# $Id: gls_evaluate.pl,v 1.3 2003/01/29 18:41:05 kiess Exp $

use strict;

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

# wk stuff
my %query_distance = ( 'max' => 0 , 'numberOfEntries' => 0);
my %cl_deviation   = ( 'max' => 0 , 'numberOfEntries' => 0);
my $query_distance_quant_step = 250;
my %lookup =('queries' => 0, 'cache_lookups' => 0, 'request_send' => 0, 
	     'reply_receive' => 0, 'request_drop' => 0, 'reply_drop' => 0, 'ifq_drop' => 0);
# the following two variables contain the position of a node when a test query 
# is launchend (that value is from GOD). If GLS does a cache lookup, we can 
# determine the deviation
my $dstx; 
my $dsty;
my $target_node;

my %cache_statistic = ('maxage' => 0,'minage' => 0,'totalage' => 0);
my $query_distance_quant_step = 250;
my $deviation_quant_step = 50;

my %query_list = (); # stores the send time of a query
my %request_time_hash = ();
my %reply_duplicate_list = ();
my %reply_statistic = ('maxage' => 0,'minage' => 0,'totalage' => 0);

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
my @LOCSTYPE = ("QUERY ", "REPLY ", "DATA    ", "UPDATE", "UPDACK", "BEACON", "BCNREQ");
my @GPSRTYPE = ("GREEDY", "PERI  ", "PROBE   ", "BEACON", "BCNREQ");
my @GOAFRTYPE= ("GREEDY", "PERI  ", "ADVANCE ", "PROBE ", "BEACON", "BCNREQ");
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
    # wk stuff
    %reply_duplicate_list = ();
    %query_list = ();
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
	print ".";
	$mark += $step;
      }
      $parsedsize += length($line);

      next if $line =~ /^V/o;

      # Parameter
      if ($line =~ /^M \d+.\d+ nn (\d+) x (\d+) y (\d+) rp (\S+)/o) {
	$nn += $1/$noFiles; $x += $2/$noFiles; $y += $3/$noFiles; $routing = $4;
	next;
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

      # wk : gls performance evaluation
      #HLS result parsing wk
      if($line =~ /^TESTQ (\d+\.\d+) (\d+) \((\d+\.\d+) (\d+\.\d+)\) (\d+) \((\d+\.\d+) (\d+\.\d+)\)/o) {
	$lookup{queries}++;
	my $actual_time = $1;
	my $source_node = $2;
	my $srcx = $3;
	my $srcy = $4;
	$target_node = $5;
	$dstx = $6;
	$dsty = $7;
	#print "$srcx, $srcy, $dstx, $dsty   +++ ";
	#print distance($srcx, $srcy, $dstx, $dsty)."\n";
	incrDistanceHash(\%query_distance, distance($srcx, $srcy, $dstx, $dsty),
			 $query_distance_quant_step);
      }

      if($line =~ /LSIIC: (\d+.\d+) _(\d+)_ \[(\d+) (\d+.\d+) (\d+.\d+) (\d+.\d+)\]/o) {
	#print "LSIIC match\n";
	$lookup{cache_lookups}++;
	my $actual_time = $1;
	my $node_id = $2;
	my $node = $3;
	if($node != $target_node) {
	  print "\n";
	  print "#########################error, wrong node######################\n";
	  print "\n";
	}
	  
	my $cache_entry_timestamp = $4;
	my $x = $5;
	my $y = $6;
	# now start calculating
	my $deviation = distance($x, $y, $dstx, $dsty);

	my $age = $actual_time - $cache_entry_timestamp;
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

      if($line =~ /LSRR: (\d+.\d+) _(\d+)_ \((\d+)->(\d+)\)/o) {
#	print $line;
	
	my $actual_time = $1;
	my $reply_receiver = $4;
	my $reply_sender = $3;
	#print "$reply_sender->$reply_receiver\n";
	
	my $key = "$reply_receiver - $reply_sender";
	my $overall_time = $actual_time - $query_list{$key};
	#print "$overall_time\n";

	if(!(exists $reply_duplicate_list{$key})) {
	  $lookup{reply_receive}++;
	  incrQuantHash(\%request_time_hash, $overall_time, 1);
	  $reply_duplicate_list{$key} = 1;
	}
      }

      # parse the receiving of a reply packet at the request sender =>
      # the age is in the packet and can be determined
      if($line =~ /r (\d+.\d+) _(\d+)_ RTR  --- \d+ LOCS .* ------- \[(\d+):255 (\d+):255 \d+ \d+\] (\d+) \[(\d+) (\d+.\d+).*\]->\[(\d+) (\d+.\d+)/o) {
	my $actual_time = $1;
	my $actual_node = $2;
	my $sender      = $3;
	my $target      = $4;
	my $pkt_type    = $5;
	# $6 is once again the sender
	my $sender_timestamp = $7;
	# $8 is the target 
	my $target_last_position_timestamp = $9;
	if(($actual_node == $target) &&
	   ($pkt_type == 2)
	  ) {
	  # it's a reply packet arriving at it's target
	  my $age = $actual_time - $sender_timestamp;
	  
	  #print "stt: $sender_target_travel_time \n";
	  if($reply_statistic{maxage} <= $age){
	    $reply_statistic{maxage} = $age
	  }
	  if($reply_statistic{minage} >= $age){
	    $reply_statistic{minage} = $age
	  }
	  $reply_statistic{totalage} += $age;
	}
      }
      
      if($line =~ /LSSRC: (\d+.\d+) _(\d+)_ \((\d+)->(\d+)\)/o) {
	$lookup{request_send}++;
	my $actual_time = $1;
	my $request_receiver = $4;
	my $request_sender = $3;
	#print "$reply_sender->$reply_receiver\n";
	
	my $key = "$request_sender - $request_receiver";
	$query_list{$key} = $actual_time;
      }


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
          elsif (uc($ptype1) eq "GOAFR") {$cause1 = $GOAFRTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "LOCS") { $cause1 = $LOCSTYPE[$psub1]; }
	  elsif (uc($ptype1) eq "PING") { $cause1 = $PINGTYPE[$psub1]; }
	  else { $cause1 = "UK/$ptype1"; }
	}
	if ($psub2 == -1) { $cause2 = $ptype2; }
	else {
	  if (uc($ptype2) eq "CBF") { $cause2 = $CBFTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "GPSR") { $cause2 = $GPSRTYPE[$psub2]; }
  	  elsif (uc($ptype2) eq "GOAFR") { $cause2 = $GOAFRTYPE[$psub2]; }
	  elsif (uc($ptype2) eq "LOCS") { $cause2 = $LOCSTYPE[$psub2]; }
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
      if($line =~ /^[srfD] \d+.\d+ _\d+_ (\w+)\s+\S+ (\d+) (\S+) \d+ \[.*- \[.*\] \d+ \[(\d+)\] \d+.\d+/o) {
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
      if($line =~ /^([srfD]) (\d+.\d+) _(\d+)_ (\w+)\s+(\S+) (\d+) (\S+) (\d+) \[\w+ \w+ (\w+).*- \[(\S+):\d+ (\S+):\d+ (\d+) (\S+)\] (.*)/o) {
	my $op           = $1;
	my $time         = $2;
	my $node         = $3;
	my $layer        = $4;
	my $drop_rsn     = $5;
	my $pkt_uid      = $6;
	my $protocol     = $7;
	my $pkt_size     = $8;
	my $fromhex      = $9;
	my $from         = hex($9);
	my $pkt_src      = $10;
	my $pkt_dst      = $11;
	my $pkt_ttl      = $12;
	my $pkt_nhop     = $13;
	my $subline      = $14;

	# Update Timestamp
	if ($time > $duration) {
	  $duration = $time;
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

	if (($protocol eq "GPSR") && 
	    ($subline =~ /^(\d+) \d+ \[\S+ \S+\]/o)) {
	  my $pkt_type     = $1;

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$GPSRTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$GPSRTYPE[$pkt_type]}++;
	  }
	  next;
	}

	if (($protocol eq "GOAFR") && 
	    ($subline =~ /^(\d+) \d+ \[\S+ \S+\]/o)) {
	  my $pkt_type     = $1;

	  # Packet Statistics
	  if ($layer eq "RTR") {
	    if ($op eq 'D') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{drop}++; }
	    if ($op eq 'r') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{recv}++; }
	    if ($op eq 'f') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{forw}++; }
	    if ($op eq 's') { $stats{$protocol}{$GOAFRTYPE[$pkt_type]}{send}++; }
	  }

	  if ($op eq 'D') {
	    my $reason = "$layer/$drop_rsn";
	    $drops{$reason}{$GOAFRTYPE[$pkt_type]}++;
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

	if (($protocol eq "Ping") &&
	    ($subline =~ /^(\d+) \[(\d+)\] \d+.\d+/o)) {
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

	      if (($miss_analysis == 1) && ($noFiles == 1)) {
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

	      if (($miss_analysis == 1) && ($noFiles == 1)) {
		$delivery_count{$pkt_uid}++;
	      }
	    }
	  }
	  if ($layer eq "RTR") {

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

######################
# wk's subs
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

sub printRequestTravelTime {
  my $key;
  print "Request Travel Time : \n";
  foreach $key (sort keys %request_time_hash) {
    printf("$key s: %.2f % \n", calcPercent( $lookup{reply_receive},
					     $request_time_hash{$key}));
  }
}

sub round {
  my $input = shift();
  $input = $input * 100;
  $input = int($input + 0.5);
  $input = $input / 100;
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

sub printWKGLSstats {

  print "GLS statistics:\n\n";
  print "Queries         : $lookup{queries}\n";
  my $avgage = -1;
  if($lookup{cache_lookups} != 0) {
    $avgage = $cache_statistic{totalage} / $lookup{cache_lookups};
  }
  print "Cache Lookups   : $lookup{cache_lookups} min age $cache_statistic{minage}, max age $cache_statistic{maxage}, avg age $avgage\n";
  $avgage = 0;
  if ($lookup{reply_receive} != 0) {
    $avgage = $reply_statistic{totalage} / $lookup{reply_receive};
  }
  print "Requests send   : $lookup{request_send} \n";
  print "Replies received: $lookup{reply_receive} min age $reply_statistic{minage}, max age $reply_statistic{maxage}, avg age $avgage\n";
  my $failure_rate = ($lookup{queries} - $lookup{cache_lookups} - $lookup{reply_receive}) / $lookup{queries};
  # following code rounds $failure_rate to two decimals behind commma
  $failure_rate = $failure_rate * 10000;
  $failure_rate = int($failure_rate + 0.5);
  $failure_rate = $failure_rate / 100;
  # end rounding
  print "failure rate    : $failure_rate %\n"; 
  print "\n";

  print "Cache Lookup deviation:\n";
  my $cache_entries_above_rr = printCacheLookupHash(\%cl_deviation,$deviation_quant_step);
  my $percentage_of_all_queries = ($cache_entries_above_rr / $lookup{queries})*100;
  printf("This corresponds to %.2f percent of all queries\n", $percentage_of_all_queries);
  print "\n";
  # requests sropped

  printRequestTravelTime();
}
#################
# end wk's subs



sub printStatistics {

  print "\nStatistics:\n";
  print "----------\n";
  print "$noFiles $routing Runs evaluated ($mac)\n";
  printf ("%i Nodes in an Area of %ix%i sqm for %7.3f secs\n",$nn, $x, $y, $duration);
  printSpeed();
  print "----------\n\n";

  printWKGLSstats();

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
  if ($miss_analysis == 1) { printDeliveryCount(); }
}
