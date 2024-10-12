# Will Stevens, September 2024
# 
# Assemble surfaces from patches piece-by-piece.
#
# Surface point lists are in folders call sNNN, named vNNN_X.csv, where X is an arbitrary number that got
# assigned to a surface during processing.
#
# Released under GNU Public License V3

import csv
import os
import time
import sys
from PIL import Image
from math import sqrt

def NeighbourTest(p,ps1,ps1_xz,e1,ps2,ps2_xz,e2):
  (xmin1,ymin1,zmin1,xmax1,ymax1,zmax1) = (e1[0]-1,e1[1]-1,e1[2]-1,e1[3]+1,e1[4]+1,e1[5]+1)
  (xmin2,ymin2,zmin2,xmax2,ymax2,zmax2) = (e2[0]-1,e2[1]-1,e2[2]-1,e2[3]+1,e2[4]+1,e2[5]+1)
  
  overlap = False
  
  if (xmin2 >= xmin1 and xmin2 <= xmax1 or xmax2 >= xmin1 and xmax2 <= xmax1) and \
        (ymin2 >= ymin1 and ymin2 <= ymax1 or ymax2 >= ymin1 and ymax2 <= ymax1) and \
        (zmin2 >= zmin1 and zmin2 <= zmax1 or zmax2 >= zmin1 and zmax2 <= zmax1):
    overlap = True
   
  if (xmin1 >= xmin2 and xmin1 <= xmax2 or xmax1 >= xmin2 and xmax1 <= xmax2) and \
        (ymin1 >= ymin2 and ymin1 <= ymax2 or ymax1 >= ymin2 and ymax1 <= ymax2) and \
        (zmin1 >= zmin2 and zmin1 <= zmax2 or zmax1 >= zmin2 and zmax1 <= zmax2):
    overlap = True

  if not overlap:
    return False

  common = ps1.intersection(ps2)
                
  common_xz = ps1_xz.intersection(ps2_xz)
    
  l1 = len(ps1)
  l2 = len(ps2)
  
  lmin = min(l1,l2)
  
  sqrtlmin = sqrt(lmin)
    
  # These two tests aim to limit the number of intersections that we end up with
  # Only join if the common boundary is more than a value close to the square root of the approx area of the smallest patch, and if the number of intersection voxels is no more than a small constant
  # times the boundary size.
  r = (len(common)>sqrtlmin//2 and len(common_xz) <= len(common))
  
  if len(common)>0:
    print("NT %s: l1=%d l2=%d len(common)=%d sqrt(lmin)=%f len(common_xz)=%d (%s)" % (str(r),l1,l2,len(common),sqrtlmin,len(common_xz),p))

  return r
  
def UpdateExtent(a,b):
  if a==[]:
    return b
  if b==[]:
    return a
  return [min(a[0],b[0]),min(a[1],b[1]),min(a[2],b[2]),max(a[3],b[3]),max(a[4],b[4]),max(a[5],b[5])]

if len(sys.argv) != 2:
  print("Usage: jigsaw.py <directory>")
  exit(-1)
  
d1 = sys.argv[1]

matches = []
      
files1 = []
  
for file in os.listdir(d1):
  if file.endswith(".csv") and file[0]=="x":
    files1 += [file]

extentsFiles1 = {}
for f1 in files1:
  with open(d1+"/"+f1) as csvfile:
    extreader = csv.reader(csvfile)
    for row in extreader:
      extentsFiles1[f1] = [int(s) for s in row]

pointsetsFiles1 = {}
  
ptMult = 1024
print("Loading volumes")
for f1 in files1:
  print(f1)
  pointset1 = set()
  with open(d1+"/v"+f1[1:]) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [int(s) for s in row]
      # represent each point as an integer
      for xo in range(-1,2):
        for yo in range(-1,2):
          for zo in range(-1,2):
            pointset1.add( ((pt[1]+1+yo)*ptMult+pt[0]+1+xo)*ptMult+pt[2]+1+zo )

  pointsetsFiles1[f1] = pointset1

print("Finished loading volumes")

# Now sort files1 in order of descending size
files1 = sorted(files1, key=lambda x : len(pointsetsFiles1[x]),reverse=True)

pointsets_xz_Files1 = {}

for f1 in files1:
  pointsets_xz_Files1[f1] = set([x % (ptMult*ptMult) for x in pointsetsFiles1[f1]])
  
filesProcessed = set()
constructingPointSet = set()
constructingPointSet_xz = set()
constructingExtent = []
dst = Image.new('L', (512,512))
outputCount = 0

catString = "cat "
while len(filesProcessed) != len(files1):
  addedAny = False
  for p in [x for x in files1 if x not in filesProcessed]:
    if len(constructingPointSet)==0 or NeighbourTest(p,constructingPointSet,constructingPointSet_xz,constructingExtent,pointsetsFiles1[p],pointsets_xz_Files1[p],extentsFiles1[p]):
      filesProcessed.add(p)
      constructingPointSet = constructingPointSet.union(pointsetsFiles1[p])
      constructingPointSet_xz = constructingPointSet_xz.union(pointsets_xz_Files1[p])
      constructingExtent = UpdateExtent(constructingExtent,extentsFiles1[p])
      addedAny = True
      
      im = Image.open(d1 + "/v" + p[1:-4] + ".tif")
      mask = im.copy()
      mask = mask.convert('L')
      mask = mask.point( lambda p:255 if p>0 else 0 )
      dst.paste(im, (0, 0), mask = mask)
      
      print("Added " + p)
      catString += d1 + "/v" + p[1:-4] + ".csv "
      
  if not addedAny:
    print("Outputting " + str(outputCount))
    # Output constructingPointSet
    dst.save(d1 + "/output/out_" + str(outputCount) + '.png')
    dst = Image.new('L', (512,512))
    
    catString += " > " + d1 + "/output/v"+str(outputCount)+".csv"
    
    # concatenate all of the contributing patches
    os.system(catString)
    catString = "cat "
    
    # Output the list of points and the extent
#    with open(d1 + "/output/v"+str(outputCount)+".csv","w") as vout:
#      for p in constructingPointSet:
#        z = (p%ptMult)-1
#        p = p//ptMult
#        x = (p%ptMult)-1
#        y = (p//ptMult)-1

#        vout.write("%d,%d,%d\n" % (x,y,z))

    with open(d1 + "/output/x"+str(outputCount)+".csv","w") as xout:
      xout.write("%d,%d,%d,%d,%d,%d\n" % (constructingExtent[0],constructingExtent[1],constructingExtent[2],constructingExtent[3],constructingExtent[4],constructingExtent[5]))
        
    outputCount += 1
    constructingPointSet = set()        
    constructingPointSet_xz = set()        
    constructingExtent = []

print("Finished")   
