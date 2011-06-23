import os, sys, cProfile, pstats, random, copy, math, json, pickle

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
  data = []
  random.seed()
	
  for num in range(0, num_pointset):
    # We must ensure that all points are uniqe
    used_points = {}

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
  for graph_index in range(0, num):
    
    if graph_index % 10 == 0 and graph_index > 0:
      print 'Made ' + str(graph_index) + ' graphs' 
    
    file_name = point_set_location + str(graph_index + 1)
    new_data = load_pickle_file(file_name)
    #new_data = [(old_entry[0], old_entry[1]) for old_entry in data]
 
    (normal_graph, tree) = make_graph.SciPy_KDTree(new_data,cutoff_distance)
    save_pickle_file(non_planar_f + str(graph_index + 1), normal_graph)   

    gabriel_graph = make_graph.gabriel_graph(copy.deepcopy(normal_graph), tree)
    save_pickle_file(gabriel_graph_f + str(graph_index + 1), gabriel_graph)

    rng_graph = make_graph.rn_graph(copy.deepcopy(normal_graph))
    save_pickle_file(rng_f + str(graph_index + 1), rng_graph)

def perform_test(num, number_tests):
  for graph_index in range(0, num):
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
    gabriel_graph_results = do_actual_test(non_planar_graph, copy.deepcopy(node_pairs_to_check))
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

    results.append((D, P, Path, D_unit, P_unit, Path_unit))
  
  return results

generate_point_sets(num_graphs)
print "Generated Pointsets"

generate_graphs(num_graphs)
print "Made graphs"

perform_test(num_graphs, pr_graph_test)
os.system('pm-hibernate')
