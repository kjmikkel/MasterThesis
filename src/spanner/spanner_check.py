import os, sys, random, copy, math, json, pickle

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

""" 
Container class to have a bit more structured data transference from the results
"""
class results_container:
  min_distance = []
  max_distance = []
  distance = []
  distance_value = 0
  average_distance = 0

  min_unit_distance = []
  max_unit_distance = []
  unit_distance = []
  unit_distance_value = 0
  average_unit_distance = 0

  min_value = 0
  max_value = 0

  min_unit_value = 0
  max_unit_value = 0

  num_errors = 0

  def get_min_internal(self, min_dist):
    return min(min_dist)

  def get_max_internal(self, max_dist):
    return max(max_dist)

  def get_min(self):
    return self.get_min_internal(self.min_distance)

  def get_max(self):
    return self.get_max_internal(self.max_distance)

  def get_unit_min(self):
    return self.get_min_internal(self.min_unit_distance)

  def get_unit_max(self):
    return self.get_max_internal(self.max_unit_distance)

  def get_distance(self):
    return sum(self.distance)

  def get_unit_distance(self):
    return sum(self.unit_distance)

  def finalize(self, divide_value):
    divide_value -= self.num_errors
    
    self.distance_value = self.get_distance()
    print self.distance_value
    self.unit_distance_value = self.get_unit_distance() 

    self.average_distance = (self.distance_value * 1.0) / (divide_value * 1.0)
    self.average_unit_distance = (self.unit_distance_value * 1.0) / (divide_value * 1.0)

    self.min_value = self.get_min()
    self.max_value = self.get_max()

    self.min_unit_value = self.get_unit_min()
    self.max_unit_value = self.get_unit_max()

  def print_latex(self):

    newline = '\\\\\n'
    string  = '\\begin{tabular}{lll}\n'
    string += ' & Distance & Unit Distance' + newline
    string += 'Total: & ' + str(self.distance_value) + ' & ' + str(self.unit_distance_value) +  newline
    string += 'Average: & ' + str(self.average_distance) + ' & ' + str(self.average_unit_distance) + newline
    string += 'Min value: & ' + str(self.min_value) + ' & ' + str(self.min_unit_value) + newline
    string += 'Max value: & ' + str(self.max_value) + ' & ' + str(self.max_unit_value) + newline
    string += '\\hline\n'
    string += 'Number of missing paths: & ' + str(self.num_errors) + ' & \n' 
    string += '\\end{tabular}'
    
    return string 

  def __str__(self):
    string = 'Total distance: ' + str(self.distance_value) + '\n'
    string += 'Average distance: ' + str(self.average_distance) + '\n' 
    string += 'Min: ' + str(self.min_value) + ', Max: ' + str(self.max_value) + '\n\n'
    
    string += 'Unit distance: ' + str(self.unit_distance_value) + '\n'
    string += 'Average Unit distance: ' + str(self.average_unit_distance) + '\n'
    string += 'Min: ' + str(self.min_unit_value) + ', Max: ' + str(self.max_unit_value) + '\n\n'

    string += 'Number of missing paths: ' + str(self.num_errors)

    return string

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
    
    gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(normal_graph), tree)
    filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    save_pickle_file(filename, gabriel_graph)

    rng_graph = make_graph.rn_graph(copy.deepcopy(normal_graph))
    filename = rng_f + point_str + rng_placement + rng_filename + index_str
    save_pickle_file(filename, rng_graph)
        
