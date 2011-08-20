import os, re
"""
os.chdir("../Processed Motion/GaussMarkov/")
files = os.listdir(os.getcwd())
m = re.compile("(\d+)-(\d+)-1.0")
for f in files:
  res = m.search(f)
  if res:
    new_name = "GaussMarkov-%s-%s-0.ns_movements" % (res.group(1), res.group(2))
    os.system("mv %s %s" % (f, new_name))

"""
os.chdir("../../../bonnmotion-1.5a/bin/")
size_option = [500, 750]
for size in size_option:
  for j in xrange(10):
    for i in xrange(100):
      nodes = 10 * (i + 1)
      max_speed = 2
      name = "GaussMarkov-%s-%s-%s" % (nodes, size, j)
      if os.path.exists(name):
        continue

      os.system("./bm -f %s GaussMarkov -i 120 -n %s -x %s -y %s -z 0 -d 90 -h %s -u %s" % (name, nodes, size, size, max_speed, 1))

      os.system("./bm NSFile -f %s" % name)
      os.system("mv %s.ns_params ../../src/Motion/Parameters\ for\ Motion/GaussMarkov/." % name)
      os.system("mv %s.params ../../src/Motion/Parameters\ for\ Motion/GaussMarkov/." % name)
      os.system("mv %s.ns_movements ../../src/Motion/Processed\ Motion/GaussMarkov/." % name)
