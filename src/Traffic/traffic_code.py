import os

os.chdir("../../ns-allinone-2.33/ns-2.33/indep-utils/cmu-scen-gen/")

for i in xrange(100):
  nodes = 100 * (i + 1)
  name = "Traffic-%s.tcl" % nodes
  third = nodes / 3
  os.system("../../ns cbrgen.tcl -type tcp -nn %s -seed -mc %s -max 60 > %s" % (nodes, third, name))
  os.system("mv %s ../../../../src/Traffic/Trace/%s" % (name, name))
