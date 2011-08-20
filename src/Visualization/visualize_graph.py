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

def c_round(num):
  return "%.2f" % num

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
  s += '}{\n\t\\node[invis]  (\\name) at \\pos {};\n' 
  s += '\t\\node[vertex] () at \\pos {};\n'
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
  begin += '\\subfloat[The normal graph]{\label{fig:norm_graph}\n' + str_graph +'}\n'
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

def simple_print_graph(name, point_list, graph): 
  neighbour_dict = {}

  min_x = sys.float_info.max
  min_y = sys.float_info.max

  max_x = sys.float_info.min
  max_y = sys.float_info.min
    
  point_index_outer = 0
  point_index_inner = 0

  # make directory to turn point into name
  point_to_name = {}
  only_points = []
  for entry in point_list:
    point_to_name[entry[1]] = entry[0]
    only_points.append(entry[1])
     
  start = open('graph-basis.tex', 'r')
  begin = start.read()
  start.close()

  edge_list = make_edge_list(graph)

  str_graph = tikz_graph(graph, edge_list, point_to_name)

  begin += str_graph + '\n'

  save = open('tex/' + name + '.tex', 'w')
  save.write(begin)
  save.flush()
  save.close()

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

def graph_examples():
  point_list = []
  random.seed()
  num_points = 75
  max_size = 75
  
  point_dict = {}

  for index in range(0, num_points):
    new_point = False
   
    while not new_point:
      x = random.randint(0, max_size)
      y = random.randint(0, max_size)
      point = (x, y)
      if not point_dict.get(point):
        point_dict[point] = 1
        point_list.append(point)
        new_point = True
  
  point_list = give_points_names(point_list)
  make_and_print_graphs(point_list, 'example', 20)

def gateway_graphs():
  point_list = [0.75]

def motion_points(point_num, max_values):
  bonn_dir = "../../bonnmotion-1.5a/bin/"
  os.chdir(bonn_dir)

  max_points = 15

  name = "GaussMarkov-Movement_test_nodes-%s" % (point_num)
#  os.system("./bm -f %s GaussMarkov -i 60 -n %s -x %s -y %s -z 0 -d 300 -q 10" % (name, point_num, max_values, max_values))
 # os.system("gzip -df %s.movements.gz" % name)
  bonn_file = open(name + ".movements", "r")
  
  os.chdir("../../src/Visualization")
    
  point_mvt = []
  all_points = []
  coor_pat = re.compile("\d+.\d+ (\d+.\d+) (\d+.\d+)")

  for i in xrange(10):
    line = bonn_file.readline()
    coors = coor_pat.findall(line)
    
    num_coors = []
    for coor in coors:
      num_coors.append((float(coor[0]), float(coor[1])))
    
    max_coor_length = min(len(num_coors), max_points)
    num_coors = num_coors[0:max_coor_length]
    local_mvt = []
    
    for coor in num_coors:
      local_mvt.append(coor)
      all_points.append(coor)

    point_mvt.append(local_mvt)
  
  bonn_file.close()

  all_points = give_points_names(all_points)
  
  edge_list = []
  neighbour_dict = {}

  # make directory to turn point into name
  point_to_name = {}
  only_points = []
  for entry in all_points:
    point_to_name[entry[1]] = entry[0]
    only_points.append(entry[1])

  # Make the background edges
  pr = 100 / point_num

  background_edges = ""
  for i in xrange(point_num):
    dark = pr * (i + 1)
    background_edges += "\\tikzstyle{back_edge_%s} = [draw, line width=5pt,-,black!%s]\n" % (dark, dark)
  
  # We create the variables we are going to use to keep it orderly
  def_str = ""
  for index in xrange(point_num):
    mvt = point_mvt[index]
    
    temp = "\\def\\edgeList%s{" % (chr(index + 97))
    
    name_list = ""
    for inner_index in xrange(len(mvt) - 1):
      p1 = mvt[inner_index]
      p2 = mvt[inner_index + 1]
      
      n1 = point_to_name[p1]
      n2 = point_to_name[p2]

<<<<<<< HEAD
  # Make the background edges
  pr = 100.0 / point_num
  for i in xrange(point_num):
    dark = pr * (i + 1)
 
\tikzstyle{edgeBackground} = [draw, line width=2cm,-,gray!20] 
    edge


  edge_list = make_edge_list(graph)
  gauss_movement = tikz_graph(graph, edge_list, point_to_name)
=======
      name_list += "%s/%s, " % (n1, n2)
      
    def_str += "%s%s}\n" % (temp, name_list[0:-2])
 
  # We create the list of points
  point_list_str = ""
  for point in only_points:
    point_list_str += "{(%s, %s)/%s}, " % (c_round(point[0]), c_round(point[1]), point_to_name[point])
    
  last_bit  = '\\foreach \\pos/\\name in {%s} {\n' % point_list_str[0:-2] 
  last_bit += '\t\\node[invis]  (\\name) at \\pos {};\n' 
  last_bit += '\t\\node[vertex] () at \\pos {};\n}\n\n'
>>>>>>> c2969a8fb9fed91a4de22210c3c268e49693ebc3
  
  for index in xrange(point_num):
    name   = "\\edgeList%s" % (chr(index + 97)) 
    local  = '\\foreach \\source/\\sink in %s {\n' % name
    local += '\t\\path[edge] (\\source) -- (\\sink);\n}\n\n'

    last_bit += local

  last_bit += '\\begin{pgfonlayer}{background}\n'
 
  semi_local = ""
  for index in xrange(point_num):
    name   = "\\edgeList%s" % (chr(index + 97)) 
    local  = '\t\\foreach \\source/\\sink in %s {\n' % name
    local += '\t\t\\path[back_edge_%s] (\\source) -- (\\sink);\n' % (pr * (index + 1))
    local += '\t}\n'

    semi_local = local + semi_local

  last_bit += semi_local
  last_bit += '\\end{pgfonlayer}\n\n'
  
  graph  = "\\pgfdeclarelayer{last}\n"
  graph += "\\pgfdeclarelayer{background}\n"
  graph += "\\pgfsetlayers{last,background,main}\n\n"

  start = open('graph-basis.tex', 'r')
  graph += start.read()
  start.close()

  graph += background_edges + "\n"
  graph += def_str + "\n"
  graph += '\\begin{tikzpicture}[scale=\\scale]\n'
  graph += last_bit
  graph += "\\end{tikzpicture}"
  
  save = open('test_results/test.tex', 'w')
  save.write(graph)
  save.flush()
  save.close()

motion_points(10, 100)  



"""
make_graph_from_list('{(0,2)/a}, {(2,1)/b}, {(-2,1)/e}, {(1,-1)/c}, {(-1,-1)/d}, {(0,3)/f}, {(3,1.5)/g}, {(-3, 1.5)/j}, {(2,-2)/h}, {(-2,-2)/i}', 'peterson', 4)
"""

"""
points = load_pickle_file('/home/mikkel/Documents/MasterThesis/src/spanner/Pointsets/pointset_1')
point_list = give_points_names(points)
make_and_print_graphs(point_list, 'check', 20)
"""

"""
cutoff_distance = 20
read_dir(dir_list, 'test', cutoff_distance)
"""

"""
graph_examples()
"""

