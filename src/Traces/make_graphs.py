import os, json

latex_location = '../../report/results/graph/'

def save_file(file_name, data):
  file_name += '.tex'
  with open(file_name, mode='w') as f:
    f.write(data)  

def find_percent_points(size, path):
  points = ""
  for i in xrange(10):  
    nodes = (i + 1) * 10
    filename = "%s_results/%s-%s-%s.json" % (path, path, nodes, size)
   
    f = open(filename, 'r')
    json_data = f.read()
    f.close()
    data = json.loads(json_data)
    percent = data[0]

    points += "\t(%s, %s)\n" % (nodes, percent)

  return points

def make_graph(size):
  
  plot = "\\addplot[color=%s, mark=%s] coordinates{\n"
  
  GPSR   = plot % ("blue",  "*")
  GOAFR  = plot % ("black", "triangle*")
  GREEDY = plot % ("red",   "square*")

  GPSR   += find_percent_points(size, "GPSR")
  GOAFR  += find_percent_points(size, "GOAFR")
  GREEDY += find_percent_points(size, "GREEDY")

  GPSR   += "}; \\addlegendentry{GPSR}\n"
  GOAFR  += "}; \\addlegendentry{GOAFR}\n"
  GREEDY += "}; \\addlegendentry{GREEDY}\n"

  axis = "axis"

  xticks = ""
  xtick_vals = ""
  max_val = 101
  for num in xrange(max_val):
    if num > 0 and (num % 1 == 0): 
      xticks += "%s" % num
      if (num % 10 == 0):
        xtick_vals += "$%s$" % num
      else:
        xtick_vals += ""
      if num != max_val - 1:
        xticks     += ", "
        xtick_vals += ", "

  yticks      = ""
  ytick_vals  = ""
  max_val = 101
  for num in xrange(max_val):
    yticks     += "%s" % num
    if (num % 10 == 0):
      ytick_vals += "%s" % num
    if num != max_val - 1:
      yticks     += ", "
      ytick_vals += ", "

  y_left  = "\% of successfully transmitted messages"
  width = "0.8\linewidth"

  graph = "\\begin{tikzpicture}\n"
  graph += "\\pgfplotsset{every axis legend/.append style={at={(0.5,1.03)},anchor=south}}\n"
  graph += "\\begin{%s}[scale only axis, xtick={%s}, xticklabels={%s}, ytick={%s}, yticklabels={%s}, transpose legend, legend columns=2, width=%s, xlabel=Number of nodes in the graph, ylabel=%s, legend cell align=left]\n" % (axis, xticks, xtick_vals, yticks, ytick_vals, width, y_left)
  graph += GPSR
  graph += GOAFR
  graph += GREEDY
  graph += "\\end{%s}\n" % axis
  graph += "\\end{tikzpicture}\n"
    
  save_file(latex_location + "percentage_graph", graph)



for size in [100, 250]:
  make_graph(size)

