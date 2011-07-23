import os, sys, random, copy, math, json, pickle

path = os.path.dirname(os.path.abspath(__file__))

folders =[path + '/../graph_support', path + '/../python movement visualization']
for cmd_folder in folders:
  if cmd_folder not in sys.path:
    sys.path.insert(0, cmd_folder)

import make_graph, dijkstra, visualize_graph

source_sink = [(0.75, 3, 's'), (5, 3, 't')]

saved_points_name = 'gateway/gateway_points'
saved_graphs_name = 'gateway/gateway_graph'
save_nodes_name = 'gateway/gateway_usefull_nodes'

def load_pickle_file(file_name):
  result = None
  file_name += '.pickle'
  with open(file_name, mode='rb') as f:
    result = pickle.load(f)
  return result

def save_pickle_file(file_name, data):
  file_name += '.pickle'
  with open(file_name, mode='wb') as f:
    pickle.dump(data, f)

def make_nodes(clasified_nodes, number_of_points, max_x_values, max_y_values):
  # We must ensure that all points are uniqe
  used_points = {}
  data = []

  for node in clasified_nodes:
      (x, y) = node[0:2]
      used_points[(x, y)] = 1
    	  	   
  for i in range(0, number_of_points):

    not_used = False
    while not not_used:
      x = (random.randint(0, max_x_values) * 1.0) / 10.0
      y = (random.randint(0, max_y_values) * 1.0) / 10.0
        
      if used_points.get((x, y)) == None:
        used_points[(x, y)] = 1
          
        not_used = True

        entry = (x, y)
        data.append(entry)
  
  save_pickle_file(saved_points_name, data)

def make_gateway_graph(clasified_nodes, cutoff):
  points = load_pickle_file(saved_points_name)
  
  for node in clasified_nodes:
    points.append(node[0:2])

  (graph, tree) = make_graph.SciPy_KDTree(points, cutoff)
  
  # The initial graph has been made, now we must remove all the nodes that are too close to each other
  """
  ignore = {}
  too_close = 0.3
  ignore_count = 0

  for point in points:
    # if we have removed this point before, then we just ignore it
    if ignore.get(point):
      ignore_count += 1
      continue
    
    # Now we must ensure the node is not too close to the other nodes
    close = tree.query_ball_point(point, too_close)

    for too_close_node in close:
#      ignore_node = points[too_close_node]
      ignore_node = 
      ignore[ignore_node] = 1

  # At this point we have found all the points we need to ignore, now we need to remove them from the data we are going to build the new graph from
  print ignore_count
  point_dict = dict([(p, 1) for p in points])
  
  print len(points), len(ignore.keys())

  for del_point in ignore.keys():
    del point_dict[del_point]

  points = point_dict.keys()
  print points

  (graph, tree) = make_graph.SciPy_KDTree(points, cutoff)
  """
  
  gabriel_graph = make_graph.gabriel_graph(graph, tree, points)

  save_pickle_file(saved_graphs_name, gabriel_graph)

def find_node_and_gateway(D, node, node_set, node_gateway, unit_cutoff):
  distance = D.get(node)
  if distance and distance <= unit_cutoff:
    if distance < unit_cutoff:
      node_set.append(node)
    else:
      node_gateway.append(node)


def find_usefull_points(clasified_nodes):
  graph = load_pickle_file(saved_graphs_name)
 
  node1 = clasified_nodes[0][0:2]
  node2 = clasified_nodes[1][0:2]

#  print node1, node2  

  (D1, P1) = dijkstra.UnitDijkstra(graph, node1)
  (D2, _) = dijkstra.UnitDijkstra(graph, node2)

  nodes = graph.keys()

  gateway_distance = D1[node2]
  gateway_node1_distance = gateway_distance / 2
  gateway_node2_distance = gateway_distance / 2  

  if gateway_distance % 2 == 1:
    gateway_node2_distance += 1 

  print 'Gateway distance: ' + str(gateway_node1_distance) + ', ' + str(gateway_node2_distance)
  
  # Nodes there are less than the cutoff away from one of the nodes
  node1_set = []
  node2_set = []
  
  # Nodes that are exactly the cutoff distance away.
  node1_gateways = []
  node2_gateways = []

  for node in nodes:
    if node in (node1, node2):
      continue

    find_node_and_gateway(D1, node, node1_set, node1_gateways, gateway_node1_distance)
    find_node_and_gateway(D2, node, node2_set, node2_gateways, gateway_node2_distance)

