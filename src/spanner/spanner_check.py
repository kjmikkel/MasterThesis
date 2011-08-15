import os, sys, random, copy, math, json, pickle, multiprocessing
from multiprocessing import Pool
from decimal import *
from datetime import datetime

# I define the precision of the decimal numbers
getcontext().prec = 5

cmd_folder = os.path.dirname(os.path.abspath(__file__)) + '/../graph_support'
if cmd_folder not in sys.path:
  sys.path.insert(0, cmd_folder)

import make_graph, dijkstra

point_set_location = 'Pointsets/'
point_filename = 'pointset_'

non_planar_placement = 'Non-planar/'
non_planar_filename = 'graph_'

gg_placement = 'Gabriel graphs/'
gg_filename = 'gg_graph_'

rng_placement = 'RNG graphs/'
rng_filename = 'rng_graph_'

non_planar_f = 'Graphs/'
gabriel_graph_f = 'Graphs/'  
rng_f = 'Graphs/' 

results_non_location = 'Results/' 
results_gg_location = 'Results/' 
results_rng_location = 'Results/' 

latex_location = '../../report/results/spanner/'
node_pair_location = "Node pairs/"

debug = True

""" 
Container class to have a bit more structured data transference from the results
"""
class results_container:

  def __init__(self, name):
    self.name = name
    self.distance = []
    self.distance_value = 0
    self.average_distance = 0

    self.unit_distance = []
    self.unit_distance_value = 0
    self.average_unit_distance = 0

    self.min_value = 0
    self.max_value = 0

    self.min_unit_value = 10000
    self.max_unit_value = -1

    self.num_errors = 0
    self.legal_empty = 0

    # Statistics about
    self.edge_number = 0
    self.average_edges = []
    self.average_neighbours = 0
    self.max_neighbours = 0
    self.min_neighbours = 0

    self.total_length = 0

    self.cc = [] # Connected components
    self.number_cc = 0

  def get_min_internal(self, min_dist):
    pr_test = len(min_dist) / 500
    min_list = []
    for i in range(500):
      min_list.append(min(min_dist[i * pr_test: (i + 1) * pr_test - 1]))
      
    min_sum = sum(min_list)
    min_result = (min_sum * 1.0) / (500 * 1.0)
    print min_result
    return min_result

  def get_max_internal(self, max_dist):
    pr_test = len(max_dist) / 500
    max_list = []
    for i in range(500):
      max_list.append(max(max_dist[i * pr_test: (i + 1) * pr_test - 1]))
      
    max_sum = sum(max_list)
    max_result = (max_sum * 1.0) / (500 * 1.0)
    print max_result
    return max_result

  def get_min(self):
    return self.get_min_internal(self.distance)

  def get_max(self):
    return self.get_max_internal(self.distance)

  def get_unit_min(self):
    return self.get_min_internal(self.unit_distance)

  def get_unit_max(self):
    return self.get_max_internal(self.unit_distance)

  def get_distance(self):
    return sum(self.distance)

  def get_unit_distance(self):
    return sum(self.unit_distance)

  def finalize(self):
    divide_value = len(self.distance) * 1.0

    # Distance
    self.distance_value = self.get_distance()
    self.unit_distance_value = self.get_unit_distance()

    self.average_distance = (self.distance_value * 1.0) / divide_value

    self.average_unit_distance = (self.unit_distance_value * 1.0) / divide_value

    self.min_value = self.get_min()
    self.max_value = self.get_max()

    self.min_unit_value = self.get_unit_min()
    self.max_unit_value = self.get_unit_max()

    self.distance = []
    self.unit_distance = []

    # Edges:
    self.edge_number = self.edge_number / 2
    self.average_neighbours = sum(self.average_edges) * 1.0 / len(self.average_edges) * 1.0
    self.average_edges = []
    # CC
    self.number_cc = (sum(self.cc) * 1.0) / len(self.cc) * 1.0 # Connected components
    self.cc = []

  def get_latex_values(self):
  #  self.finalize()

    distance = (c_round(self.distance_value), c_round(self.max_value), c_round(self.min_value), c_round(self.average_distance))
    unit_distance = (str(self.unit_distance_value), c_round(self.max_unit_value), c_round(self.min_unit_value), c_round(self.average_unit_distance))
    neighbours = (c_round(self.max_neighbours), c_round(self.min_neighbours), c_round(self.average_neighbours))
    
    return_dict = {}
    return_dict["distance"] = distance
    return_dict["unit"] = unit_distance
    return_dict["error"] = str(self.num_errors)
    return_dict["total_length"] = c_round(self.total_length)
    return_dict["num_edges"] = str(self.edge_number)
    return_dict["neighbours"] = neighbours
    return_dict["CC"] = str(self.number_cc)
 
    return return_dict

  def __str__(self):
    string =  'Name: ' + self.name + '\n'
    string += 'Total distance: ' + c_round(self.distance_value) + '\n'
    string += 'Average distance: ' + c_round(self.average_distance) + '\n' 
    string += 'Min: ' + c_round(self.min_value) + ', Max: ' + c_round(self.max_value) + '\n\n'
    
    string += 'Unit distance: ' + str(self.unit_distance_value) + '\n'
    string += 'Average Unit distance: ' + c_round(self.average_unit_distance) + '\n'
    string += 'Min: ' + str(self.min_unit_value) + ', Max: ' + str(self.max_unit_value) + '\n'
    string += 'Number of missing paths: ' + str(self.num_errors) + '\n\n'
    string += 'Number of edges: ' + str(self.edge_number / 2) + '\n'
    string += 'Average numbers of neighbours: ' + str(self.average_neighbours) + '\n'  
    string += 'Max Neighbours: ' + str(self.max_neighbours) + ', Min: ' + str(self.min_neighbours)
    
    return string

