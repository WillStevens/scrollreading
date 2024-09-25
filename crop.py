import sys
import csv

if len(sys.argv) != 8:
  print("Usage: crop.py <file> x1 y1 z1 x2 y2 z2")
  exit(-1)
  
file = sys.argv[1]
xmin = int(sys.argv[2])
ymin = int(sys.argv[3])
zmin = int(sys.argv[4])
  
xmax = int(sys.argv[5])
ymax = int(sys.argv[6])
zmax = int(sys.argv[7])

with open(sys.argv[1]) as csvfile:
  volreader = csv.reader(csvfile)
  for row in volreader:
    pt = [int(s) for s in row]
    if pt[0]>=xmin and pt[0]<=xmax and pt[1]>=ymin and pt[1]<=ymax and pt[2]>=zmin and pt[2]<=zmax:
      print("%d,%d,%d" % (pt[0],pt[1],pt[2]))