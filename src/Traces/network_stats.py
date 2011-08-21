#!/usr/bin/python
#-*- coding: utf-8 -*-

# network_stats.py: Script to analyse the json files produced by make_json.
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

import os, json, math

def sample_deviation(sample_list):
  mean = average_list(sample_list)

  accum = 0
  for val in sample_list:
    diff = val - mean
    accum += diff * diff
  
  return math.sqrt(accum / len(sample_list))

def average_list(avg_list):
  return sum(avg_list) * 1.0 / len(avg_list) * 1.0

def average_suite(data_list, min_list, max_list):
  average     = average_list(data_list)
  min_average = average_list(min_list)
  max_average = average_list(max_list)
  std_value   = sample_deviation(data_list)
  return (average, min_average, max_average, std_value)

paths = ["GREEDY", "GPSR", "GOAFR", "DSDV"]
sizes = [100, 500, 750]

for path in paths:
  for i in xrange(10):
    nodes = 10 * (i + 1)
    for size in sizes:
      hops = []
      time = []
      recv = 0
      sends = 0
      
      min_hops = []
      max_hops = []

      min_time = []
      max_time = []

      for j in xrange(10):
        filename = "%s_json/%s-%s-%s-%s.json" % (path, path, nodes, size, j)
        f = open(filename, 'r')
	json_data = f.read()
	f.close()
	data = json.loads(json_data)

	# Get insert the data	
        local_hops = data[0]
	local_time = data[1]
        local_percent = data[2]
        
        hops.extend(local_hops)
	time.extend(local_time)
	if local_time == []:
          print filename
        min_time.append(min(local_time))
        max_time.append(max(local_time))
        
	min_hops.append(min(local_hops))
	max_hops.append(max(local_hops))

        recv  += local_percent[0]
        sends += local_percent[1]
      
      # Find the values:
      percent = (recv * 1.0 / sends * 1.0) * 100
      filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
      
      print filename 
      hop_data  = average_suite(hops, min_hops, max_hops)
      time_data = average_suite(time, min_time, max_time)

      data = (percent, hop_data, time_data)

      save_data = json.dumps(data)
      
      f = open(filename, "w")
      f.write(save_data)
      f.close()

