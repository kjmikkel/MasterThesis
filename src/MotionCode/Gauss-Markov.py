import os

os.chdir("../../bonnmotion-1.5a/bin/")
size_option = [500, 1000]

for size in size_option:
  for i in xrange(100):
    nodes = 100 * (i + 1)
    name = "GaussMarkov-%s-%s" % (nodes, size)
    os.system("./bm -f %s GaussMarkov -i 60 -n %s -x %s -y %s -z 0 -d 60" % (name, nodes, size, size))
    os.system("./bm NSFile -f %s" % name)
    os.system("mv %s.ns_params ../../src/Parameters\ for\ Motion/GaussMarkov/." % name)
    os.system("mv %s.params ../../src/Parameters\ for\ Motion/GaussMarkov/." % name)
    os.system("mv %s.ns_movements ../../src/Processed\ Motion/GaussMarkov/." % name)

