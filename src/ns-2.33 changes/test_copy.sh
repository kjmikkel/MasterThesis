#!/bin/sh

NS_ROOT="../../ns-allinone-2.33/ns-2.33/"
SRC_FOLDER="../../../src/ns-2.33 changes/"
VISULIZE="../../../src/python movement visualization/test_results/"

# The test folder is copied
cp -rf goafr_test $NS_ROOT
#cp gls_evaluate.pl "../python movement visualization/test_results/."

# Move to directory and run test
cd $NS_ROOT"goafr_test/"
python unified_test.py

#../ns goafr_test_00.tcl
#../ns gpsr_test_00.tcl
#cp goafr_test_00.tr "$SRC_FOLDER""goafr_test/."
#cp gpsr_test_00.tr "$SRC_FOLDER""goafr_test/."

#cp goafr_test_00.tr "$VISULIZE""."
#cp gpsr_test_00.tr "$VISULIZE""."

#cp goafr_00.nam "$SRC_FOLDER""goafr_test/."
#cp gpsr_00.nam "$SRC_FOLDER""goafr_test/."

cd "$VISULIZE"
#perl evaluate.pl -f goafr_test_00.tr
#perl evaluate.pl -f gpsr_tst

#python trace_analysis.py

#cd tex
#pdflatex tex_test.tex
