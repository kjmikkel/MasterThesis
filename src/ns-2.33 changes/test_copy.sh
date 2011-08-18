#!/bin/sh

NS_ROOT="../../ns-allinone-2.33/ns-2.33/"

# The test folder is copied
cp -rf goafr_test $NS_ROOT

# Move to directory and run test
cd $NS_ROOT"goafr_test/"
python unified_test.py