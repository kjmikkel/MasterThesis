#!/usr/bin/python
#-*- coding: utf-8 -*-

# make_graphs.py: Script to create the graphs from the results of the analysed json files
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

import os, json

latex_location = '../../report/results/graph/'

def save_file(file_name, data):
  file_name += '.tex'
  with open(file_name, mode='w') as f:
    f.write(data)  

def find_percent_points(size, path):
  points = ""
  for i in xrange(10):  
    nodes = (i + 1) * 10
    filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
   
    f = open(filename, 'r')
    json_data = f.read()
    f.close()
    data = json.loads(json_data)
    percent = data[0]

    points += "\t(%s, %s)\n" % (nodes, percent)

  return points

def make_percentage_graph():
  
  default1 = "\\addplot[color=%s, mark=%s] coordinates{                \n" 
  default2 = "\\addplot[color=%s, mark=%s, densely dashed] coordinates{\n" 
  
  GPSR_100   = default1 % ("blue",  "*")
  GOAFR_100  = default1 % ("black", "triangle*")
  GREEDY_100 = default1 % ("red",   "square*")

  GPSR_250   = default2 % ("blue", "o")
  GOAFR_250  = default2 % ("black", "triangle")
  GREEDY_250 = default2 % ("red",   "square")

  GPSR_100   += find_percent_points(100, "GPSR")
  GOAFR_100  += find_percent_points(100, "GOAFR")
  GREEDY_100 += find_percent_points(100, "GREEDY")

  GPSR_250   += find_percent_points(250, "GPSR")
  GOAFR_250  += find_percent_points(250, "GOAFR")
  GREEDY_250 += find_percent_points(250, "GREEDY")

  GPSR_100   += "}; \\addlegendentry{GPSR 100}\n"
  GOAFR_100  += "}; \\addlegendentry{GOAFR 100}\n"
  GREEDY_100 += "}; \\addlegendentry{GREEDY 100}\n"

  GPSR_250   += "}; \\addlegendentry{GPSR 250}\n"
  GOAFR_250  += "}; \\addlegendentry{GOAFR 250}\n"
  GREEDY_250 += "}; \\addlegendentry{GREEDY 250}\n"

  axis = "axis"

  xticks = ""
  xtick_vals = ""
  max_val = 101
  for num in xrange(max_val):
    if num > 0 and (num % 1 == 0): 
      xticks += "%s" % num
      if (num % 10 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != max_val - 1:
        xticks     += ", "
        xtick_vals += ", "

  yticks      = ""
  ytick_vals  = ""
  max_val = 101
  for num in xrange(max_val):
    yticks     += "%s" % num
    if (num % 10 == 0):
      ytick_vals += "%s" % num
    if num != max_val - 1:
      yticks     += ", "
      ytick_vals += ", "

  y_left  = "\% of successfully transmitted messages"
  width = "0.8\linewidth"
  
  graph = "\\begin{tikzpicture}\n"
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}}\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, ytick={%s}, yticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s, legend cell align=left]\n" % (axis, xticks, xtick_vals, yticks, ytick_vals, width, y_left)
  graph += GPSR_100
  graph += GOAFR_100
  graph += GREEDY_100
  graph += GPSR_250
  graph += GOAFR_250
  graph += GREEDY_250
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "percentage_graph", graph)

def find_hop_points(size, path):
  points = ""
  for i in xrange(10):  
    nodes = (i + 1) * 10
    filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
   
    f = open(filename, 'r')
    json_data = f.read()
    f.close()
    data = json.loads(json_data)
    avg = data[1][0]

    points += "\t(%s, %s)\n" % (nodes, avg)

  return points

