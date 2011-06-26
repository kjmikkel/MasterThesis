from scipy.spatial import KDTree
import math, copy
from priodict import priorityDictionary

def brute_force(data, cut_off):
  """
  !DO NOT USE! - The brute-force method is way to slow for larger pointsets
  Find the list of lists of points that are equal or less than cut_off distance away from each other
  """
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
  """
  Find the distance between points in the euclidean space - nothing revolutionary
  """
  x_distance = abs(p1[0] - p2[0])
  y_distance = abs(p1[1] - p2[1])
    
  euclidian_distance = math.sqrt(x_distance * x_distance + y_distance * y_distance)
  return euclidian_distance


def find_midpoint(p1, p2):
  """
  Returns the point that are placed half-way between p1 and p2 on their line-segment
  """
  min_x = min(p1[0], p2[0])
  min_y = min(p1[1], p2[1])

  max_x = max(p1[0], p2[0])
  max_y = max(p1[1], p2[1])

  mid_x = min_x + (abs(max_x - min_x) * 1.0) / 2.0
  mid_y = min_y + (abs(max_y - min_y) * 1.0) / 2.0

  return (mid_x, mid_y)


def gabriel_graph_old(edges, neighbours_dict):
  """
  The old version of the gabriel graph code - this version does not use the kDTree code, but rather uses an already existing graph and then eliminates the edges where a node exist in the disc
  """
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
  """
  The new version of the Gabriel Graph algorithm which uses the kd-tree to create the edges, instead of removing them
  """

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
      mid_point_radius = (distance_to_v * 1.0)  / 2.0
    
      close_nodes = kd_tree.query_ball_point(mid_point, mid_point_radius)      
         
      if len(close_nodes) == 2:
        local_edges[v] = distance_to_v   
    
    graph[u] = local_edges    
  return graph

def rng_graph_old(edges, neighbours_dict):
  """
  The old version of the Relative Neighbourhood graph, which removes edges that should not be in the neighbourhood graph - 
  """
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
  """
  Current implementation of the Relative Neighbourhood Graph, still based on the old graph, but this version should be faster
  """
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
          add_edge = False
          break
      
      if add_edge:
        local_graph[v] = distance_to_v
    
    graph[u] = local_graph
            
  return graph



def MST_Kruskal(graph):
  """
  An implementation of Kruskal and Prim's Minimum Spanning tree algorithm - implemented after the description in Cormen
  """

  result_graph = {}
  set_container = {}

  # Initially the nodes only know themselves
  for node in graph:
    set_container[node] = [node]
    result_graph[node] = {}
  
  # we make the priority queue
  Q = priorityDictionary()
  # We make sure the edge is only there once
  already_there = {}
  for outer_node in graph:
    inner_nodes = graph[outer_node]
    
    for inner_node in inner_nodes:
      if already_there.get((inner_node, outer_node)):
        continue
     
      already_there[(inner_node, outer_node)] = 1
      already_there[(outer_node, inner_node)] = 1

      Q[(outer_node, inner_node)] = inner_nodes[inner_node]
          
  for edge in Q:
    p1 = edge[0]
    p2 = edge[1]
    
    if set_container[p1] != set_container[p2]:
      result_graph[p1][p2] = Q[edge]
      result_graph[p2][p1] = Q[edge]
      
      set1 = set_container[p1]
      set2 = set_container[p2]

      for node in set2:
        if not node in set1:
          set1.append(node)

      for node in set1:
        set_container[node] = set1

  return result_graph 



def SciPy_KDTree(data, cutoff_distance):
  """
  The kDTree is used to create the "complete"-moblie ad-hoc graph
  """
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
