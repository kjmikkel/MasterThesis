import svgfig
from svgfig import *
import os, math, colorsys, webcolors, copy, re

dir_list = ["test_movement"]
time_point = 6
cutoff_distance = 50
modifier = 1
colour_list = webcolors.css3_names_to_hex.keys()


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

# SVG creation
def svg_graph(name, points, edges, max_x, max_y, min_x, min_y):
    fig = Fig()
    
    for p in points:
        x = p[1][0]
        y = p[1][1]

        c = SVG('circle', cx = x, cy = y, r = 2, fill='black')
        fig.d.append(c)

    for e in edges:
        p1 = e[0][1]
        p2 = e[1][1]
        start_x = p1[0]
        end_x = p2[0]

        start_y = p1[1]
        end_y = p2[1]

        l = Line(start_x, start_y, end_x, end_y, stroke='black')
        fig.d.append(l)
    
    svgfig._canvas_defaults['width'] = "1600px"
    svgfig._canvas_defaults['height'] = "1600px"
    svgfig._canvas_defaults['viewBox'] = str(min_x) + " " + str(min_y) + " " + str(max_x) + " " + str(max_y)
    
    s = SVG("top_level")  
    s.append(fig.SVG())
    s.save(name + ".svg")

def tikz_graph(points, edges):
    s = '\\begin{tikzpicture}[scale=\\scale]\n'
    s += '\\foreach \\pos/\\name in {'
    first = True
    for p in points:
        if not first:
            s += ', '
        first = False
        x = str(p[1][0] / modifier)
        y = str(p[1][1] / modifier)
        s += '{(' + x + ', ' + y + ')' + '/' + p[0] + '}'
    s += '}{\n\t\\node[vertex] (\\name) at \\pos {};\n'
  #  s += '\t\draw[outline] {\\pos circle (' + str(cutoff_distance / modifier) +  ')} node {};\n
    s += '}' 
    
    first = True
    s += '\n\\foreach \\source/\\dest in {'
    for e in edges:
        if not first:
            s += ', '
        first = False
        p1 = e[0][0]
        p2 = e[1][0]
        s += p1 + '/' + p2
        
    s += '} {\n '
    s += '\path[edge] (\\source) -- (\\dest);\n}'
    s += '\n\\end{tikzpicture}'
    return s

def add_neighbour(n_dict, main_pt, sec_pt):
    neighbours = n_dict.get(main_pt)
                
    if neighbours == None:
        n_dict[main_pt] = [sec_pt]
    else:
        neighbours.append(sec_pt)

def gabriel_graph(edges, neighbours_dict):
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

def rng_graph(edges, neighbours_dict):
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

def read_dir(dir_list, name, cutoff_distance):
    for fdir in dir_list:
        point_list = []

        letter = ord('a')
        subfix = 1
        for filename in os.listdir(fdir):
        
            f = open(fdir + '/' + filename)
            lines = f.readlines()
            f.close()

            split = lines[time_point].split(' ')
            x = float(split[0])
            y = float(split[1])
 
            if subfix < 2:
                point_list.append((chr(letter), (x, y)))
            else:
                point_list.append((chr(letter) + str(subfix), (x, y)))

            letter += 1
            if letter > ord('z'):
              letter = ord('a')
              subfix += 1

            make_graph(point_list, name + ' ' + filename)        

def make_graph(point_list, name, cutoff_distance): 
    edge_list = []
    neighbour_dict = {}

    min_x = sys.float_info.max
    min_y = sys.float_info.max

    max_x = sys.float_info.min
    max_y = sys.float_info.min
    
    for entry in point_list:
       
        (x, y) = entry[1]

        min_x = min(min_x, x)
        max_x = max(max_x, x)
        
        min_y = min(min_y, y)
        max_y = max(max_y, y)

    point_index_outer = 0
    point_index_inner = 0
    while point_index_outer < len(point_list):
        outer_point = point_list[point_index_outer]
        
        point_index_inner = point_index_outer + 1        
        while point_index_inner < len(point_list):
            inner_point = point_list[point_index_inner]
            
            euclidian_distance = find_distance(outer_point[1], inner_point[1])
          
            if euclidian_distance <= cutoff_distance:
                
                add_neighbour(neighbour_dict, outer_point, inner_point)
                add_neighbour(neighbour_dict, inner_point, outer_point)

                edge_list.append((outer_point, inner_point))

            point_index_inner += 1
        point_index_outer += 1

    gg_edges = gabriel_graph(copy.deepcopy(edge_list), neighbour_dict)

    rng_edges = rng_graph(copy.deepcopy(edge_list), neighbour_dict)

    start = open('graph-basis.tex', 'r')
    begin = start.read()
    start.close()

    print "***" + str(point_list) + "***"
    exit

    str_graph = tikz_graph(point_list, edge_list)
    str_gg_graph = tikz_graph(point_list, gg_edges)
    str_rng_graph = tikz_graph(point_list, rng_edges)
    
    begin += '\n\n'
    begin += '\\subfloat[The nomral graph]{\label{fig:norm_graph}\n' + str_graph +'}\n'
    begin += '\\subfloat[The Gabriel graph]{\label{fig:gg_graph}\n' + str_gg_graph + '}\n\n'
    begin += '\\subfloat[The RNG graph]{\label{rng_graph}\n' + str_rng_graph + '}'

    save = open('test.tex', 'w')
    save.write(begin)
    save.flush()
    save.close()

    svg_graph('svg/Normal-' + name, point_list, edge_list, max_x, max_y, min_x, min_y)
    svg_graph('svg/Gabriel graph-' + name, point_list, gg_edges, max_x, max_y, min_x, min_y)
    svg_graph('svg/RNG graph-' + name, point_list, rng_edges, max_x, max_y, min_x, min_y)


def make_graph_from_list(str_list, name, cutoff_distance):
  point_list = []
  entries = str_list.split('}')
  print entries
  number = '([-+\s]*[\d.]+)'
  find = re.compile(',?\s*{\(' + number + ',' + number + '\)/(\w+)' )
  for entry in entries:
    if len(entry) > 0:
      result = find.match(entry)
      point_list.append((result.group(3), (float(result.group(1)), float(result.group(2)))))
  
  make_graph(point_list, name, cutoff_distance)  


make_graph_from_list('{(0,2)/a}, {(2,1)/b}, {(-2,1)/e}, {(1,-1)/c}, {(-1,-1)/d}, {(0,3)/f}, {(3,1.5)/g}, {(-3, 1.5)/j}, {(2,-2)/h}, {(-2,-2)/i}', 'peterson', 4)

#read_dir(dir_list, 'test', cutoff_distance)
