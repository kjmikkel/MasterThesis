#!/usr/bin/python
#-*- coding: utf-8 -*-

# Gauss-Markov.py.py: Script to use the BonnMotion tool to generate movement traces for the ns-2
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
os.chdir("../Processed Motion/GaussMarkov/")
files = os.listdir(os.getcwd())
m = re.compile("(\d+)-(\d+)-1.0")
for f in files:
  res = m.search(f)
  if res:
    new_name = "GaussMarkov-%s-%s-0.ns_movements" % (res.group(1), res.group(2))
    os.system("mv %s %s" % (f, new_name))

"""
os.chdir("../../../bonnmotion-1.5a/bin/")
size_option = [100, 500, 750]
for size in size_option:
  for j in xrange(10):
    for i in xrange(10):
      nodes = 10 * (i + 1)
      max_speed = 2
      name = "GaussMarkov-%s-%s-%s" % (nodes, size, j)
      if os.path.exists("../Processed Motion/GaussMarkov/" + name + ".ns_movements"):
        continue

      os.system("./bm -f %s GaussMarkov -i 120 -n %s -x %s -y %s -z 0 -d 150 -h %s -u %s" % (name, nodes, size, size, max_speed, 1))

      os.system("./bm NSFile -f %s" % name)
      os.system("mv %s.ns_params ../../src/Motion/Parameters\ for\ Motion/GaussMarkov/." % name)
      os.system("mv %s.params ../../src/Motion/Parameters\ for\ Motion/GaussMarkov/." % name)
      os.system("mv %s.ns_movements ../../src/Motion/Processed\ Motion/GaussMarkov/." % name)
