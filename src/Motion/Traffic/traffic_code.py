import os, re

"""
os.chdir("Trace")
files = os.listdir(os.getcwd())
m = re.compile("(\d+)")
for f in files:
  res = m.search(f)
  new_name = "Traffic-%s-1.tcl" % (res.group(1))
  os.system("mv %s %s" % (f, new_name))
"""

os.chdir("../../../ns-allinone-2.33/ns-2.33/indep-utils/cmu-scen-gen/")

for j in xrange(10):
  for i in xrange(100):
    nodes = 10 * (i + 1)
    name = "Traffic-%s-%s.tcl" % (nodes, j)
    third = nodes / 3
    os.system("../../ns cbrgen.tcl -type tcp -nn %s -seed -mc %s -max 90 > %s" % (nodes, third, name))
    os.system("mv %s ../../../../src/Motion/Traffic/Trace/%s" % (name, name))

