import os
paths = ['GPSR/', "GREEDY/", "GOAFR/"]
for path in paths:
  listing = os.listdir(path)
  for infile in listing:
    filename = path + infile
    os.system("perl evaluate.pl -f %s" % filename)
