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
algos = ["DSDV", "GPSR", "GREEDY", "GOAFR"]

def save_file(file_name, data):
  file_name += '.tex'
  with open(file_name, mode='w') as f:
    f.write(data)     

def c_round(num):
  return "%.2f" % num

def make_percent_table():
  result_dict = {}

  for algo in algos:
    result_dict[algo] = {}
    for size in [500, 750]:
      result_dict[algo][size] = {}
      for nn in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
              
        filename = "%s_results/%s-%s-%s.json" % (algo, algo, nn, size)
        
        f = open(filename, 'r')
        json_data = f.read()
        f.close()
        data = json.loads(json_data)
        
        result_dict[algo][size][nn] = c_round(float(data[0]))
                
  newline = "\\\\\n"
  table  = "\\begin{tabular}{lrcccc}\n"
  table += "Size & Num nodes & DSDV & Greedy & GPSR & GOAFR " + newline
  
  table_dict = {}
  for size in [500, 750]:
    accum = ""
    for nn in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
      first_label = ""
      if nn == 10:
        first_label = "\\multirow{10}{*}{%s}" % size

      DSDV   = result_dict["DSDV"][size][nn]
      Greedy = result_dict["GREEDY"][size][nn]
      GPSR   = result_dict["GPSR"][size][nn]
      GOAFR  = result_dict["GOAFR"][size][nn]

      accum += " %s & %s & %s \%% & %s \%% & %s \%% & %s \%% %s" % (first_label, nn, DSDV, Greedy, GPSR, GOAFR, newline)
    table_dict[size] = accum 
  
  table += table_dict[500]
  table += "\\hline\n"
  table += table_dict[750]
  table += "\\end{tabular}"

  savename = "%spercentage_table" % (latex_location)
  save_file(savename, table)

def make_comm_table():
  result_dict = {}

  for algo in algos:
    result_dict[algo] = {}
    for size in [500, 750]:
      result_dict[algo][size] = {}
      for nn in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
              
        filename = "%s_results/%s-%s-%s.json" % (algo, algo, nn, size)
        
        f = open(filename, 'r')
        json_data = f.read()
        f.close()
        data = json.loads(json_data)
        
        result_dict[algo][size][nn] = data[3]
                
  newline = "\\\\\n"
  table  = "\\begin{tabular}{lrr rr r rr r rr r rr}\n"
  table += "Size & Num nodes & \\multicolumn{2}{c}{DSDV}&  & \\multicolumn{2}{c}{Greedy} & & \\multicolumn{2}{c}{GPSR} & & \\multicolumn{2}{c}{GOAFR} " + newline
  table += "     &           & Recv      & Sends        &  &   Recv  & Sends             & &    Recv      & Sends      & & Recv   & Sends " + newline
  
  table_dict = {}
  for size in [500, 750]:
    accum = ""
    for nn in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
      first_label = ""
      if nn == 10:
        first_label = "\\multirow{10}{*}{%s}" % size

      DSDV   = result_dict["DSDV"][size][nn]
      Greedy = result_dict["GREEDY"][size][nn]
      GPSR   = result_dict["GPSR"][size][nn]
      GOAFR  = result_dict["GOAFR"][size][nn]

      accum += " %s & %s & %s & %s & & %s & %s && %s & %s && %s & %s %s" % (first_label, nn, DSDV[0], DSDV[1], Greedy[0], Greedy[1], GPSR[0], GPSR[1], GOAFR[0], GOAFR[1], newline)
    table_dict[size] = accum 
  
  table += table_dict[500]
  table += "\\hline\n"
  table += table_dict[750]
  table += "\\end{tabular}"

  savename = "%scomm_table" % (latex_location)
  save_file(savename, table)

def make_table(type_name, size, path, algo, index):
  newline = "\\\\\n"
  table  = "\\begin{tabular}{cllll}\n"
  table += "Num nodes: & Avg. %s & Avg. Max %s & Avg. Min %s & Std. deviation %s" %(type_name, type_name, type_name, newline)
  for nn in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
    filename = "%s_results/%s-%s-%s.json" % (algo, algo, nn, size)
        
    f = open(filename, 'r')
    json_data = f.read()
    f.close()

    data = json.loads(json_data)
    rel_data = data[index]
    avg      = c_round(rel_data[0])
    max_val  = c_round(rel_data[2])
    min_val  = c_round(rel_data[1])
    std_devi = c_round(rel_data[3])
    
    table += "%s       & %s        & %s            & %s & %s %s" % (nn, avg, max_val, min_val, std_devi, newline)
  
  table += "\\end{tabular}"

  savename = "%s%s-%s_table_%s" % (path, type_name, algo, size)
  save_file(savename, table)

def find_value_points(size, path, indicies):
  points = ""
  for i in xrange(10):  
    nodes = (i + 1) * 10
    filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
    
    f = open(filename, 'r')
    json_data = f.read()
    f.close()

    data = json.loads(json_data)
    if len(indicies) == 1:
      value = data[indicies[0]]
    else:
      value = data[indicies[0]][indicies[1]]

    points += "\t(%s, %s)\n" % (nodes, value)

  return points

def make_graph_points(default, num, algo, indicies):
  line_str  = default
  line_str += find_value_points(num, algo, indicies)
  line_str += "}; \\addlegendentry{%s %s}\n" % (num, algo)
  return line_str

def make_graph(name, y_left, size, indicies):
  default_1 = "\\addplot[color=%s, mark=%s] coordinates{\n"
 
  GPSR   = make_graph_points(default_1 % ("blue",  "*"),         size, "GPSR", indicies)
  GOAFR  = make_graph_points(default_1 % ("black", "triangle*"), size, "GOAFR", indicies)
  GREEDY = make_graph_points(default_1 % ("red",   "square*"),   size, "GREEDY", indicies)
  DSDV   = make_graph_points(default_1 % ("green", "diamond"),   size, "DSDV"  , indicies)

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

  width = "0.8\linewidth"
  
  graph = "\\begin{tikzpicture}\n"
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}}\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s, legend cell align=left]\n" % (axis, xticks, xtick_vals, width, y_left)
  graph += GPSR
  graph += GOAFR
  graph += GREEDY
  graph += DSDV
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "%s_graph_%s" % (name, size), graph)

def make_percentage_graph():
  name = "percentage"
  label = "\% of successfully transmitted messages"
  make_percent_table()
  for size in [500, 750]:
    make_graph(name, label, size, [0])
  

def make_hop_graph():
  name = "hop"
  label = "average number of hops for arrived messages"
  for size in [500, 750]:
    make_graph(name, label, size, [1, 0])
    for algo in algos:
      make_table(name, size, latex_location, algo, 1)

def make_time_graph():
  name = "time"
  label = "average time for arrived messages (s)"
  for size in [500, 750]:
    make_graph(name, label, size, [2, 0])
    for algo in algos:
      make_table(name, size, latex_location, algo, 2)

make_percentage_graph()
make_hop_graph()
make_time_graph()
make_comm_table()
