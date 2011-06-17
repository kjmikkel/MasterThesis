import os

time = '300'
wait = '360'
nodes = '30'
width = '10000'
height = '10000'

incident_zone = '100, 100, 200, 100, 200, 200, 0, 10, 0'
wait_treat = '20, 20, 20, 40, 40, 40, 40, 20, 1, 10, 0'
clearing_station = '400, 400, 400, 300, 200, 300, 2, 10, 0'

os.system('../bonnmotion-1.5a/bin/bm -f disasterScenario DisasterArea -d ' + time + ' -i ' + wait + ' -n ' + nodes + ' -x ' + width + ' -y ' + height + ' -e 3 -b ' + incident_zone +' -b ' + wait_treat + ' -b ' + clearing_station)