def ftd(num):
  return Decimal(str(num))

def c_round(num):
  return "%.2f" % num

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
  
def load_json_file(file_name):
  result = None
  file_name += '.json'
  with open(file_name, mode='r') as f:
    result = json.load(f)
  return result

def save_json_file(file_name, data):
  file_name += '.json'
  with open(file_name, mode='w') as f:
    json.dumps(data, f)

def generate_point_sets(number_of_points, num_pointset, max_values):
  random.seed()
  
  point_str = str(number_of_points) + '/'

  for num in range(0, num_pointset):
    # We must ensure that all points are uniqe
    used_points = {}
    data = []

    if num % 10 == 0 and num > 0:
      print 'Made ' + str(num) + ' pointsets' 
    	  	   
    for i in range(0, number_of_points):

      not_used = False
      while not not_used:
        x = random.randint(0, max_values)
	y = random.randint(0, max_values)
        
        if used_points.get((x, y)) == None:
          used_points[(x, y)] = 1
          
          not_used = True

          entry = (x, y)
          data.append(entry)
    
    filename = point_set_location + point_str + point_filename + str(num + 1)
    save_pickle_file(filename, data)

def generate_graphs(number_of_points, number_of_graphs, cutoff_distance):
  for graph_index in range(0, number_of_graphs):
    point_str = str(number_of_points) + '/'

    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' graphs' 
    
    index_str = str(graph_index + 1)
    
    filename = point_set_location + point_str + point_filename + index_str
    new_data = load_pickle_file(filename)

    tree = make_graph.SciPy_KDTree(new_data)
    
    normal_graph = make_graph.make_non_planar_graph(new_data, cutoff_distance, tree)
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    save_pickle_file(filename, normal_graph) 

    if number_of_points > 2500:
      rn_graph = make_graph.rn_graph_kdtree(new_data, tree, cutoff_distance)
    else:
      rn_graph = make_graph.rn_graph_brute(new_data, cutoff_distance)

    filename = rng_f + point_str + rng_placement + rng_filename + index_str
    save_pickle_file(filename, rn_graph)
 
    if number_of_points > 2500:
      g_graph = make_graph.gabriel_graph_kdtree(new_data, tree, cutoff_distance)
    else:
      g_graph = make_graph.gabriel_graph_brute(new_data, cutoff_distance)
  
    filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    save_pickle_file(filename, g_graph)
            
def make_node_pairs((number_of_points, num, number_tests, from_val, to_val)):
  point_str = str(number_of_points) + '/'

  for graph_index in range(from_val, to_val):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made node pairs for ' + str(graph_index) + ' tests.'

    # If the file already exist we just cut it down to the right size
    if os.path.exists(node_pair_location + point_str + "node_pair_" + str(graph_index + 1) + ".pickle"):
      save_filename = node_pair_location + point_str + "node_pair_" + str(graph_index + 1)
      data = load_pickle_file(save_filename)
      data = data[0:number_tests]
      save_pickle_file(save_filename, data)
      continue

    node_pairs_to_check = []
    
    index_str = str(graph_index + 1)
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
   
    non_planar_graph = load_pickle_file(filename)

    (_ , set_container) = make_graph.MST_Kruskal(non_planar_graph)
    nodes = non_planar_graph.keys()

    # We find out how many sets we are working with
    num_sets_dict = {}
    set_len = 0
    for node in nodes:
      tup = tuple(set_container[node])
      if not num_sets_dict.get(tup):
        num_sets_dict[tup] = 1
        set_len += 1
      
      if set_len >= 5:
        break

    node_pairs_to_check = []
    if set_len < 5:
      node_pairs_to_check = advanced_node_pairs(number_tests, set_container, nodes)
    else:
      node_pairs_to_check = brute_force_node_pairs(number_tests, set_container, nodes)

    save_pickle_file(node_pair_location + point_str + "node_pair_" + str(graph_index + 1), node_pairs_to_check)
    

