import os, sys, random, copy, math, json, pickle
from multiprocessing import Process
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

    self.min_unit_value = 0
    self.max_unit_value = 0

    self.num_errors = 0
    self.legal_empty = 0

    # Statistics about
    self.edges = []
    self.edge_number = 0
    self.average_neighbours = 0
    self.max_neighbours = 0
    self.min_neighbours = 0

    self.total_length = 0

    self.cc = [] # Connected components
    self.number_cc = 0

  def get_min_internal(self, min_dist):
    return min(min_dist)

  def get_max_internal(self, max_dist):
    return max(max_dist)

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
    
    # Edges:
    self.edge_number = sum(self.edges) / 2
    self.average_neighbours = (self.edge_number * 1.0) / len(self.edges) * 1.0
    self.max_neighbours = max(self.edges)
    self.min_neighbours = min(self.edges)

    # CC
    self.number_cc = (sum(self.cc) * 1.0) / len(self.cc) * 1.0 # Connected components

  def get_latex_values(self):
    self.finalize()

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
            
def make_node_pairs(number_of_points, num, number_tests, from_val, to_val):
  point_str = str(number_of_points) + '/'

  for graph_index in range(from_val - 1, to_val	):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made node pairs for ' + str(graph_index) + ' tests.'
	
    if os.path.exists(node_pair_location + point_str + "node_pair_" + str(graph_index + 1) + ".pickle"):
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
      print "advanced"
      node_pairs_to_check = advanced_node_pairs(number_of_points, set_container, nodes)
    else:
      node_pairs_to_check = brute_force_node_pairs(number_of_points, set_container, nodes)

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
    """
    if test_index % 10 == 0:
    print 'Made ' + str(test_index) + ' out of ' + str(number_tests)
    """
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
 

def do_test(number_of_points, num, number_tests, from_val, to_val):
  point_str = str(number_of_points) + '/'
  for graph_index in range(from_val, to_val):
    index_str = str(graph_index + 1)  

 #   if os.path.exists(results_rng_location + point_str + rng_placement + rng_filename + index_str + ".pickle"):
 #     continue

    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' tests.'


    node_pairs_to_check = load_pickle_file(node_pair_location + point_str + 'node_pair_' + str(graph_index + 1))

    # We perform the actual tests
    
    # Start Non-planar
    load_filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    save_filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    perform_tests(load_filename, save_filename, node_pairs_to_check)
    # End non-planar

    # Start Gabriel Graph
    load_filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    save_filename = results_gg_location + point_str + gg_placement + gg_filename + index_str
    perform_tests(load_filename, save_filename, node_pairs_to_check)
    # End Gabriel Graph

    # Start RNG 
    load_filename = rng_f + point_str + rng_placement + rng_filename + index_str
    save_filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    perform_tests(load_filename, save_filename, node_pairs_to_check)
    # End RNG    

def perform_tests(load_graph_name, save_data_name, node_pairs):
    graph = load_pickle_file(load_graph_name)
    local_node_pairs = copy.deepcopy(node_pairs)

    results                        = do_actual_test(graph, local_node_pairs) 
    (neighbours, graph_distance)   = get_edge_data(graph)   

    # We find all the Connected Components
    (_ , set_container) = make_graph.MST_Kruskal(graph)

    cc_dict = {}
    for node in set_container.keys():
      set_tuple = tuple(set_container[node])
      cc_dict[set_tuple] = 1
    
    num_cc = len(cc_dict.keys())  
    
    save_pickle_file(save_data_name, (results, neighbours, graph_distance, num_cc))

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
    return (neigh_list, total_length)
        
def do_actual_test(graph, node_pairs):
  results = []
  for pair in node_pairs:
    (start_node, end_node) = pair 
    
    (D, _, Path) = dijkstra.shortestPath(graph, start_node, end_node)  
    (D_unit, _, _) = dijkstra.shortestUnitPath(graph, start_node, end_node)

    if len(Path) > 0:
      try:
        distance = D[end_node]
      except KeyError:
        print D, end_node
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
  
  if debug:
    print non_planar_container
    print '***'
    print gg_container
    print '***'
    print rng_container  

