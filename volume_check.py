# Look for common points between two volumes, and check number of overlapping x,z coords

import sys
import csv

if len(sys.argv) != 3:
  print("Usage: volume_check.py <vol 1> <vol 2>")
  exit(1)

def LoadVolume(v):  
  pointset = set()
  pointset_xz = set()
  with open(v) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [int(s) for s in row]
      pointset.add( (pt[0],pt[1],pt[2]) )
      pointset_xz.add( (pt[0],pt[2]) )

  return (pointset,pointset_xz)
  
(ps1,ps1_xz) = LoadVolume(sys.argv[1])
(ps2,ps2_xz) = LoadVolume(sys.argv[2])
    
common = ps1.intersection(ps2)
common_xz = ps1_xz.intersection(ps2_xz)
  
print("Common points: " + str(len(common)))
print("Common projectedpoints: " + str(len(common_xz)))