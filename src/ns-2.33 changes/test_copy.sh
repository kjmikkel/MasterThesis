#!/bin/sh

NS_ROOT="../../ns-allinone-2.33/ns-2.33/"
SRC_FOLDER="../../../src/ns-2.33 changes/"

# The test folder is copied
cp -rf goafr_test $NS_ROOT

# Move to directory and run test
cd $NS_ROOT"goafr_test/"

../ns goafr_test_00.tcl
cp goafr_test.tr $SRC_FOLDER"goafr_test/."
cp goafr_00.nam $SRC_FOLDER"goafr_test/."