def brute_force_node_pairs(number_tests, set_container, nodes):
    random.seed()
    node_pairs_to_check = []
    # We find all possible combinations of pairs inside the mst
    already_there = {}
    total_pair_list = []
    container = None

    for start_node in set_container.keys():
      container = set_container[start_node]
      for end_node in container:
        if (not already_there.get((end_node, start_node))) and start_node != end_node:
          total_pair_list.append((start_node, end_node))
          already_there[(end_node, start_node)] = 1
          already_there[(start_node, end_node)] = 1
     
    # we take all the pairs we can
    
    difference = abs(number_tests - len(total_pair_list))

    if number_tests >= len(total_pair_list):
      # If we have more or an equal number of tests to all the combinations, then all the combinations are our tests
      node_pairs_to_check = total_pair_list
    elif number_tests > difference:
      # If the difference between the two is less than the number of tests, then we just delete pairs from the total list untill we the two numbers match
      for i in range(0, difference):
        ran_index = random.randint(0, len(total_pair_list) - 1)
        del total_pair_list[ran_index]
      node_pairs_to_check = total_pair_list
    else:
      for i in range(0, number_tests):
        ran_index = random.randint(0, len(total_pair_list) - 1)
        node_pairs_to_check.append(total_pair_list[ran_index])
        del total_pair_list[ran_index]
   
    return node_pairs_to_check

def advanced_node_pairs(number_tests, set_container, nodes):
  random.seed()
  already_there = {}
  node_pairs_to_check = []

  for test_index in range(0, number_tests):
    # We must ensure the pair is not replicated
    new_pair = False
    num_failure = 1000

    while not new_pair:  
      # First we make sure we have from node that is not totally isolated in the non-planar graph
      """       
      WARNING: Under certian configurations this might make the program enter a infinite loop no (or not enough) nodes are connected to other nodes in the tree. 
        
      By our uniqueness demand we cannot ensure termination (we can specify a demand for a greater number of unique pairs than there are), but the current implemention could certianly be improved
      """
      social_node = False
      source_node = None
      source_node_connections = None
        
      sink_node = None

      while not social_node:
        source_index = random.randint(0, len(nodes) - 1)
        source_node = nodes[source_index]
        source_node_connections = set_container[source_node]
        if len(source_node_connections) > 1:
          social_node = True   

      # We have now found a from node that has at least one neighbour, and so we try to find a to_node
      while not sink_node:
        sink_index = random.randint(0, len(source_node_connections) - 1)
        temp_node = source_node_connections[sink_index]

        # We only add the sink node if it is different than the source node
        if temp_node != source_node:
          sink_node = temp_node
        
      can_pair = (source_node, sink_node)
      aux_pair = (sink_node, source_node)

      # If neither of the above are in the in the dictionary, then we add it 
      if not (already_there.get(can_pair) or already_there.get(aux_pair)):
        already_there[can_pair] = 1
        already_there[aux_pair] = 1
        node_pairs_to_check.append(can_pair)
        new_pair = True
        # reset failure
        num_failure = 1000
      else:
        #emergency handbrake 
        num_failure -= 1
        if num_failure <= 0:
          print '***   Failure    ***'
          break
  
  return node_pairs_to_check
 

def do_test((number_of_points, num, number_tests, from_val, to_val)):
  point_str = str(number_of_points) + '/'
  for graph_index in range(from_val, to_val):
    index_str = str(graph_index + 1)  

    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' tests.'

    node_pairs_to_check = load_pickle_file(node_pair_location + point_str + 'node_pair_' + str(graph_index + 1))

    # We perform the actual tests
    
    # Start Non-planar
 
    load_filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    save_filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    if not os.path.exists(save_filename + ".pickle"): 
      perform_tests(load_filename, save_filename, node_pairs_to_check)
    else:
      cutdown(save_filename, number_tests)
    # End non-planar

    # Start Gabriel Graph

    load_filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    save_filename = results_gg_location + point_str + gg_placement + gg_filename + index_str
    if not os.path.exists(save_filename + ".pickle"):
      perform_tests(load_filename, save_filename, node_pairs_to_check)
    else:
      cutdown(save_filename, number_tests)
    # End Gabriel Graph

    # Start RNG 
    load_filename = rng_f + point_str + rng_placement + rng_filename + index_str
    save_filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    if not os.path.exists(save_filename + ".pickle"):
      perform_tests(load_filename, save_filename, node_pairs_to_check)
    else:
      cutdown(save_filename, number_tests)
    # End RNG

def cutdown(save_filename, number_tests):
  data = load_pickle_file(save_filename)
  (results, neighbour_data, graph_distance, num_cc) = data
  results = results[0:number_tests]
  data = (results, neighbour_data, graph_distance, num_cc)
  save_pickle_file(save_filename, data)

