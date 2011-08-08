from scipy.spatial import KDTree
from scipy.spatial import Delaunay
import math, copy
from priodict import priorityDictionary
from datetime import datetime

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


def gabriel_graph_old(old_graph, kd_tree):
  """
  The old version of the gabriel graph code - this version does not use the kDTree code, but rather uses an already existing graph and then eliminates the edges where a node exist in the disc
  """   
  new_graph = copy.deepcopy(old_graph) 
  nodes = new_graph.keys()
  for u in nodes:
    neighbours = old_graph[u]

    for v in neighbours:            
      mid_point = find_midpoint(u, v)
            
      for w in neighbours:
        if v == w:
          continue

        if find_distance(mid_point, w) < find_distance(v, mid_point):
          del neighbours[v]
          break
        
  return new_graph

def gabriel_graph(old_graph, kd_tree, data):
  """
  The new version of the Gabriel Graph algorithm which uses the kd-tree to create the edges, instead of removing them
  """

  graph = {}
    
  nodes = old_graph.keys()
  for u in nodes:
    neighbours = old_graph[u].keys()
    local_edges = {}

    for v in neighbours:          
      mid_point = find_midpoint(u, v)
      distance_to_v = old_graph[u][v]
      mid_point_radius = ((distance_to_v * 1.0)  / 2.0) + 0.000000001 # This last bit is due to rounding errors
    
      close_nodes = kd_tree.query_ball_point(mid_point, mid_point_radius)      
      """  
      if len(close_nodes) < 2:
          print mid_point, u, v
          print distance_to_v, find_distance(v, u), mid_point_radius 
          print close_nodes, data[close_nodes[0]]
      """   
      if len(close_nodes) == 2:
        local_edges[v] = distance_to_v   
    
    graph[u] = local_edges    
  return graph

def gabriel_graph_kdtree(points, kd_tree, cutoff_distance):
  """
  The new version of the Gabriel Graph algorithm which uses the kd-tree to create the edges, instead of removing them
  """
  graph = {}
  edges = make_delaunay_edges(points)
    
  for edge in edges:
    u = edge[0]
    v = edge[1]
    distance_to_v = find_distance(u, v)
    if cutoff_distance < distance_to_v:
      continue
    mid_point = find_midpoint(u, v)

    # This last addition is due to avoid problems with rounding errors, should not create problems as the points have their coordinates as integers
    mid_point_radius = find_distance(u, mid_point)  + 0.000001   
    close_nodes = kd_tree.query_ball_point(mid_point, mid_point_radius)      
      
    if len(close_nodes) <= 2:
      graph = add_graph_value(graph, v, u, distance_to_v)
      graph = add_graph_value(graph, u, v, distance_to_v)

  return graph

def gabriel_graph_brute(points, cutoff_distance):
  """
  The new version of the Gabriel Graph algorithm which uses the kd-tree to create the edges, instead of removing them
  """
  graph = {}
  edges = make_delaunay_edges(points)
    
  for edge in edges:
    u = edge[0]
    v = edge[1]
    distance_to_v = find_distance(u, v)
    if cutoff_distance < distance_to_v:
      continue
    mid_point = find_midpoint(u, v)
    mid_dist = find_distance(u, mid_point)
    add = True
    for point in points:
      if not (point == u or point == v):
        p_dist = find_distance(point, mid_point)

        if p_dist <= mid_dist:
          add = False;
          break
      else:
        continue

    if add:
      graph = add_graph_value(graph, v, u, distance_to_v)
      graph = add_graph_value(graph, u, v, distance_to_v)
          
  return graph

def add_graph_value(graph, index1, index2, point_distance):
  if graph.get(index1):
    graph[index1][index2] = point_distance
  else:
    graph[index1] = {index2: point_distance}    
  return graph

def make_delaunay_edges(points):
  delaunay = Delaunay(points)
  
  edges = []
  for i in xrange(delaunay.nsimplex): 
    if i > delaunay.neighbors[i,2]: 
      edges.append((points[delaunay.vertices[i,0]], points[delaunay.vertices[i,1]])) 
    if i > delaunay.neighbors[i,0]: 
      edges.append((points[delaunay.vertices[i,1]], points[delaunay.vertices[i,2]])) 
    if i > delaunay.neighbors[i,1]: 
      edges.append((points[delaunay.vertices[i,2]], points[delaunay.vertices[i,0]])) 

  return edges

def rn_graph_brute(points, cutoff_distance):
  edges = make_delaunay_edges(points)  
  rn_graph_dict = {}

  for edge in edges:
    v = edge[0]
    u = edge[1]
    edge_distance = find_distance(u, v)
    
    if cutoff_distance < edge_distance:
      continue

    add = True
    for point in points:
      if not (point == u or point == v):
        if edge_distance > max(find_distance(v, point), find_distance(u, point)):
          add = False
          break
      else:
        continue
    if add:
      add_graph_value(rn_graph_dict, v, u, edge_distance)
      add_graph_value(rn_graph_dict, u, v, edge_distance)

  return rn_graph_dict

def rn_graph_kdtree(points, kdtree, cutoff_distance):
  edges = make_delaunay_edges(points)
  rn_graph_dict = {}

  for edge in edges:
    v = edge[0]
    u = edge[1]
    edge_distance = find_distance(u, v)
    
    # If the nodes are father apart then we can mannage, then we proceed to the next edge
    if cutoff_distance < edge_distance:
      continue
    
    add = True
    u_points = kdtree.query_ball_point(u, edge_distance)
    v_points = kdtree.query_ball_point(v, edge_distance)

    points_index = u_points
    points_index.extend(v_points)

    for point_index in points_index:
      point = points[point_index]
      if not (point == u or point == v):
        if edge_distance > max(find_distance(v, point), find_distance(u, point)):
          add = False
          break
      else:
        continue
    if add:
      add_graph_value(rn_graph_dict, v, u, edge_distance)
      add_graph_value(rn_graph_dict, u, v, edge_distance)

  return rn_graph_dict  

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

  return (result_graph, set_container) 

def SciPy_KDTree(points):
  """
  The kDTree is used to create the "complete"-moblie ad-hoc graph
  """
  
  tree = KDTree(points)
  return tree

def make_non_planar_graph(points, cutoff_distance, tree):
  # Now we make the actuall graph
  neighbours = tree.query_ball_point(points, cutoff_distance)
 
  for entry_num in range(0, len(points)):
    point = points[entry_num]
    local_neighbours = neighbours[entry_num]
    local_result = {}    
 
    for actual_neighbour in local_neighbours:
      neighbour = points[actual_neighbour]
      
      # if one of our 'neighbours' is the point itself then we skip it
      if neighbour == point:
        continue
      else:
        local_result[neighbour] = find_distance(neighbour, point)
    
    result[point] = local_result

  return (result, tree)