#  print node1_gateway, node2_gateway

  node1_gateway_node = None
  node2_gateway_node = None
  
  min_dist = 50

  for gateway1 in node1_gateways:
    (gateway_distance, _) = dijkstra.UnitDijkstra(graph, gateway1)

    break_out = False
    for gateway2 in node2_gateways:
      min_dist = min(min_dist, gateway_distance[gateway2])

      if gateway_distance[gateway2] <= 1:
        node1_gateway_node = gateway1
        node2_gateway_node = gateway2
        break_out = True
        break
    if break_out:
      break
  
  # We add the rest of the node to the node set
  node1_gateways.remove(node1_gateway_node)
  node2_gateways.remove(node2_gateway_node)

  for node in node1_gateways:
    node1_set.append(node)

  for node in node2_gateways:
    node2_set.append(node)

  data_to_save = (node1_set, node2_set, node1_gateway_node, node2_gateway_node)  
  save_pickle_file(save_nodes_name, data_to_save)

def list_points(avoid_point, work_points, name_dict):
  points_str = '{'
  for point in work_points:
    if point == avoid_point:
      continue
    points_str += '{' + print_point(point) + '/' + name_dict[point] + '}, '
  
  points_str = points_str[0:-2] + '}'
  return points_str  

def print_point(point):
  x_coor = str(point[0])
  y_coor = str(point[1])

  if len(x_coor) > 4:
    x_coor = x_coor[0:4]

  if len(y_coor) > 4:
    y_coor = y_coor[0:4]
 
  return '(' + x_coor + ', ' + y_coor + ')'

def make_latex(clasified_nodes, cutoff):
  (node1_set, node2_set, gateway_node1, gateway_node2) = load_pickle_file(save_nodes_name)

  source = clasified_nodes[0][0:2]
  sink   = clasified_nodes[1][0:2]

  # I make the list that will be used for the final graph
  points = []
  points.extend(node1_set)
  points.extend(node2_set)
  points.append(gateway_node1)
  points.append(gateway_node2)
  points.append(source)
  points.append(sink)

  (graph, tree) = make_graph.SciPy_KDTree(points, cutoff)  
  gabriel_graph = make_graph.gabriel_graph(graph, tree, points)

  # We name the nodes
  letter = ord('a')
  subfix = 1
  
  name_dict = {}

  for point in points:
    
    # s and t are reserved letters
    if chr(letter) == 's' and subfix == 1:
      letter += 2

    if subfix == 1:
      name_dict[point] = chr(letter)
    else:
      name_dict[point] = chr(letter) + str(subfix)

    letter += 1
    if letter > ord('z'):
      letter = ord('a')
      subfix += 1
  
  name_dict[source] = 's'
  name_dict[sink]   = 't'

  # now we have named the points

  ignore_dict = {}
  final_edge_dict = {}

  for outer_point in gabriel_graph:
    outer_name = name_dict[outer_point]
    local_edges = []
       
    for inner_point in gabriel_graph[outer_point]:
      
      inner_name = name_dict[inner_point]
      
      # If the edge is already in the list then we ignore it
      if ignore_dict.get((outer_name, inner_name)):
        continue
      
      local_edges.append(inner_name)

      # Belt and suspenders
      ignore_dict[(outer_name, inner_name)] = 1
      ignore_dict[(inner_name, outer_name)] = 1
    
    final_edge_dict[outer_name] = local_edges  
  
  # Now we make the list of points and their locations
  source_sink = '\\def\\clusterHeads{{' + print_point(source) + '/s}, {' + print_point(sink) + '/t}}'
  gateway     = '\\def\\gateway{{' + print_point(gateway_node1) + '/' + name_dict[gateway_node1] +'},' + '{' + print_point(gateway_node2) + '/' + name_dict[gateway_node2] + '}}'  

  source_nodes = '\\def\\nodesOne' + list_points([source], node1_set, name_dict)
  sink_nodes   = '\\def\\nodesTwo' + list_points([sink  ], node2_set, name_dict)
  
  print source_sink
  print gateway
  print source_nodes
  print sink_nodes
  
  # Now we make the connections of the graph
  graph_edges = '\\def\\connect{'
  for outer_name in final_edge_dict.keys():
    for inner_name in final_edge_dict[outer_name]:
      graph_edges += outer_name + '/' + inner_name + ', '
  
  graph_edges = graph_edges[0:-2] + '}'
  print graph_edges

  

def gateway_suite(number_points, max_x_value, max_y_value, cutoff, state):
  if state <= 0:
    make_nodes(source_sink, number_points, max_x_value, max_y_value)
    print 'Made points'

  if state <= 1:
    make_gateway_graph(source_sink, cutoff)
    print 'Made graphs'
  
  if state <= 2:
    find_usefull_points(source_sink)
    print 'found usefull points'

  if state <= 3:
    make_latex(source_sink, cutoff)

gateway_suite(200, 60, 60, 1.5, 1)