def perform_tests(load_graph_name, save_data_name, node_pairs):
    graph = load_pickle_file(load_graph_name)
    local_node_pairs = copy.deepcopy(node_pairs)

    results                            = do_actual_test(graph, local_node_pairs) 
    (neighbour_data, graph_distance)   = get_edge_data(graph)   

    # We find all the Connected Components
    (_ , set_container) = make_graph.MST_Kruskal(graph)

    cc_dict = {}
    for node in set_container.keys():
      set_tuple = tuple(set_container[node])
      cc_dict[set_tuple] = 1
     
    num_cc = len(cc_dict.keys())  
    
    save_pickle_file(save_data_name, (results, neighbour_data, graph_distance, num_cc))

def get_edge_data(graph):
    nodes = graph.keys()
    total_length = 0
    
    neigh_list = []
    for outer_node in nodes:
      neighbours = graph[outer_node]
      neigh_list.append(len(neighbours))
      for inner_node in neighbours:
        total_length += neighbours[inner_node]

    total_length /= 2
    
    total_number = sum(neigh_list)
    neigh_max = max(neigh_list)
    neigh_min = min(neigh_list)
    neigh_avg = total_number / len(neigh_list)
    neigh_data = (total_number, neigh_max, neigh_min, neigh_avg)
    
    return (neigh_data, total_length)
        
def do_actual_test(graph, node_pairs):
  results = []
  for pair in node_pairs:
    (start_node, end_node) = pair 
    
    (D, _, Path)   = dijkstra.shortestPath(    graph, start_node, end_node)  
    (D_unit, _, _) = dijkstra.shortestUnitPath(graph, start_node, end_node)

    if len(Path) > 0:
      try:
        distance = D[end_node]
      except KeyError:
        print "Key error"
        print D, start_node, end_node, Path
      unit_distance = D_unit[end_node]
    else:
      distance = -1 
      unit_distance = -1
    
    result = (distance, len(Path) - 1, unit_distance)
    results.append(result)
  
  return results
  
def analyse_results(number_of_points, num_results, pr_graph_test):
  
  non_planar_container = results_container('Non-planar')
  gg_container         = results_container('Gabriel Graph')
  rng_container        = results_container('Relative Neighbourhood Graph')

  point_str = str(number_of_points) + '/'
  
  for result_index in range(0, num_results):
    index_str = str(result_index + 1)

    if result_index % 10 == 0 and result_index > 0:
      print 'Collected ' + str(result_index) + ' of the results'
    
    filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    initial_results = load_pickle_file(filename)[0]
    initial_paths = [True for i in range(0, len(initial_results))]

    new_paths = container_analysis(non_planar_container, filename, initial_paths)

    filename = results_gg_location + point_str + gg_placement + gg_filename + index_str    
    container_analysis(gg_container, filename, new_paths)
    
    filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    container_analysis(rng_container, filename, new_paths)

  # I save the results so they are independent of LaTeX code we are going to generate
  non_planar_container.finalize()
  gg_container.finalize()
  rng_container.finalize()
  
  filename = results_non_location + point_str + 'graph_results'
  save_pickle_file(filename, non_planar_container)
  
  filename = results_gg_location + point_str + 'gg_results'
  save_pickle_file(filename, gg_container)
  
  filename = results_rng_location + point_str + 'rng_results'
  save_pickle_file(filename, rng_container)
  """ 
  if debug:
    print non_planar_container
    print '***'
    print gg_container
    print '***'
    print rng_container  
  """

def container_analysis(container, filename, paths):
  try:
    (main_results, neighbours, graph_distance, num_cc) = load_pickle_file(filename)
    (total_number, neigh_max, neigh_min, neigh_avg) = neighbours
  except ValueError:
   print filename
   index = 0
   for item in load_pickle_file(filename): 
     print "**" + str(index) +"**: " + str(item)
     print "------------------------------------"
     index += 1
 
  container.edge_number += total_number
  container.max_neighbours = max(neigh_max, container.max_neighbours)
  container.min_neighbours = min(neigh_min, container.min_neighbours)
  container.average_edges.append(neigh_avg)

  container.total_length += graph_distance
  container.cc.append(num_cc)

  old_missing_path = copy.deepcopy(paths)  
  
  distance = 0
  unit_distance = 0
  
  empty = 0
  legal_empty = 0

  missing_path = []

  for result_index in range(0, len(main_results)):
    result = main_results[result_index]
    (local_distance, path_length, local_unit_distance) = result

    if path_length == 0:

      # if there was not an error in the graphs before this one, then we up the number of empty paths       
      if not old_missing_path[result_index]:
        empty += 1
      
      # Regardless of what happened above, we need to mention that there is a missing path here
      missing_path.append(True)
      # And we note that there is an empty path
      legal_empty += 1
    elif path_length == 0 and local_unit_distance > 0:
      print 'Error 1'
      raise BaseException, 'Path error, Unit path not the same as path. Unit path:' + len(unit_path) + ', path: ' + len(path)
    elif path_length > 0 and local_unit_distance == 0:
      print 'Error 2'
      raise BaseException, 'Path error, Path not the same as unit path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    else:
       # We record that we can reach the point - so no there is no missing path
      missing_path.append(False)
  
      container.distance.append(local_distance)
      container.unit_distance.append(local_unit_distance)
  
  # Append the results to the container
  container.num_errors += empty
  container.legal_empty += legal_empty

  return missing_path

