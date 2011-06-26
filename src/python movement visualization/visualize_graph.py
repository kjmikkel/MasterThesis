import svgfig
from svgfig import *
import os, sys, math, colorsys, webcolors, copy, re, pickle

cmd_folder = os.path.dirname(os.path.abspath(__file__)) + '/../graph_support'
if cmd_folder not in sys.path:
  sys.path.insert(0, cmd_folder)

import make_graph

dir_list = ["test_movement"]
time_point = 1
modifier = 1
colour_list = webcolors.css3_names_to_hex.keys()

# SVG creation
def svg_graph(name, graph, edge_list, max_x, max_y, min_x, min_y):
    fig = Fig()
    
    for p in graph:
        x = p[0]
        y = p[1]

        c = SVG('circle', cx = x, cy = y, r = 2, fill='black')
        fig.d.append(c)

    for edge in edge_list:
        p1 = edge[0]
        p2 = edge[1]
        
        start_x = p1[0]
        end_x   = p2[0]

        start_y = p1[1]
        end_y   = p2[1]

        l = Line(start_x, start_y, end_x, end_y, stroke='black')
        fig.d.append(l)
    
    svgfig._canvas_defaults['width'] = "1600px"
    svgfig._canvas_defaults['height'] = "1600px"
    svgfig._canvas_defaults['viewBox'] = str(min_x) + " " + str(min_y) + " " + str(max_x) + " " + str(max_y)
    
    s = SVG("top_level")  
    s.append(fig.SVG())
    s.save(name + ".svg")

def tikz_graph(graph, edge_list, point_to_name):
  s = '\\begin{tikzpicture}[scale=\\scale]\n'
  s += '\\foreach \\pos/\\name in {'
  first = True
  for point in graph:
    if not first:
      s += ', '
    first = False
    x = str(point[0] / modifier)
    y = str(point[1] / modifier)
    name = point_to_name[point]
    s += '{(' + x + ', ' + y + ')' + '/' + name + '}'
  s += '}{\n\t\\node[vertex] (\\name) at \\pos {};\n'
# s += '\t\draw[outline] {\\pos circle (' + str(cutoff_distance / modifier) +  ')} node {};\n
  s += '}' 
    
  # Now to take care of the edges
  
  first = True
  s += '\n\\foreach \\source/\\dest in {'
  for edge in edge_list: 
    if not first:
      s += ', '
    first = False
    
    p1_name = point_to_name[edge[0]]
    p2_name = point_to_name[edge[1]]
    s += p1_name + '/' + p2_name  
  
  s += '} {\n '
  s += '\path[edge] (\\source) -- (\\dest);\n}'
  s += '\n\\end{tikzpicture}'
  return s

def read_dir(dir_list, name, cutoff_distance):
  for fdir in dir_list:
    point_list = []

    letter = ord('a')
    subfix = 1
    for filename in os.listdir(fdir):
        
      f = open(fdir + '/' + filename)
      lines = f.readlines()
      f.close()

      split = lines[time_point].split(' ')
      x = float(split[0])
      y = float(split[1])

      point_list.append((x, y))
    
    point_list = give_points_names(point_list)
    make_and_print_graphs(point_list, name + ' ' + filename, cutoff_distance)

def give_points_names(point_list):
  letter = ord('a')
  subfix = 1
  
  return_list = []

  for point in point_list:
    if subfix < 2:
      return_list.append((chr(letter), point))
    else:
      return_list.append((chr(letter) + str(subfix), point))

    letter += 1
    if letter > ord('z'):
      letter = ord('a')
      subfix += 1

  return return_list

def make_edge_list(graph):
  edge_list = []
  already_made = {}

  for outer_point in graph:
    edges = graph[outer_point]
    
    for inner_point in edges:
      if already_made.get((inner_point, outer_point)) or outer_point == inner_point:
        continue
      
      already_made[(inner_point, outer_point)] = 1
      already_made[(outer_point, inner_point)] = 1
   
      edge_list.append((outer_point, inner_point))
  
  return edge_list    
      
