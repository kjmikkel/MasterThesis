import os, sys, random, copy, math, json, pickle
from decimal import *
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

    # Statistics about edges - implement this!
    self.edges = []
    self.edge_number = 0
    self.average_neighbours = 0
    self.max_neighbours = 0
    self.min_neighbours = 0

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

  def print_latex(self):

    newline = '\\\\\n'
    
    string  = '\\begin{tabular}{lll}\n'
    string += ' & Distance & Unit Distance' + newline
    string += 'Total: & ' + c_round(self.distance_value) + ' & ' + str(self.unit_distance_value) +  newline
    string += 'Average: & ' + c_round(self.average_distance) + ' & ' + c_round(self.average_unit_distance) + newline
    string += 'Min value: & ' + c_round(self.min_value) + ' & ' + str(self.min_unit_value) + newline
    string += 'Max value: & ' + c_round(self.max_value) + ' & ' + str(self.max_unit_value) + newline
    string += '\\hline\n'
    string += 'Number of missing paths: & ' + str(self.num_errors) + ' &' + newline
    string += '\\end{tabular}' + newline
    string += 'Number of edges: ' + str(self.edge_number / 2) + newline
    string += 'Average numbers of neighbours: ' + str(self.average_neighbours) + newline
    string += 'Minimum number of neighbours: ' + str(self.max_neighbours) + newline
    string += 'Maximum number of neighbours: ' + str(self.min_neighbours) + newline

    
    return string 

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
  return "%.4f" % num

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

    (normal_graph, tree) = make_graph.SciPy_KDTree(new_data, cutoff_distance)
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    save_pickle_file(filename, normal_graph)   
    
    gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(normal_graph), tree, new_data)
    filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    save_pickle_file(filename, gabriel_graph)

    rng_graph = make_graph.rn_graph(copy.deepcopy(normal_graph))
    filename = rng_f + point_str + rng_placement + rng_filename + index_str
    save_pickle_file(filename, rng_graph)
            
def make_node_pairs(number_of_points, num, number_tests):
  point_str = str(number_of_points) + '/'

  for graph_index in range(0, num):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made node pairs for ' + str(graph_index) + ' tests.'

    node_pairs_to_check = []
    
    index_str = str(graph_index + 1)
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    non_planar_graph = load_pickle_file(filename)

    (_ , set_container) = make_graph.MST_Kruskal(non_planar_graph)
    nodes = non_planar_graph.keys()

    random.seed()
    already_there = {}
    for test_index in range(0, number_tests):
      """
      if test_index % 10 == 0:
        print 'Made ' + str(test_index) + ' out of ' + str(number_tests)
      """
      # We must ensure the pair is not replicated
      new_pair = False
      num_failure = 100

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
          num_failure = 100
        else:
          #emergency handbrake 
          num_failure -= 1
          if num_failure <= 0:
            break

    save_pickle_file(node_pair_location + point_str + "node_pair_" + str(graph_index + 1), node_pairs_to_check)

def get_edge_data(graph):
    return_dict = {}
    nodes = graph.keys()
    
    neigh_list = []
    for node in nodes:
        neigh_list.append(len(graph[node]))
        
    return neigh_list

def do_test(number_of_points, num, number_tests):
  point_str = str(number_of_points) + '/'
  for graph_index in range(0, num):

    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' tests.'

    index_str = str(graph_index + 1)   

    node_pairs_to_check = load_pickle_file(node_pair_location + point_str + 'node_pair_' + str(graph_index + 1))
    non_nodepairs = copy.deepcopy(node_pairs_to_check)

    # We perform the actual tests
    
    # Start Non-planar
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    non_planar_graph = load_pickle_file(filename)

    non_planar_results = do_actual_test(non_planar_graph, non_nodepairs) 
    non_planar_neigh   = get_edge_data(non_planar_graph)
    
    filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    save_pickle_file(filename, (non_planar_results, non_planar_neigh))
    # End non-planar

    # Start Gabriel Graph
    filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    gabriel_graph = load_pickle_file(filename)
    gg_nodepairs = copy.deepcopy(node_pairs_to_check)
    
    gabriel_graph_results = do_actual_test(gabriel_graph, gg_nodepairs)    
    gabriel_graph_neigh   = get_edge_data(gabriel_graph)
    
    filename = results_gg_location + point_str + gg_placement + gg_filename + index_str
    save_pickle_file(filename, (gabriel_graph_results, gabriel_graph_neigh))
    # End Gabriel Graph

    # Start RNG 
    filename = rng_f + point_str + rng_placement + rng_filename + index_str
    rn_graph = load_pickle_file(filename) 
    rng_nodepairs = copy.deepcopy(node_pairs_to_check)
    
    rn_graph_results = do_actual_test(rn_graph, rng_nodepairs)
    rn_graph_neigh   = get_edge_data(rn_graph)
    
    filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    save_pickle_file(filename, (rn_graph_results, rn_graph_neigh))
    # End RNG    
        
def do_actual_test(graph, node_pairs):
  results = []
  for pair in node_pairs:
    (start_node, end_node) = pair 
    
    (D, P, Path) = dijkstra.shortestPath(graph, start_node, end_node)  
    (D_unit, P_unit, Path_unit) = dijkstra.shortestUnitPath(graph, start_node, end_node)

    if len(Path) > 0:
      distance = D[end_node]
      unit_distance = D_unit[end_node]
    else:
      distance = -1 
      unit_distance = -1
    
    results.append((distance, Path, unit_distance, Path_unit))
  
  return results
  
