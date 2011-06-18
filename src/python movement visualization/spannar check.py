from scipy.spatial import KDTree
import cProfile, pstats, random, copy, math

def distance(p1, p2):
  x1 = p1[0]
  x2 = p2[0]

  y1 = p1[1]
  y2 = p2[1]

  dx = abs(x1 - x2)
  dy = abs(y1 - y2)
 
  return math.sqrt(dx * dx + dy * dy)

def doSciPyKDTree(data, cut_off):
  result = {}
  tree = KDTree(data)
  neighbors = tree.query_ball_point(data, cut_off)
  for entry_num in range(0, len(data)):
    point = data[entry_num]
    local_neighbors = neighbors[entry_num]
    local_result = []     
 
    for actual_neighbor in local_neighbors:
      neighbor = data[actual_neighbor]
      local_result.append((neighbor, distance(neighbor, point)))
    
    result[point] = local_result  

  return result

def brute_force(data, cut_off):
  result = {}
  for point in data:
    local_result = []
    
    for compare_point in data:
      if point != compare_point:
	local_distance = distance(point, compare_point)
	if local_distance <= cut_off:
	   local_result.append((compare_point, local_distance))

    result[point] = local_result
  
  return result  

random.seed()

data = []
max_values = 5000
number_of_points = 10000
cut_off = 5

for i in range(0, number_of_points):
  entry = (random.randint(0, max_values), random.randint(0, max_values))
  data.append(entry)

#brute_data = copy.deepcopy(data)
#cProfile.run('brute_force(brute_data, cut_off)', sort=1)

k_data = copy.deepcopy(data)
cProfile.run('doSciPyKDTree(k_data, cut_off)', sort=1)