def make_hop_graph():
  
  default1 = "\\addplot[color=%s, mark=%s] coordinates{                \n" 
  default2 = "\\addplot[color=%s, mark=%s, densely dashed] coordinates{\n" 
  
  GPSR_100   = default1 % ("blue",  "*")
  GOAFR_100  = default1 % ("black", "triangle*")
  GREEDY_100 = default1 % ("red",   "square*")

  GPSR_250   = default2 % ("blue", "o")
  GOAFR_250  = default2 % ("black", "triangle")
  GREEDY_250 = default2 % ("red",   "square")

  GPSR_100   += find_hop_points(100, "GPSR")
  GOAFR_100  += find_hop_points(100, "GOAFR")
  GREEDY_100 += find_hop_points(100, "GREEDY")

  GPSR_250   += find_hop_points(250, "GPSR")
  GOAFR_250  += find_hop_points(250, "GOAFR")
  GREEDY_250 += find_hop_points(250, "GREEDY")

  GPSR_100   += "}; \\addlegendentry{GPSR 100}\n"
  GOAFR_100  += "}; \\addlegendentry{GOAFR 100}\n"
  GREEDY_100 += "}; \\addlegendentry{GREEDY 100}\n"

  GPSR_250   += "}; \\addlegendentry{GPSR 250}\n"
  GOAFR_250  += "}; \\addlegendentry{GOAFR 250}\n"
  GREEDY_250 += "}; \\addlegendentry{GREEDY 250}\n"

  axis = "axis"

  xticks = ""
  xtick_vals = ""
  max_val = 101
  for num in xrange(max_val):
    if num > 0 and (num % 1 == 0): 
      xticks += "%s" % num
      if (num % 10 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != max_val - 1:
        xticks     += ", "
        xtick_vals += ", "

  y_left  = "Average number of hops"
  width = "0.8\linewidth"
  
  graph = "\\begin{tikzpicture}\n"
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}}\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s, legend cell align=left]\n" % (axis, xticks, xtick_vals, width, y_left)
  graph += GPSR_100
  graph += GOAFR_100
  graph += GREEDY_100
  graph += GPSR_250
  graph += GOAFR_250
  graph += GREEDY_250
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "hop_graph", graph)

def find_time_points(size, path):
  points = ""
  for i in xrange(10):  
    nodes = (i + 1) * 10
    filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
   
    f = open(filename, 'r')
    json_data = f.read()
    f.close()
    data = json.loads(json_data)
    percent = data[2][0]

    points += "\t(%s, %s)\n" % (nodes, percent)

  return points

def make_time_graph():
  
  default1 = "\\addplot[color=%s, mark=%s] coordinates{                \n" 
  default2 = "\\addplot[color=%s, mark=%s, densely dashed] coordinates{\n" 
  
  GPSR_100   = default1 % ("blue",  "*")
  GOAFR_100  = default1 % ("black", "triangle*")
  GREEDY_100 = default1 % ("red",   "square*")

  GPSR_250   = default2 % ("blue", "o")
  GOAFR_250  = default2 % ("black", "triangle")
  GREEDY_250 = default2 % ("red",   "square")

  GPSR_100   += find_time_points(100, "GPSR")
  GOAFR_100  += find_time_points(100, "GOAFR")
  GREEDY_100 += find_time_points(100, "GREEDY")

  GPSR_250   += find_time_points(250, "GPSR")
  GOAFR_250  += find_time_points(250, "GOAFR")
  GREEDY_250 += find_time_points(250, "GREEDY")

  GPSR_100   += "}; \\addlegendentry{GPSR 100}\n"
  GOAFR_100  += "}; \\addlegendentry{GOAFR 100}\n"
  GREEDY_100 += "}; \\addlegendentry{GREEDY 100}\n"

  GPSR_250   += "}; \\addlegendentry{GPSR 250}\n"
  GOAFR_250  += "}; \\addlegendentry{GOAFR 250}\n"
  GREEDY_250 += "}; \\addlegendentry{GREEDY 250}\n"

  axis = "axis"

  xticks = ""
  xtick_vals = ""
  max_val = 101
  for num in xrange(max_val):
    if num > 0 and (num % 1 == 0): 
      xticks += "%s" % num
      if (num % 10 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != max_val - 1:
        xticks     += ", "
        xtick_vals += ", "

  y_left  = "Average time required to reach the destination"
  width = "0.8\linewidth"
  
  graph = "\\begin{tikzpicture}\n"
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}}\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s, legend cell align=left]\n" % (axis, xticks, xtick_vals, width, y_left)
  graph += GPSR_100
  graph += GOAFR_100
  graph += GREEDY_100
  graph += GPSR_250
  graph += GOAFR_250
  graph += GREEDY_250
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "time_graph", graph)

make_percentage_graph()
make_hop_graph()
make_time_graph()