def make_and_print_graphs(point_list, name, cutoff_distance): 
  edge_list = []
  neighbour_dict = {}

  min_x = sys.float_info.max
  min_y = sys.float_info.max

  max_x = sys.float_info.min
  max_y = sys.float_info.min
    
  for entry in point_list:
    
    (x, y) = entry[1]
   
    min_x = min(min_x, x)
    max_x = max(max_x, x)
        
    min_y = min(min_y, y)
    max_y = max(max_y, y)

  point_index_outer = 0
  point_index_inner = 0

  # make directory to turn point into name
  point_to_name = {}
  only_points = []
  for entry in point_list:
    point_to_name[entry[1]] = entry[0]
    only_points.append(entry[1])
     
  (graph, tree) = make_graph.SciPy_KDTree(only_points, cutoff_distance)
    
  gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(graph), tree)
    
  rn_graph = make_graph.rn_graph(copy.deepcopy(graph))
    
  mst = make_graph.MST_Kruskal(copy.deepcopy(graph))

  start = open('graph-basis.tex', 'r')
  begin = start.read()
  start.close()

  graph_edge_list = make_edge_list(graph)
  gg_edge_list = make_edge_list(gabriel_graph)
  rng_edge_list = make_edge_list(rn_graph)
  mst_edge_list = make_edge_list(mst)
    
  str_graph = tikz_graph(graph, graph_edge_list, point_to_name)
  str_gg_graph = tikz_graph(gabriel_graph, gg_edge_list, point_to_name)
  str_rng_graph = tikz_graph(rn_graph, rng_edge_list, point_to_name)
  str_mst = tikz_graph(mst, mst_edge_list, point_to_name)    

  begin += '\n\n'
  begin += '\\subfloat[The nomral graph]{\label{fig:norm_graph}\n' + str_graph +'}\n'
  begin += '\\subfloat[The Gabriel graph]{\label{fig:gg_graph}\n' + str_gg_graph + '}\n\n'
  begin += '\\subfloat[The RNG graph]{\label{fig:rng_graph}\n' + str_rng_graph + '}'
  begin += '\\subfloat[The MST]{\label{fig:mst}\n' + str_mst + '}'

  save = open('tex/test.tex', 'w')
  save.write(begin)
  save.flush()
  save.close()

  normal_graph_name = 'svg/Normal-' + name
  svg_graph(normal_graph_name, graph, graph_edge_list, max_x, max_y, min_x, min_y)
   
  gabriel_graph_name = 'svg/Gabriel graph-' + name
  svg_graph(gabriel_graph_name, gabriel_graph, gg_edge_list, max_x, max_y, min_x, min_y)
   
  rn_graph_name = 'svg/RNG graph-' + name
  svg_graph(rn_graph_name, rn_graph, rng_edge_list, max_x, max_y, min_x, min_y)

  mst_name = 'svg/MST-' + name
  svg_graph(mst_name, mst, mst_edge_list, max_x, max_y, min_x, min_y) 


def make_graph_from_list(str_list, name, cutoff_distance):
  point_list = []
  entries = str_list.split('}')
  number = '([-+\s]*[\d.]+)'
  find = re.compile(',?\s*{\(' + number + ',' + number + '\)/(\w+)' )
  for entry in entries:
    if len(entry) > 0:
      result = find.match(entry)
      point_list.append((result.group(3), (float(result.group(1)), float(result.group(2)))))
  
  make_and_print_graphs(point_list, name, cutoff_distance)  

def load_pickle_file(file_name):
  result = None
  file_name += '.pickle'
  with open(file_name, mode='rb') as f:
    result = pickle.load(f)
  return result

#make_graph_from_list('{(0,2)/a}, {(2,1)/b}, {(-2,1)/e}, {(1,-1)/c}, {(-1,-1)/d}, {(0,3)/f}, {(3,1.5)/g}, {(-3, 1.5)/j}, {(2,-2)/h}, {(-2,-2)/i}', 'peterson', 4)

points = load_pickle_file('/home/mikkel/Documents/MasterThesis/src/spanner/Pointsets/pointset_1')

point_list = give_points_names(points)
#print point_list
make_and_print_graphs(point_list, 'check', 20)

#cutoff_distance = 20
#read_dir(dir_list, 'test', cutoff_distance)

