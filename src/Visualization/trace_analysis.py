-*- coding: utf-8 -*-

import svgfig
from svgfig import *
import re, io, visualize_graph

def find_packet_routes(file_location, filename):
  trace_file = open(file_location + filename, 'r')
  lines = trace_file.readlines()
  
  trace_file.close()
  pattern = re.compile("-Is ([-+]?\d+).\d+ -Id ([-+]\d+).\d+")
  unique_id = re.compile("-Ii (\d+)")
  coordinates = re.compile("-Ni (\d+) -Nx ([\d.]+) -Ny ([\d.]+)")
  
  trace_dict = {}
  svg_location = {}

  for line in lines:
    match = pattern.search(line)
    if match:
      unique_id_val = unique_id.search(line)
      coor = coordinates.search(line)
      id_val = (match.group(1), match.group(2), unique_id_val.group(1))
      cor = "ID: " + coor.group(1) + ", x: " + coor.group(2) + ", y: " + coor.group(3) + ": "
      

      line = cor + line
      svg_coor = (float(coor.group(2)), float(coor.group(3)))
      if not trace_dict.get(id_val):
        trace_dict[id_val] = [line]
        svg_location[id_val] = [svg_coor] 
      else:
        trace_dict[id_val].append(line)
        svg_location[id_val].append(svg_coor)

  analysis = ""
  for key in trace_dict.keys():
    traces = trace_dict[key]
    analysis += "".join(traces)
    analysis += '****************************\n'

  file_to_write = open("test_text/" + filename[0:-2] + 'ana', 'w')
  file_to_write.write(analysis)
  file_to_write.close()
 
  index = 0
  for key in svg_location.keys():
    index += 1
    loc_list = svg_location[key]
    named_points = visualize_graph.give_points_names(loc_list)
    
    graph = {}
    for name_index in range(1, len(named_points)):
      name_1 = named_points[name_index - 1][1]
      name_2 = named_points[name_index][1]
      
      if not graph.get(name_1):
        graph[name_1] = {name_2: 1}
      else: 
        graph[name_1][name_2] = 1

#    visualize_graph.simple_print_graph(filename[0:2] + str(index), named_points, graph)
 
  write_svg(svg_location, filename[0:-2])
  
  
def write_svg(all_locs_dict, name):
  # we make the edges
  name_index = 0
  for key in all_locs_dict.keys():
    loc_list = all_locs_dict[key]
    edge_dict = {}

    name_index += 1

    # We find the minimum values
    min_x = sys.float_info.max
    min_y = sys.float_info.max

    max_x = sys.float_info.min
    max_y = sys.float_info.min
    

    for entry in loc_list:

      (x, y) = entry
   
      min_x = c_round(min(min_x, x))
      max_x = c_round(max(max_x, x))
        
      min_y = c_round(min(min_y, y))
      max_y = c_round(max(max_y, y))

    for location_index in range(1, len(loc_list)):
      fst_loc = loc_list[location_index -1]
      snd_loc = loc_list[location_index]
    
      edge = (fst_loc, snd_loc)
      if not edge_dict.get(edge):
        edge_dict[edge] = 1
      else:
        edge_dict[edge] += 1

    fig = Fig()
    
    for p in loc_list:
      x = p[0]
      y = p[1]

      c = SVG('circle', cx = x, cy = y, r = 0.1, fill='black')
      fig.d.append(c)

    for edge in edge_dict:
      p1 = edge[0]
      p2 = edge[1]
        
      start_x = float(p1[0])
      end_x   = float(p2[0])
        
      start_y = float(p1[1])
      end_y   = float(p2[1])

      l = Line(start_x, start_y, end_x, end_y, stroke='black', width = edge_dict[edge])
      fig.d.append(l)
    
    svgfig._canvas_defaults['width'] = "1600px"
    svgfig._canvas_defaults['height'] = "1600px"
    
    (min_x, max_x) = margin(min_x, max_x)
    (min_y, max_y) = margin(min_y, max_y)

    svgfig._canvas_defaults['viewBox'] = str(min_x) + " " + str(min_y) + " " + str(max_x) + " " + str(max_y)
    
    s = SVG("top_level")
    s.append(fig.SVG())
    s.save("test_svg/" + name + str(name_index) + ".svg")

def margin(min_val, max_val):
  diff = abs(max_val - min_val)
  max_val += diff/2
  min_val -= diff/2
  return (min_val, max_val)

def c_round(number):
  str_num = str(number)
  split_num = str_num.split('.')
  if len(split_num) == 1:
    return number
  deci = split_num[1]
  split_num[1] = deci[0:min(4, len(deci))]
  return float(split_num[0] + '.' + split_num[1])

file_location = "test_results/"
filename = "goafr_test_00.tr"

find_packet_routes(file_location, filename)
