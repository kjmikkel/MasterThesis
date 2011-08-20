# traffic_code.py: Script to use ns-2 cbrgen.tcl tool to generate traffic traces
#Copyright (C) 2011 Mikkel Kjær Jensen (kjmikkel@gmail.com)
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

sizes = [100, 500, 750]

for j in xrange(10):
  for i in xrange(10):
    for size in sizes:
      nodes = 10 * (i + 1)
      name = "Traffic-%s-%s-%s.tcl" % (nodes, size, j)
      if os.path.exists("/Trace/" + name):
        continue

      third = nodes / 3
      os.system("../../ns cbrgen.tcl -type tcp -nn %s -seed -mc %s -max 90 > %s" % (nodes, third, name))
      os.system("mv %s ../../../../src/Motion/Traffic/Trace/%s" % (name, name))