def save_file(file_name, data):
  file_name += '.tex'
  with open(file_name, mode='w') as f:
    f.write(data)  

def print_latex_results(number_of_points):
  
  point_str = str(number_of_points) + '/'
  
  filename = results_non_location + point_str + 'graph_results' 
  graph_results = load_pickle_file(filename)
  
  filename = results_gg_location + point_str + 'gg_results'
  gg_results = load_pickle_file(filename)
  
  filename = results_rng_location + point_str + 'rng_results'
  rng_results = load_pickle_file(filename)

  newline = "\\\\\n"
  graph_latex = graph_results.get_latex_values()
  gg_latex = gg_results.get_latex_values()
  rng_latex = rng_results.get_latex_values()

  graph_dist_list = graph_latex["distance"]
  gg_dist_list = gg_latex["distance"]
  rng_dist_list = rng_latex["distance"]

  graph_unit_list = graph_latex["unit"]
  gg_unit_list = gg_latex["unit"]
  rng_unit_list = rng_latex["unit"]

  graph_unit_distance = graph_results.unit_distance_value * 1.0
  gg_unit_distance = gg_results.unit_distance_value * 1.0
  rng_unit_distance = rng_results.unit_distance_value * 1.0

  graph_distance = graph_results.distance_value * 1.0
  gg_distance    = gg_results.distance_value * 1.0
  rng_distance   = rng_results.distance_value * 1.0
  
  graph_gg =  c_round(gg_distance / graph_distance * 100.00)
  graph_rng = c_round(rng_distance / graph_distance * 100.00)
  
  graph_gg_unit  = c_round((gg_unit_distance  / graph_unit_distance) * 100.00)
  graph_rng_unit = c_round((rng_unit_distance / graph_unit_distance) * 100.00)

  graph_cc = graph_latex["CC"]

  graph_error = graph_latex["error"]
  gg_error    = gg_latex["error"]
  rng_error   = rng_latex["error"]

  latex_table =  "\\begin{tabular}{ccrrrr}\n"
  latex_table += "\\multicolumn{2}{}{}        & Length of graph: & Max node-pair: & Min node-pair: & Avg node-pair" + newline
  latex_table += "\\multirow{3}{*}{Distance}  & NML & %s & %s & %s & %s%s " % (graph_dist_list[0], graph_dist_list[1], graph_dist_list[2], graph_dist_list[3], newline) 
  latex_table += "                            & GG  &  %s & %s & %s & %s%s" % (gg_dist_list[0], gg_dist_list[1], gg_dist_list[2], gg_dist_list[3], newline)
  latex_table += "                            & RNG & %s & %s & %s & %s%s " % (rng_dist_list[0], rng_dist_list[1], rng_dist_list[2], rng_dist_list[3], newline) 
  latex_table += "\\hline \n"
  latex_table += "Unit      & NML & %s\phantom{.00} & %s & %s & %s%s" % (graph_unit_list[0], graph_unit_list[1], graph_unit_list[2], graph_unit_list[3], newline)  
  latex_table += "Distance  & GG  & %s\phantom{.00} & %s & %s & %s%s" % (gg_unit_list[0], gg_unit_list[1], gg_unit_list[2], gg_unit_list[3], newline)  
  latex_table += "          & RNG & %s\phantom{.00} & %s & %s & %s%s" % (rng_unit_list[0], rng_unit_list[1], rng_unit_list[2], rng_unit_list[3], newline)
  latex_table += "\hline\n" 
  latex_table += "\hline\n"
  latex_table += "               &     & Distance:   & Unit Distance: & %s &  %s %s" % ("\multicolumn{1}{||c}{}", "\# Missing paths", newline) 
  latex_table += "Percentage     & NML & 100.00 \\%% & 100.00 \\%%    & %s &  %s %s" % ("\multicolumn{1}{||c}{NML}", gg_error, newline)
  latex_table += "compared to the& GG  & %s     \\%% & %s \\%%        & %s &  %s %s" % (graph_gg, graph_gg_unit,  "\multicolumn{1}{||c}{GG}" , gg_error, newline)
  latex_table += "normal graph   & RNG & %s     \\%% & %s \\%%        & %s &  %s %s" % (graph_rng, graph_rng_unit,"\multicolumn{1}{||c}{RNG}", rng_error, newline)
  latex_table += "\hline\n"
  latex_table += "\# Connected Components: & %s \n" % graph_cc
  latex_table += "\end{tabular}"

  save_file(latex_location + 'graph_results_' + point_str[0:-1], latex_table)

