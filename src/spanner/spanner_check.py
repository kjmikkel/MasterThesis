import os, sys, cProfile, pstats, random, copy, math, json, pickle

cmd_folder = os.path.dirname(os.path.abspath(__file__)) + '/../graph_support'
if cmd_folder not in sys.path:
  sys.path.insert(0, cmd_folder)

import make_graph, dijkstra

max_values = 50
number_of_points = 1000
cutoff_distance = 5
num_graphs = 10
pr_graph_test = 10

non_planar_f = 'Graphs/Non-planar/graph_'
gabriel_graph_f = 'Graphs/Gabriel graphs/gg_graph_' 
rng_f = 'Graphs/RNG graphs/rng_graph_'

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
    	  	   
    for i in range(0, number_of_points):
      entry = (random.randint(0, max_values), random.randint(0, max_values))
      data.append(entry)
    
    file_name = "Pointsets/pointset_" + str(num + 1)
    save_pickle_file(file_name, data)

def profiling():
  k_data = copy.deepcopy(data)
  cProfile.run('SciPy_KDTree(k_data,cutoff_distance)', sort=1)

def generate_graphs(num):
  for graph_index in range(0, num):
    
    file_name = "Pointsets/pointset_" + str(graph_index + 1)
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
    non_planar_graph = load_pickle_file(non_planar_f + str(graph_index + 1))
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

      (D, P, Path) = Dijkstra.shortestPath(non_planar_graph, start_node, end_node)
      print Path, D[end_node]

      (D, P, Path) = Dijkstra.shortestUnitPath(non_planar_graph, start_node, end_node)
      print Path, D[end_node]


#generate_point_sets(1)
#generate_graphs(1)
perform_test(1, 1) #pr_graph_test)