def perform_test(number_of_points, num, number_tests):
  point_str = str(number_of_points) + '/'

  for graph_index in range(0, num):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Done ' + str(graph_index) + ' tests.'

    node_pairs_to_check = []
    
    index_str = str(graph_index + 1)
    filename = non_planar_f + point_str + non_planar_placement + non_planar_filename + index_str
    non_planar_graph = load_pickle_file(filename)

    nodes = non_planar_graph.keys()

    random.seed()
    for test_index in range(0, number_tests):
      
      from_index = 0
      to_index = 0
      while from_index == to_index:
        from_index = random.randint(0, len(nodes) - 1)
        to_index = random.randint(0, len(nodes) - 1)      

      start_node = nodes[from_index]
      end_node = nodes[to_index]
      node_pairs_to_check.append((start_node, end_node))
    
    # We perform the actual tests
    non_nodepairs = copy.deepcopy(node_pairs_to_check)
    non_planar_results = do_actual_test(non_planar_graph, non_nodepairs) 
    filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    save_pickle_file(filename, non_planar_results)

    filename = gabriel_graph_f + point_str + gg_placement + gg_filename + index_str
    gabriel_graph = load_pickle_file(filename)
    gg_nodepairs = copy.deepcopy(node_pairs_to_check)
    gabriel_graph_results = do_actual_test(gabriel_graph, gg_nodepairs)
    filename = results_gg_location + point_str + gg_placement + gg_filename + index_str
    save_pickle_file(filename, gabriel_graph_results)

    filename = rng_f + point_str + rng_placement + rng_filename + index_str
    rn_graph = load_pickle_file(filename) 
    rng_nodepairs = copy.deepcopy(node_pairs_to_check)
    rn_graph_results = do_actual_test(rn_graph, rng_nodepairs)
    filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    save_pickle_file(filename, rn_graph_results)
    
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

  min_value = sys.maxint
  max_value = -1  

  min_unit_value = sys.maxint
  max_unit_value = -1

  missing_path = []

  for result_index in range(0, len(results)):
    result = results[result_index]

    (local_distance, path, local_unit_distance, unit_path) = result

    if len(path) == 0 and len(unit_path) == 0:
 
     # if there was not an error in the graphs before this one, then we up the number of empty paths       
      if not old_missing_path[result_index]:
        empty += 1
      
      # Regardless of what happened above, we need to mention that there is a missing path here
      missing_path.append(True)
      continue
    elif len(path) == 0 and len(unit_path) > 0:
      print 'Error 1'
      raise 'Path error, Unit path not the same as path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    elif len(path) > 0 and len(unit_path) == 0:
      print 'Error 2'
      raise 'Path error, Path not the same as unit path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    else:
      # We record that we can reach the point - so no there is no missing path
      missing_path.append(False)
  
      distance      += local_distance
      unit_distance += local_unit_distance

      min_value = min(min_value, local_distance)
      max_value = max(max_value, local_distance)

      min_unit_value = min(min_unit_value, local_unit_distance)
      max_unit_value = max(max_unit_value, local_unit_distance)
  
  # Append the results to the container
  
  container.min_distance.append(min_value)
  container.max_distance.append(max_value)
  container.distance.append(distance)

  container.min_unit_distance.append(min_unit_value)
  container.max_unit_distance.append(max_unit_value)
  container.unit_distance.append(unit_distance)

  container.num_errors += empty

  return (missing_path, container)

def analyse_results(number_of_points, num_results, pr_graph_test):
  
  non_planar_container = results_container()
  gg_container         = results_container()
  rng_container        = results_container()

  point_str = str(number_of_points) + '/'
  
  init_missing_paths = [True for i in range(0, pr_graph_test)]
 
  for result_index in range(0, num_results):
    index_str = str(result_index + 1)

    if result_index % 10 == 0 and result_index > 0:
      print 'Collected ' + str(result_index) + ' of the results'
#: ' + str(non_planar_container.get_min()) + ', ' + str(gg_container.get_min()) + ', ' + str(rng_container.get_min()) 

    filename = results_non_location + point_str + non_planar_placement + non_planar_filename + index_str
    graph_results = load_pickle_file(filename)  
    (empty_index, non_planar_container) = analyse_individual_results(graph_results, init_missing_paths, non_planar_container)

    filename = results_gg_location + point_str + gg_placement + gg_filename + index_str    
    gg_results = load_pickle_file(filename)
    (gg_empty_index, gg_container)  = analyse_individual_results(gg_results, copy.deepcopy(empty_index), gg_container)
    
    filename = results_rng_location + point_str + rng_placement + rng_filename + index_str
    rng_results = load_pickle_file(filename)
    (rng_empty_index, rng_container)  = analyse_individual_results(rng_results, copy.deepcopy(empty_index), rng_container)
  
  # I save the results so they are independent of LaTeX code we are going to generate
  total_number_of_tests = num_results * pr_graph_test

  non_planar_container.finalize(total_number_of_tests)
  gg_container.finalize(total_number_of_tests)
  rng_container.finalize(total_number_of_tests)
  
  filename = results_non_location + point_str + 'graph_results' 
  save_pickle_file(filename, non_planar_container)
  
  filename = results_gg_location + point_str + 'gg_results'
  save_pickle_file(filename, gg_container)
  
  filename = results_rng_location + point_str + 'rng_results'
  save_pickle_file(filename, rng_container)

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
  gg_latex = graph_results.print_latex()
  save_file(latex_location + 'gg_results_' + point_str[0:-1], gg_latex) 

  print 'RN graph'
  rng_latex = rng_results.print_latex()
  save_file(latex_location + 'rng_results_' + point_str[0:-1], rng_latex)

def do_suite(point_num, num_graphs, pr_graph_test, max_values, cut_off):

#  generate_point_sets(point_num, num_graphs, max_values)
  print "Generated Pointsets"

#x  generate_graphs(point_num, num_graphs, cut_off)
  print 'Made graphs'
  
 # perform_test(point_num, num_graphs, pr_graph_test)
  print 'Done results'

#  analyse_results(point_num, num_graphs, pr_graph_test)
  print 'Analysed results'

  print_latex_results(point_num)
  print 'Printed LaTeX file'


#do_suite(100, 50, 100, 250, 10)
"""
max_values = 500
number_of_points = 1000
cutoff_distance = 20
num_graphs = 500
pr_graph_test = 100
"""

"""
generate_point_sets(num_graphs)
print "Generated Pointsets"
"""

"""
generate_graphs(0, num_graphs)
print "Made graphs"
"""

"""
perform_test(num_graphs, pr_graph_test)
print 'Done results'
"""

"""
analyse_results(num_graphs)
print 'Analysed results'
"""
"""
print_latex_results()
print 'Printed LaTeX file'
"""

#os.system('pm-hibernate')
