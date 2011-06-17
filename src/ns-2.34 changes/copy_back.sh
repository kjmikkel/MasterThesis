#!/bin/sh

NS_ROOT="../../ns-allinone-2.34/ns-2.34/"

cp Makefile $NS_ROOT"Makefile"
cp priqueue.cc $NS_ROOT"/queue/priqueue.cc"
cp packet.h $NS_ROOT"/common/packet.h"
cp cmu-trace.h $NS_ROOT"/trace/cmu-trace.h"
cp cmu-trace.cc $NS_ROOT"/trace/cmu-trace.cc"
cp ns-packet.tcl $NS_ROOT"/tcl/lib/ns-packet.tcl"
cp ns-lib.tcl $NS_ROOT"/tcl/lib/ns-lib.tcl"
cp ns-agent.tcl $NS_ROOT"/tcl/lib/ns-agent.tcl"
cp ns-mobilenode.tcl $NS_ROOT"/tcl/lib/ns-mobilenode.tcl"

cp -rf wfrp $NS_ROOT
cp -rf greedy $NS_ROOT