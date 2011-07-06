#!/bin/sh

NS_ROOT="../../ns-allinone-2.33/ns-2.33/"

cp Makefile $NS_ROOT"/."

cp packet.h $NS_ROOT"/common/."

# The test folder is copied
cp -rf goafr_test $NS_ROOT

# Move to directory and run test
cd ../../ns-allinone-2.33/ns-2.33/goafr_test/
../ns goafr_test_00.tcl