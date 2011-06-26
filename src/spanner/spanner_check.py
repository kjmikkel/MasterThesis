import os, sys, cProfile, pstats, random, copy, math, json, pickle, psyco

cmd_folder = os.path.dirname(os.path.abspath(__file__)) + '/../graph_support'
if cmd_folder not in sys.path:
  sys.path.insert(0, cmd_folder)

import make_graph, dijkstra

max_values = 500
number_of_points = 1000
cutoff_distance = 20
num_graphs = 500
pr_graph_test = 100

point_set_location = "Pointsets/pointset_"

non_planar_placement = 'Non-planar/graph_'
gg_placement = 'Gabriel graphs/gg_graph_'
rng_placement = 'RNG graphs/rng_graph_'

non_planar_f = 'Graphs/' + non_planar_placement
gabriel_graph_f = 'Graphs/' + gg_placement 
rng_f = 'Graphs/' + rng_placement

results_non_location = 'Results/' + non_planar_placement
results_gg_location = 'Results/' + gg_placement
results_rng_location = 'Results/' + rng_placement

""" 
Container class to have a bit more structured data transference from the results
"""
class results_container:
  min_distance = []
  max_distance = []
  distance = []
  average_distance = 0

  min_unit_distance = []
  max_unit_distance = []
  unit_distance = []
  average_unit_distance = 0

  num_errors = 0

  def get_min_internal(self, min_dist):
    result = sys.maxint
    for dist in min_dist:
      result = min(dist, result)
    
    return result

  def get_max_internal(self, max_dist):
    result = -1
    for dist in max_dist:
      result = max(dist, result)
    
    return result

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
    self.average_distance = (self.get_distance() * 1.0) / (divide_value * 1.0)
    self.average_unit_distance = (self.get_unit_distance() * 1.0) / (divide_value * 1.0)

  def __str__(self):
    string = 'Total distance: ' + str(self.get_distance()) + '\n'
    string += 'Average distance: ' + str(self.average_distance) + '\n' 
    string += 'Min: ' + str(self.get_min()) + ', Max: ' + str(self.get_max()) + '\n\n'
    
    string += 'Unit distance: ' + str(self.get_unit_distance()) + '\n'
    string += 'Average Unit distance: ' + str(self.average_unit_distance) + '\n'
    string += 'Min: ' + str(self.get_unit_min()) + ', Max: ' + str(self.get_unit_max()) + '\n\n'
    
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

def generate_point_sets(num_pointset):
  random.seed()
	
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
    
    file_name = point_set_location + str(num + 1)
    save_pickle_file(file_name, data)

def profiling():
  k_data = copy.deepcopy(data)
  cProfile.run('SciPy_KDTree(k_data,cutoff_distance)', sort=1)

def generate_graphs(num):
  generate_graphs(0, num)

def generate_graphs(from_num, num):
  for graph_index in range(from_num, num):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' graphs' 
    
    index_str = str(graph_index + 1)

    file_name = point_set_location + index_str
    new_data = load_pickle_file(file_name)

    (normal_graph, tree) = make_graph.SciPy_KDTree(new_data,cutoff_distance)
    save_pickle_file(non_planar_f + index_str, normal_graph)   

    gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(normal_graph), tree)
    save_pickle_file(gabriel_graph_f + index_str, gabriel_graph)

    rng_graph = make_graph.rn_graph(copy.deepcopy(normal_graph))
    save_pickle_file(rng_f + index_str, rng_graph)
        
def perform_test(num, number_tests):
  for graph_index in range(0, num):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Done ' + str(graph_index) + ' tests.'

    node_pairs_to_check = []

    index_str = str(graph_index + 1)
    non_planar_graph = load_pickle_file(non_planar_f + index_str)

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
    non_planar_results = do_actual_test(non_planar_graph, copy.deepcopy(node_pairs_to_check))
    save_pickle_file(results_non_location + index_str, non_planar_results)

    gabriel_graph = load_pickle_file(gabriel_graph_f + index_str)    
    gabriel_graph_results = do_actual_test(gabriel_graph, copy.deepcopy(node_pairs_to_check))
    save_pickle_file(results_gg_location + index_str, gabriel_graph_results)

    rn_graph = load_pickle_file(rng_f + index_str)
    rn_graph_results = do_actual_test(rn_graph, copy.deepcopy(node_pairs_to_check))
    save_pickle_file(results_rng_location + index_str, rn_graph_results)
    
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

  min_distance = sys.maxint
  max_distance = -1  

  min_unit_distance = sys.maxint
  max_unit_distance = -1

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
      raise 'Path error, Unit path not the same as path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    elif len(path) > 0 and len(unit_path) == 0:
      raise 'Path error, Path not the same as unit path. Unit path: ' + len(unit_path) + ', path: ' + len(path)
    else:
      # We record that we can reach the point - so no there is no missing path
      missing_path.append(False)
  
      distance      += local_distance
      unit_distance += local_unit_distance

      min_distance = min(min_distance, local_distance)
      max_distance = max(max_distance, local_distance)

      min_unit_distance = min(min_unit_distance, local_unit_distance)
      max_unit_distance = max(max_unit_distance, local_unit_distance)
  
  # Append the results to the container
  container.min_distance.append(min_distance)
  container.max_distance.append(max_distance)
  container.distance.append(distance)

  container.min_unit_distance.append(min_unit_distance)
  container.max_unit_distance.append(max_unit_distance)
  container.unit_distance.append(unit_distance)

  container.num_errors += empty

  return (missing_path, container)

def analyse_results(num_results):
  
  non_planar_container = results_container()
  gg_container         = results_container()
  rng_container        = results_container()
  
  init_missing_paths = [True for i in range(0, pr_graph_test)]
 
  for result_index in range(0, num_results):
    index_str = str(result_index + 1)

    if result_index % 10 == 0 and result_index > 0:
      print 'Collected ' + str(result_index) + ' of the results'

    graph_results = load_pickle_file(results_non_location + index_str)  
    (empty_index, non_planar_container) = analyse_individual_results(graph_results, init_missing_paths, non_planar_container)
    gg_results = load_pickle_file(results_gg_location + index_str)
    (gg_empty_index, gg_container)  = analyse_individual_results(gg_results, empty_index, gg_container)
    
    rng_results = load_pickle_file(results_rng_location + index_str)
    (rng_empty_index, rng_container)  = analyse_individual_results(rng_results, gg_empty_index, rng_container)
  
  # I save the results so they are independent of LaTeX code we are going to generate
  total_number_of_tests = num_results * pr_graph_test

  non_planar_container.finalize(total_number_of_tests)

  gg_container.finalize(total_number_of_tests)

  rng_container.finalize(total_number_of_tests)
  
  save_pickle_file('Results/graph_results', non_planar_container)
  save_pickle_file('Results/gg_results', gg_container)
  save_pickle_file('Results/rng_results', rng_container)

  print non_planar_container
  print '***'
  print gg_container
  print '***'
  print rng_container  

"""
generate_point_sets(num_graphs)
print "Generated Pointsets"
"""

generate_graphs(0, num_graphs)
print "Made graphs"

perform_test(num_graphs, pr_graph_test)
print 'Done results'

analyse_results(num_graphs)
print 'Analysed results'


#os.system('pm-hibernate')
