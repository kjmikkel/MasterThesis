#!/usr/bin/python
#-*- coding: utf-8 -*-

# make_json.py: Script to evaluate all the traces (which will print them to a json file).
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

import os, multiprocessing
from multiprocessing import Pool

def get_json(filename):

  os.system("perl evaluate.pl -f %s" % filename)

pool = Pool(processes=multiprocessing.cpu_count() - 2)
paths = ['GPSR/', "GREEDY/", "GOAFR/", "DSDV/"]
for path in paths:
  listing = os.listdir(path)
  files = []
  for infile in listing:
    filename = path + infile
    json_name = os.path.basename(infile)[0:-3] + ".json"
    json_path = path[0:-1] + "_json/" + json_name
    if not os.path.exists(json_path):
      files.append(filename)
    
  pool.map(get_json, files)

