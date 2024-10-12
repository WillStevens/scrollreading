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
import random

ptMult = 1024

def LoadExtent(file,dilate):
  with open(file) as csvfile:
    extreader = csv.reader(csvfile)
    for row in extreader:
      extents = [int(s) for s in row]
      if dilate:
        extents = [extents[0]-1,extents[1]-1,extents[2]-1,extents[3]+1,extents[4]+1,extents[5]+1]
    return extents


def LoadVolume(file,dilate):
  global ptMult
  pointset = set()
  with open(file) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [int(s) for s in row]
      # represent each point as an integer
      if dilate:
        for xo in range(-1,2):
          for yo in range(-1,2):
            for zo in range(-1,2):
              pointset.add( ((pt[1]+1+yo)*ptMult+pt[0]+1+xo)*ptMult+pt[2]+1+zo )
      else:
        pointset.add( ((pt[1]+1)*ptMult+pt[0]+1)*ptMult+pt[2]+1 )

  return pointset

memo = {}
def NeighbourTest(p1,p2,ps1,ps1_xz,e1,ps2,ps2_xz,e2):
  global fibreFiles,fibrePointsetsFiles,fibreExtentsFiles
  global memo
  
  if (p1,p2,len(ps1),len(ps2)) in memo:
    print("Memoized")
    return memo[(p1,p2,len(ps1),len(ps2))]
  
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
    memo[(p1,p2,len(ps1),len(ps2))]=0
    return 0
    
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
  
  fibreBonus = 0
  
  if len(common)>0:
    # If the two pointsets have a fibre in common, this increases the score
    ps1Fibres = set()
    ps2Fibres = set()
    for fibreFile in fibreFiles:
      if len(fibrePointsetsFiles[fibreFile].intersection(ps1))>0:
         ps1Fibres.add(fibreFile)
      if len(fibrePointsetsFiles[fibreFile].intersection(ps2))>0:
         ps2Fibres.add(fibreFile)

    fibreBonus = 200 if len(ps1Fibres.intersection(ps2Fibres))>0 else 0

    print("NT %s: l1=%d l2=%d len(common)=%d sqrt(lmin)=%f len(common_xz)=%d fibreBonus=%d (%s,%s)" % (str(r),l1,l2,len(common),sqrtlmin,len(common_xz),fibreBonus,p1,p2))

  if r:
    memo[(p1,p2,len(ps1),len(ps2))]=len(common) + fibreBonus
    return len(common) + fibreBonus
  else:
    memo[(p1,p2,len(ps1),len(ps2))]=0
    return 0
  
def UpdateExtent(a,b):
  if a==[]:
    return b
  if b==[]:
    return a
  return [min(a[0],b[0]),min(a[1],b[1]),min(a[2],b[2]),max(a[3],b[3]),max(a[4],b[4]),max(a[5],b[5])]

if len(sys.argv) != 2 and len(sys.argv) != 3:
  print("Usage: jigsaw2.py <directory> [<fiber-directory>]")
  exit(-1)
  
# This version of jigsaw works by growing one patch at a time, each time looking for the best-fitting piece

print("Loading volumes and fibers")

dir = sys.argv[1]

fibreFiles = []
fibrePointsetsFiles = {}
fibreExtentsFiles = {}
if len(sys.argv)==3:
  fibreDir = sys.argv[2]
  for file in os.listdir(fibreDir):
    if file.endswith(".csv") and file[0]=="x":
      fibreFiles += [file]
      fibrePointsetsFiles[file] = LoadVolume(fibreDir+"/v"+file[1:],False)
      fibreExtentsFiles[file] = LoadExtent(fibreDir+"/"+file,False)
      
files = []
pointsetsFiles = {}
extentsFiles = {}
for file in os.listdir(dir):
  if file.endswith(".csv") and file[0]=="x":
    files += [file]
    pointsetsFiles[file] = LoadVolume(dir+"/v"+file[1:],True)
    extentsFiles[file] = LoadExtent(dir+"/"+file,True)

print("Finished loading volumes and fibers")

# Now sort files in order of descending size
files = sorted(files, key=lambda x : len(pointsetsFiles[x]),reverse=True)

pointsets_xz_Files = {}

for f in files:
  pointsets_xz_Files[f] = set([x % (ptMult*ptMult) for x in pointsetsFiles[f]])

filesProcessed = set()

doneOuter = False  
while not doneOuter:
  # done unless we find any matches
  doneOuter = True

  # iterate through...  
  for prand in files:
    reiterate = True
    while prand not in filesProcessed and reiterate:
      reiterate = False
      
      bestScore = 0
      bestFile=""
      
      # does anything match? if so then pick the best match
      for p in [x for x in files if x not in filesProcessed]:
        if p != prand:
          score = NeighbourTest(p,prand,pointsetsFiles[prand],pointsets_xz_Files[prand],extentsFiles[prand],pointsetsFiles[p],pointsets_xz_Files[p],extentsFiles[p])
          if score>bestScore:
            bestScore = score
            bestFile = p
            
      if bestScore > 0:
        reiterate = True
        doneOuter = False

        print("Merging %s and %s" % (prand,bestFile))
        
        filesProcessed.add(bestFile)
            
        # Merge bestFile with prand
        pointsetsFiles[prand] = pointsetsFiles[prand].union(pointsetsFiles[bestFile])
        pointsets_xz_Files[prand] = pointsets_xz_Files[prand].union(pointsets_xz_Files[bestFile])
        extentsFiles[prand] = UpdateExtent(extentsFiles[prand],extentsFiles[bestFile])

        # Remove bestFile from the dictionaries 
        del pointsetsFiles[bestFile]
        del pointsets_xz_Files[bestFile]
        del extentsFiles[bestFile]
    
    
# Now output everything left as distinct surfaces
outputCount = 0
for p in [x for x in files if x not in filesProcessed]:

  with open(dir + "/output/x"+str(outputCount)+".csv","w") as xout:
    xout.write("%d,%d,%d,%d,%d,%d\n" % (extentsFiles[p][0],extentsFiles[p][1],extentsFiles[p][2],extentsFiles[p][3],extentsFiles[p][4],extentsFiles[p][5]))

  with open(dir + "/output/v"+str(outputCount)+".csv","w") as vout:
    for p in pointsetsFiles[p]:
      z = (p%ptMult)-1
      p = p//ptMult
      x = (p%ptMult)-1
      y = (p//ptMult)-1

      vout.write("%d,%d,%d\n" % (x,y,z))

  outputCount += 1
  
print("Finished")   