def analyse_individual_results(results, old_missing_path, container):
  distance = 0
  unit_distance = 0
  
  empty = 0
  legal_empty = 0

  missing_path = []

  for result_index in range(0, len(results)):
    result = results[result_index]

    (local_distance, path, local_unit_distance, unit_path) = result

    if len(path) == 0 and len(unit_path) == 0:

      # if there was not an error in the graphs before this one, then we up the number of empty paths       
      if not old_missing_path[result_index]:
        print result
        empty += 1
      
      # Regardless of what happened above, we need to mention that there is a missing path here
      missing_path.append(True)
      # And we note that there is an empty path
      legal_empty += 1
    elif len(path) == 0 and len(unit_path) > 0:
      print 'Error 1'
      raise BaseException, 'Path error, Unit path not the same as path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    elif len(path) > 0 and len(unit_path) == 0:
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
  return (missing_path, container)

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
    graph_results = load_pickle_file(filename)      
    graph_distance = graph_results[0]
    non_planar_container.edges = graph_results[1] 
    
    init_missing_paths = [True for i in range(0, len(graph_distance))]
    (empty_index, _) = analyse_individual_results(graph_distance, init_missing_paths, non_planar_container)

    filename = results_gg_location + point_str + gg_placement + gg_filename + index_str    
    gg_results = load_pickle_file(filename)
    gg_distance = gg_results[0]
    gg_container.edges   = gg_results[1]
    
    gg_index = copy.deepcopy(empty_index)
    (_, _)  = analyse_individual_results(gg_distance, gg_index, gg_container)
    
    filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    rng_results = load_pickle_file(filename)
    rng_distance = rng_results[0]
    rng_container.edges    = rng_results[1]
    
    rng_index = copy.deepcopy(empty_index)
    (_, _)  = analyse_individual_results(rng_distance, rng_index, rng_container)

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

  print 'Normal graph'
  graph_latex = graph_results.print_latex()
  save_file(latex_location + 'graph_results_' + point_str[0:-1], graph_latex) 

  print 'Gabriel graph'
  gg_latex = gg_results.print_latex()
  save_file(latex_location + 'gg_results_' + point_str[0:-1], gg_latex) 

  print 'RN graph'
  rng_latex = rng_results.print_latex()
  save_file(latex_location + 'rng_results_' + point_str[0:-1], rng_latex)

  print 'Total results'
  graph_distance = graph_results.distance_value * 1.0
  gg_distance    = gg_results.distance_value * 1.0
  rng_distance   = rng_results.distance_value * 1.0

  graph_gg =  gg_distance / graph_distance
  graph_rng = rng_distance / graph_distance

  graph_unit_distance = graph_results.unit_distance_value * 1.0
  gg_unit_distance = gg_results.unit_distance_value * 1.0
  rng_unit_distance = rng_results.unit_distance_value * 1.0

  graph_gg_unit  = gg_unit_distance  / graph_unit_distance
  graph_rng_unit = rng_unit_distance / graph_unit_distance

  newline = '\\\\\n'
  line   = '\\hline\n'
  string = '\\begin{tabular}{l|c|c|}' + '\n'
  string += ' & Gabriel Graph & RNG' + newline
  string += line
  string += 'Distance: & ' + c_round(graph_gg) + ' & ' + c_round(graph_rng) + newline
  string += 'Unit Distance: & ' + c_round(graph_gg_unit) + ' & ' + c_round(graph_rng_unit) + newline
  string += '\\end{tabular}'  

  save_file(latex_location + 'spanner_' + point_str[0:-1], string)
  

def do_suite(point_num, num_graphs, pr_graph_test, max_values, cut_off, state):
  """
  A single function call to tie the entire test stack together and to make it easier to parameterise
  """

  """
  Quick reference for state:
  0 or below: Do everything
  1: Don't do point generation
  2: Don't do graph generation
  3: Don't make node pairs
  4: Don't find the results
  5 or above: Don't analyse results - just print the LaTeX code
  

  All of the options are culimative, so if state is 3, then you won't generate pointsets, make the graphs or node pairs
  """  

  if state < 1:
    generate_point_sets(point_num, num_graphs, max_values)
    print "Generated Pointsets"

  if state <= 1:
    generate_graphs(point_num, num_graphs, cut_off)
    print 'Made graphs'
  
  if state <= 2:
    make_node_pairs(point_num, num_graphs, pr_graph_test)
    print 'Made node pairs'

  if state <= 3:
    do_test(point_num, num_graphs, pr_graph_test)
    print 'Done results'
    
  if state <= 4:
    analyse_results(point_num, num_graphs, pr_graph_test)
    print 'Analysed results'

  print_latex_results(point_num)
  print 'Printed LaTeX file'

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
  (empty_index, _) = analyse_individual_results(non_planar_results, init_missing_paths, non_planar_container)
  (_, _)           = analyse_individual_results(gg_results        , empty_index       , gg_container        )
  (_, _)           = analyse_individual_results(rng_results       , empty_index       , rng_container       )

  non_planar_container.finalize()
  gg_container.finalize()
  rng_container.finalize()
  
  if debug:
    print non_planar_container
    print '***'
    print gg_container
    print '***'
    print rng_container

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
state = 5

do_suite(100, 500, 100, 200, 10, state)
do_suite(250, 500, 100, 300, 15, state)
do_suite(500, 500, 100, 400, 17, state)
do_suite(1000, 500, 100, 500, 20, state)
#do_suite(2500, 500, 100, 600, 23, state)
#do_suite(5000, 500, 100, 700, 25, 0)
#do_suite(7500, 500, 100, 800, 27, 0)
#do_suite(10000, 500, 100, 900, 30, 0)

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