def make_pgf_graph(item_list, plot, legend):
  latex_graph = ""
  latex_graph += "\\begin{tikzpicture}\n"
  latex_graph +=  "\\begin{axis}[xlabel=Nodes in graph, ylabel=Average neighbours,legend cell align=center style={cells={anchor=east}, legend pos=outer north east,}]\n"
  
  for item in item_list:
    latex_graph += "\\%s{\n" % plot
    latex_graph += item
    latex_graph += "};\n"

  latex_graph += "\\legend{%s}\n" % legend
  latex_graph += "\\end{axis}\n"
  latex_graph += "\\end{tikzpicture}\n"

  return latex_graph

def print_graphs(number_list):
  graph_avg = ""
  gg_avg = ""
  rng_avg = ""
  graph_folder = "../../report/figures/graph/"

  for number in number_list:
    point_str = str(number) + '/'
  
    filename = results_non_location + point_str + 'graph_results' 
    graph_results = load_pickle_file(filename)
  
    filename = results_gg_location + point_str + 'gg_results'
    gg_results = load_pickle_file(filename)
  
    filename = results_rng_location + point_str + 'rng_results'
    rng_results = load_pickle_file(filename)

    graph_latex = graph_results.get_latex_values()
    gg_latex    = gg_results.get_latex_values()
    rng_latex   = rng_results.get_latex_values()

    graph_neighbours = graph_latex["neighbours"]
    gg_neighbours    = gg_latex["neighbours"]
    rng_neighbours   = rng_latex["neighbours"]

    graph_avg += "(%s, %s) " % (number, graph_neighbours[2])
    gg_avg    += "(%s, %s) " % (number, gg_neighbours[2])
    rng_avg   += "(%s, %s) " % (number, rng_neighbours[2])

  avg_neigh_graph = make_pgf_graph([graph_avg, gg_avg, rng_avg], "addplot+[only marks] coordinates", "Non-planar, Gabriel Graph, RNG")
  save_file(graph_folder + "avg_neigh", avg_neigh_graph)

  
def do_integrity_test(point_list, node_pairs):
  (normal_graph, tree) = make_graph.SciPy_KDTree(point_list, 20)
    
  gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(normal_graph), tree)
  
  rng_graph = make_graph.rn_graph(copy.deepcopy(normal_graph))

  non_planar_results = do_actual_test(normal_graph, node_pairs)
  gg_results         = do_actual_test(gabriel_graph, node_pairs)
  rng_results        = do_actual_test(rng_graph, node_pairs)

  non_planar_container = results_container('Non-planar')
  gg_container         = results_container('Gabriel Graph')
  rng_container        = results_container('Relative Neighbourhood Graph')

  init_missing_paths = [True]
  empty_index = analyse_individual_results(non_planar_results, init_missing_paths, non_planar_container)
  analyse_individual_results(gg_results        , empty_index       , gg_container        )
  analyse_individual_results(rng_results       , empty_index       , rng_container       )

  non_planar_container.finalize()
  gg_container.finalize()
  rng_container.finalize()

def make_point(num, filename):
  graph_results = load_pickle_file(filename)
  
  avg = graph_results.average_neighbours
  max_val = graph_results.max_neighbours
  min_val = graph_results.min_neighbours
  avg_neigh = "\t(%s, %s)\n" % (num, avg)    # +- (%s, %s)\n" % (num, avg, max_val - avg, min_val)
  
  avg = graph_results.average_unit_distance
  max_val = graph_results.max_unit_value
  min_val = graph_results.min_unit_value
  
  avg_unit = "\t(%s, %s)\n" % (num, avg)    # +- (%s, %s)\n" % (num, avg, max_val - avg, min_val)
  return (avg_neigh, avg_unit) 


def make_point_distance_hops(num):
  point_str = str(num) + "/"

  filename = results_non_location + point_str + 'graph_results' 
  graph_result = load_pickle_file(filename)

  filename = results_non_location + point_str + 'gg_results' 
  gg_result = load_pickle_file(filename)

  filename = results_non_location + point_str + 'rng_results' 
  rng_result = load_pickle_file(filename)
  
  graph_distance = graph_result.distance_value
  gg_distance    = gg_result.distance_value
  rng_distance   = rng_result.distance_value

  graph_distance_unit = graph_result.unit_distance_value
  gg_distance_unit    = gg_result.unit_distance_value
  rng_distance_unit   = rng_result.unit_distance_value

  gg_percent = (gg_distance * 1.0 / graph_distance) * 100
  rng_percent = (rng_distance * 1.0 / graph_distance) * 100

  gg_unit_percent = (gg_distance_unit * 1.0 / graph_distance_unit) * 100
  rng_unit_percent = (rng_distance_unit * 1.0 / graph_distance_unit) * 100
  
  todo = [gg_percent, rng_percent, gg_unit_percent, rng_unit_percent]
  
  val_list = []

  for val in todo:
    val_list.append("(%s, %s)\n" % (num, val))

  return val_list

