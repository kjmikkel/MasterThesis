from scipy.spatial import KDTree
import math, copy

# DO NOT USE
def brute_force(data, cut_off):
  result = {}
  for point in data:
    local_result = []
    
    for compare_point in data:
      if point != compare_point:
	local_distance = find_distance(point, compare_point)
	if local_distance <= cut_off:
	   local_result.append((compare_point, local_distance))

    result[point] = local_result
  
  return result

def find_distance(p1, p2):
    x_distance = abs(p1[0] - p2[0])
    y_distance = abs(p1[1] - p2[1])
    
    euclidian_distance = math.sqrt(x_distance * x_distance + y_distance * y_distance)
    return euclidian_distance

def find_midpoint(p1, p2):
    min_x = min(p1[0], p2[0])
    min_y = min(p1[1], p2[1])

    max_x = max(p1[0], p2[0])
    max_y = max(p1[1], p2[1])

    mid_x = min_x + (max_x - min_x)/2
    mid_y = min_y + (max_y - min_y)/2

    return (mid_x, mid_y)

def add_neighbour(n_dict, main_pt, sec_pt):
    neighbours = n_dict.get(main_pt)
                
    if neighbours == None:
        n_dict[main_pt] = [sec_pt]
    else:
        neighbours.append(sec_pt)

def gabriel_graph_old(edges, neighbours_dict):
    print 'All edges:', len(edges)
    
    keys = neighbours_dict.keys()
    for key in keys:
        neighbours = neighbours_dict[key]
        edges_remove = []
        outer_index = 0
        for v in neighbours:            
            mid_point = find_midpoint(key[1], v[1])
            
            for w in neighbours:
                if v == w:
                    continue

                if find_distance(mid_point, w[1]) < find_distance(key[1], mid_point):
                    edges_remove.append((key, v))
                    break

        if len(edges_remove) > 0:
            for edge in edges_remove:
                if edge in edges:
                    edges.remove(edge)
        
    print 'Changed edges:', len(edges)
    return edges

def gabriel_graph(old_graph, kd_tree):
  graph = {}
    
  nodes = old_graph.keys()
  for u in nodes:
    neighbours = old_graph[u].keys()
    local_edges = {}

    for v in neighbours:
      if v == u:
        continue            
      mid_point = find_midpoint(u, v)
      distance_to_v = old_graph[u][v]
      mid_point_radius = distance_to_v / 2            

      close_nodes = kd_tree.query_ball_point(mid_point, mid_point_radius)      
     
      if len(close_nodes) <= 2:
        local_edges[v] = distance_to_v            
    
    graph[u] = local_edges    
  return graph

def rng_graph_old(edges, neighbours_dict):
    print 'All edges:', len(edges)
    
    keys = neighbours_dict.keys()
    for key in keys:
        neighbours = neighbours_dict[key]
        edges_remove = []
        for v in neighbours:
            for w in neighbours:
                if v == w:
                    continue

                if find_distance(key[1], v[1]) > max(find_distance(key[1], w[1]), find_distance(v[1], w[1])):
                    edges_remove.append((key, v))
                    break

        if len(edges_remove) > 0:
            for edge in edges_remove:
                if edge in edges:
                    edges.remove(edge)
        
    print 'Changed edges:', len(edges)
    return edges

def rn_graph(old_graph):
  graph = {}    
  for u in old_graph:
    local_graph = {}	
    neighbours_to_u = old_graph[u]
    
    for v in neighbours_to_u:
      neighbours_to_v = old_graph[v]
       
      add_edge = True
      distance_to_v = neighbours_to_u[v]
      
      # We have to check throgh one of the arrays, and therefore it is best if we do it with the shortest array
      fewest_neighours = neighbours_to_u
      if len(neighbours_to_v) > len(neighbours_to_u):
        fewest_neighours = neighbours_to_v

      for w in fewest_neighours:
        if w == u or w == v:
          continue
        
        if distance_to_v > max(find_distance(u, w), find_distance(v, w)):
#neighbours_to_v.get(w):
          add_edge = False
          break
      
      if add_edge:
        print 'add edge: ' + str(u) + ' ' + str(v) 
        local_graph[v] = distance_to_v
    
    graph[u] = local_graph
            
  return graph

def SciPy_KDTree(data, cutoff_distance):
  result = {}
  tree = KDTree(data)

  # Now we make the actuall graph
  neighbors = tree.query_ball_point(data, cutoff_distance)
  for entry_num in range(0, len(data)):
    point = data[entry_num]
    local_neighbors = neighbors[entry_num]
    local_result = {}    
 
    for actual_neighbor in local_neighbors:
      neighbor = data[actual_neighbor]
      
      # if one of our 'neighbours' is the point itself then we skip it
      if neighbor == point:
        continue
      else:
        local_result[neighbor] = find_distance(neighbor, point)
    
    result[point] = local_result

  return (result, tree)