def container_analysis(container, filename, paths):
  try:
    (main_results, neighbours, graph_distance, num_cc) = load_pickle_file(filename)
  except ValueError:
   print filename
   index = 0
   for item in load_pickle_file(filename): 
     print "**" + str(index) +"**: " + str(item)
     print "------------------------------------"
     index += 1
 
  container.edges.extend(neighbours)
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

    if len(path) == 0 and len(unit_path) == 0:

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

  latex_table =  "\\begin{tabular}{ccrrrr}\n"
  latex_table += "\\multicolumn{2}{}{}        & Length of graph: & Max node-pair: & Min node-pair: & Avg node-pair" + newline
  latex_table += "\\multirow{3}{*}{Distance}   & NML & %s & %s & %s & %s%s" % (graph_dist_list[0], graph_dist_list[1], graph_dist_list[2], graph_dist_list[3], newline) 
  latex_table += "                            & GG  &  %s & %s & %s & %s%s" % (gg_dist_list[0], gg_dist_list[1], gg_dist_list[2], gg_dist_list[3], newline)
  latex_table += "                            & RNG & %s & %s & %s & %s%s" % (rng_dist_list[0], rng_dist_list[1], rng_dist_list[2], rng_dist_list[3], newline) 
  latex_table += "\\hline \n"
  latex_table += "Unit      & NML & %s\phantom{.00} & %s & %s & %s%s" % (graph_unit_list[0], graph_unit_list[1], graph_unit_list[2], graph_unit_list[3], newline)  
  latex_table += "Distance  & GG  & %s\phantom{.00} & %s & %s & %s%s" % (gg_unit_list[0], gg_unit_list[1], gg_unit_list[2], gg_unit_list[3], newline)  
  latex_table += "          & RNG & %s\phantom{.00} & %s & %s & %s%s" % (rng_unit_list[0], rng_unit_list[1], rng_unit_list[2], rng_unit_list[3], newline)
  latex_table += "\hline\n" 
  latex_table += "\hline\n"
  latex_table += "             &     & Distance: & Unit Distance:" + newline 
  latex_table += "Percentage   & NML & 100.00 \% & 100,00 \%" + newline
  latex_table += "of the       & GG  & %s \\%% & %s \\%%%s" % (graph_gg, graph_gg_unit, newline)
  latex_table += "normal graph & RNG & %s \\%% %s \\%%\n" % (graph_rng, graph_rng_unit)
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
  
  if debug:
    print non_planar_container
    print '***'
    print gg_container
    print '***'
    print rng_container

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
  print "Working on " + str(point_num)
  if start_state < 1:
    generate_point_sets(point_num, num_graphs, max_values)
    print "Generated Pointsets"

  if (start_state <= 1) and (end_state >= 1):
    generate_graphs(point_num, num_graphs, cut_off)
    print 'Made graphs'
  
  if (start_state <= 2) and (end_state >= 2):
    make_node_pairs(point_num, num_graphs, pr_graph_test, from_val, to_val)
    print 'Made node pairs'

  if (start_state <= 3) and (end_state >= 3):
    do_test(point_num, num_graphs, pr_graph_test, from_val, to_val)
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
  5 or above: Don't analyse results - just print the LaTeX code
"""

# point_num num_graphs pr_graphs_test max_values cut_off state
#do_suite(10, 2, 30, 20, 15, 0)

start_state = 1
end_state = 1
number_tests = 1
pr_test = 100
radio_range = 20
for number_index in [100, 250, 500, 1000, 2500, 5000, 7500, 10000]:
  do_suite(number_index,  number_tests, pr_test, eq(number_index),  radio_range, start_state, end_state, 0, 1)

start_state = 2
end_state = 3

#for number_index in [100, 250, 500, 1000, 2500, 5000, 7500, 10000]:
#  do_suite(number_index,  number_tests, pr_test, eq(number_index),  radio_range, start_state, end_state, 0, number_tests)

step = 500 / 7
"""
p1 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, 1, step))
p2 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step, step * 2))
p3 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step * 2, step * 3))
p4 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step * 3, step * 4))
p5 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step * 4, step * 5))
p6 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step * 5, step * 6))
p7 = Process(target=do_suite, args=(10000, number_tests, pr_test, eq(10000), radio_range, start_state, end_state, step * 6, step * 7 + 3))

p1.start()
p2.start()
p3.start()
p4.start()
p5.start()
p6.start()
p7.start()

p1.join()
p2.join()
p3.join()
p4.join()
p5.join()
p6.join()
p7.join()
"""

start_state = 3
end_state = 3
"""
for num_nodes in [100, 250, 500, 1000, 2500, 5000, 7500, 10000]:
  p1 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, 1, step))
  p2 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step, step * 2))
  p3 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step * 2, step * 3))
  p4 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step * 3, step * 4))
  p5 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step * 4, step * 5))
  p6 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step * 5, step * 6))
  p7 = Process(target=do_suite, args=(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, step * 6, step * 7 + 3))

  p1.start()
  p2.start()
  p3.start()
  p4.start()
  p5.start()
  p6.start()
  p7.start()

  p1.join()
  p2.join()
  p3.join()
  p4.join()
  p5.join()
  p6.join()
  p7.join()
"""

start_state = 3
end_state = 8

#for num_nodes in [250, 500, 1000, 2500]: #, 5000, 7500, 10000]:
#  do_suite(num_nodes, number_tests, pr_test, eq(num_nodes), radio_range, start_state, end_state, 0, number_tests)

#print_graphs([250, 500])

"""
point_list = [(0,0), (20, 0), (4, -5), (15, -5)]
node_pairs = [((0,0), (20, 0))] 
do_integrity_test(point_list, node_pairs)

print ''
print '-----------'
print '***********'
print '-----------'
print ''

point_list = [(0,0), (-4, -5), (1, -10), (5, -10), (10, -10), (18, -5), (15, 0)]
node_pairs = [((0,0), (15, 0))]
do_integrity_test(point_list, node_pairs)
"""