def make_result_graphs(num_points_range):
  make_distance_hops(num_points_range)
  make_neigh_hops(num_points_range)

def make_distance_hops(num_points_range):
  default_1 = "\\addplot[color=%s, mark=%s] coordinates{                \n" 
  default_2 = "\\addplot[color=%s, mark=%s, densely dashed] coordinates{\n" 

  gg_distance  = default_1 % ("blue", "*")
  rng_distance = default_1 % ("black", "triangle*")

  gg_distance_unit  = default_2 % ("blue", "o" )
  rng_distance_unit = default_2 % ("black", "triangle")


  for num in num_points_range:
    (gg_dist, rng_dist, gg_dist_unit, rng_dist_unit) = make_point_distance_hops(num)
    
    gg_distance  += gg_dist
    rng_distance += rng_dist

    gg_distance_unit  += gg_dist_unit
    rng_distance_unit += rng_dist_unit

  gg_legend = "}; \\addlegendentry{Gabriel Graph %s}\n"
  rng_legend = "}; \\addlegendentry{Relative Neighbourhood Graph %s}\n"

  gg_distance  += gg_legend % "euclidian"
  rng_distance += rng_legend % "euclidian"

  gg_distance_unit  += gg_legend % "hops"
  rng_distance_unit += rng_legend % "hops"

#  axis = "semilogxaxis"
  axis = "axis"

  xticks = ""
  xtick_vals = ""
  for num in xrange(7501):
    if num > 0 and (num % 100 == 0): 
      xticks += "%s" % num
      if (num == 1000 or num % 2500 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != num_points_range[-1]:
        xticks     += ", "
        xtick_vals += ", "

  yticks      = ""
  ytick_vals  = ""
  max_val = 240
  for num in xrange(max_val + 1):
    if num >= 100 and num % 10 == 0:
      yticks     += "%s" % num
      ytick_vals += "%s" % num
      if num != max_val:
        yticks     += ", "
        ytick_vals += ", "

  y_left  = "\% compared to non-planar graph"
  y_right = "\% of hops compared to non-planar graph"

  width = "0.8\linewidth"

  graph = "\\begin{tikzpicture}\n"

  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}, }\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, ytick={%s}, yticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s]\n" % (axis, xticks, xtick_vals, yticks, ytick_vals, width, y_left)
#  graph += "\\addlegendimage{legend image code/.code=}\n\\addlegendentry{Distance(Left axis)}"
  graph += gg_distance
  graph += rng_distance
  graph += gg_distance_unit
  graph += rng_distance_unit
  """
  graph += "\\end{%s}\n\n" % axis
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.2)},anchor=north}}\n"
  graph += "\\begin{%s}[scale only axis, width=%s, transpose legend, legend columns=2, axis y line=right, axis x line=none, ylabel=%s]\n" % (axis, width, y_right)
  graph += "\\addlegendimage{legend image code/.code=}\n\\addlegendentry{Hops (Right axis)}"
  """
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "dist_percent" , graph)
  

