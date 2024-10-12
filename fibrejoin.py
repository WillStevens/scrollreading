# Will Stevens, October 2024
# 
# Assemble surfaces based on common fibres
#
# Released under GNU Public License V3

import csv
import os
import time
import sys
from PIL import Image
from math import sqrt

def NeighbourTest(ps1,e1,ps2,e2):
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

  return len(ps1.intersection(ps2))>0

if len(sys.argv) != 3:
  print("Usage: fibrejoin.py <surface-directory> <fibre-directoy")
  exit(-1)
  
d1 = sys.argv[1]
d2 = sys.argv[2]
      
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
      pointset1.add( ((pt[1]+1)*ptMult+pt[0]+1)*ptMult+pt[2]+1 )

  pointsetsFiles1[f1] = pointset1

files2 = []
  
for file in os.listdir(d2):
  if file.endswith(".csv") and file[0]=="x":
    files2 += [file]

extentsFiles2 = {}
for f2 in files2:
  with open(d2+"/"+f2) as csvfile:
    extreader = csv.reader(csvfile)
    for row in extreader:
      extentsFiles2[f2] = [int(s) for s in row]

pointsetsFiles2 = {}
  
ptMult = 1024
print("Loading volumes")
for f2 in files2:
  print(f2)
  pointset2 = set()
  with open(d2+"/v"+f2[1:]) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [int(s) for s in row]
      pointset2.add( ((pt[1]+1)*ptMult+pt[0]+1)*ptMult+pt[2]+1 )

  pointsetsFiles2[f2] = pointset2
    
print("Finished loading volumes")

# For each fibre, see what it intersects with
for f2 in files2:
  print("Interesctor of fibre " + str(f2))
  for f1 in files1:
    if NeighbourTest(pointsetsFiles1[f1],extentsFiles1[f1],pointsetsFiles2[f2],extentsFiles2[f2]):
      print(f1)
    
print("Finished")   
