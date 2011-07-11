#!/bin/sh

NS_ROOT="../../ns-allinone-2.33/ns-2.33/"

# The test folder is copied
cp -rf goafr_test $NS_ROOT

# Move to directory and run test
cd ../../ns-allinone-2.33/ns-2.33/goafr_test/

../ns goafr_test_00.tcl