def make_neigh_hops(num_points_range):  
  default_1 = "\\addplot[color=%s, mark=%s] coordinates{                \n" 
  default_2 = "\\addplot[color=%s, mark=%s, densely dashed] coordinates{\n" 

  non_planar = default_1 % ("red", "square*")
  gg = default_1 % ("blue", "*")
  rng = default_1 % ("black", "triangle*")

  non_planar_unit = default_2 % ("red", "square")
  gg_unit = default_2 % ("blue", "o" )
  rng_unit = default_2 % ("black", "triangle")

  for num in num_points_range:
    point_str = str(num) + "/"
    
    filename = results_non_location + point_str + 'graph_results' 
    (neighbour, distance) = make_point(num, filename)
    non_planar += neighbour
    non_planar_unit += distance

    filename = results_gg_location + point_str + 'gg_results'
    (neighbour, distance) = make_point(num, filename)
    gg += neighbour
    gg_unit += distance
  
    filename = results_rng_location + point_str + 'rng_results'
    (neighbour, distance) = make_point(num, filename)
    rng += neighbour
    rng_unit += distance

    
  planar_legend = "}; \\addlegendentry{Non-Planar Graph}\n"
  gg_legend = "}; \\addlegendentry{Gabriel Graph}\n"
  rng_legend = "}; \\addlegendentry{Relative Neighbourhood Graph}\n"

  non_planar      += planar_legend
  non_planar_unit += planar_legend 
  gg      += gg_legend
  gg_unit += gg_legend
  rng      += rng_legend
  rng_unit += rng_legend

 # axis = "semilogxaxis"
  axis = "axis"

  xticks = ""
  xtick_vals = ""
  for num in xrange(7501):
    if num > 0 and num % 100 == 0: 
      xticks += "%s" % num
      if (num == 1000 or num % 2500 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != num_points_range[-1]:
        xticks += ", "
        xtick_vals += ", "

  yticks_one = "1,...,12"
  ytick_vals_one = "1,...,12"

  yticks_two = ""
  ytick_vals_two = ""
  max_val = 66
  for num in xrange(max_val + 1):
    if num > 0 and num % 1 == 0: 
      yticks_two += "%s" % num
      if num % 10 == 0:
        ytick_vals_two += "$%s$" % num
      else:
        ytick_vals_two += ""
      if num != max_val:
        yticks_two += ", "
        ytick_vals_two += ", "

  width = "0.8\linewidth"

  graph = "\\begin{tikzpicture}\n"

  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.30)},anchor=south}, }\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, ytick={%s}, yticklabels={%s}, transpose legend, legend columns=2, width=%s, axis y line*=left, xlabel=Number of nodes in the graph, ylabel=Average number of neighbours]\n" % (axis, xticks, xtick_vals, yticks_one, ytick_vals_one, width)  
  graph += "\\addlegendimage{legend image code/.code=}\n\\addlegendentry{Neighbours (Left axis)}"
  graph += non_planar
  graph += gg
  graph += rng
  graph += "\\end{%s}\n\n" % axis

  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.2)},anchor=north}}\n"
  graph += "\\begin{%s}[scale only axis, ytick={%s}, yticklabels={%s}, width=%s, transpose legend, legend columns=3, axis y line=right, axis x line=none, ylabel=Average number of hops]\n" % (axis, yticks_two, ytick_vals_two, width)
  graph += "\\addlegendimage{legend image code/.code=}\n\\addlegendentry{Hops (Right axis)}"
  graph += non_planar_unit
  graph += gg_unit
  graph += rng_unit
  graph += "\\end{%s}\n" % axis

  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "avg_neighbour" , graph)

def do_suite(point_num, num_graphs, pr_graph_test, max_values, cut_off, start_state, end_state):	
  do_suite(point_num, num_graphs, pr_graph_test, max_values, cut_off, start_state, 0, num_graphs)	

def do_suite(point_num, num_graphs, pr_graph_test, max_values, cut_off, start_state, end_state, from_val, to_val):	
  
  """
  A single function call to tie the entire test stack together and to make it easier to parameterise

  Quick reference for state:
  0 or below: Do everything
  1: Don't do point generation
  2: Don't do graph generation
  3: Don't make node pairs
  4: Don't find the results
  5 or above: Don't analyse results - just print the LaTeX code
  

  All of the options are culimative, so if state is 3, then you won't generate pointsets, make the graphs or node pairs
  """

  print start_state

  print "Working on " + str(point_num)
  if start_state < 1:
    generate_point_sets(point_num, num_graphs, max_values)
    print "Generated Pointsets"

  if (start_state <= 1) and (end_state >= 1):
    generate_graphs(point_num, num_graphs, cut_off)
    print 'Made graphs'
  
  if (start_state <= 2) and (end_state >= 2):
    pool = Pool(processes=multiprocessing.cpu_count() - 2)
    list_parameter = []
    for i in range(0, num_graphs):
      list_parameter.append((point_num, num_graphs, pr_graph_test, i, i + 1))

    pool.map(make_node_pairs, list_parameter)
    print 'Made node pairs'

  if (start_state <= 3) and (end_state >= 3):
    pool = Pool(processes=multiprocessing.cpu_count() - 2)
    list_parameter = []
    for i in range(0, num_graphs):
      list_parameter.append((point_num, num_graphs, pr_graph_test, i, i + 1))

    pool.map(do_test, list_parameter)
    print 'Done results'
    
  if (start_state <= 4) and (end_state >= 4):
    analyse_results(point_num, num_graphs, pr_graph_test)
    print 'Analysed results'

  if (start_state <= 5) and (end_state >= 5):
    print_latex_results(point_num)
    print 'Printed LaTeX file'


def eq(points):
  return round(math.sqrt(100 * points))

"""
  Quick reference for state:
  0 or below: Do everything
  1: Don't do point generation
  2: Don't do graph generation
  3: Don't make node pairs
  4: Don't find the results
  5 or above: Don't analyse results - just print the LaTeX code"""

# point_num num_graphs pr_graphs_test max_values cut_off state
#do_suite(10, 2, 30, 20, 15, 0)

pr_test = 100
radio_range = 20


num_to_do = 250
step = num_to_do / 7
mod = num_to_do % 7
step = 500 / 7

start_state = 2
end_state = 5
number_tests = 500

for num_nodes in [100, 250, 1000, 500, 2500, 5000, 7500, 10000]:
  do_suite(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, 0, number_tests)

make_result_graphs([100, 250, 500, 1000, 2500, 5000, 7500, 10000])
