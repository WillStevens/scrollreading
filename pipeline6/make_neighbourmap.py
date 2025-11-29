from math import pi,sqrt,sin,cos,atan2
import sys
import os
import numpy as np
import random
import parameters
import pickle
 
def MakeNeighbourMap(fin):
  f = open(fin)
  
  neighbourMap = {}
  transformMap = {}
  flipState = {}
  
  for l in f.readlines():
    spl = l.split(" ")
    if spl[0]=='ABS':
      patchNum = int(spl[1])
      flipState[patchNum] = False
    elif spl[0]=='REL':
      patchNum = int(spl[1])
      other = int(float(spl[4]))
      flipState[patchNum] = (int(spl[2])==1)
      if other not in neighbourMap.keys():
        neighbourMap[other] = set()
      if patchNum not in neighbourMap.keys():
        neighbourMap[patchNum] = set()
      neighbourMap[other].add(patchNum)
      neighbourMap[patchNum].add(other)
      transformMap[(other,patchNum)] = l
      
  f.close()

  return (neighbourMap,transformMap,flipState)
  	  
    
if len(sys.argv) not in [5]:
  print("Usage: make_neighbourmap <patch coords> <neighbour map> <transform map> <flipstate>")
  exit(0)
  
(neighbourMap,transformMap,flipState) = MakeNeighbourMap(sys.argv[1])

with open(sys.argv[2], 'wb') as outp:
  pickle.dump(neighbourMap,outp)

with open(sys.argv[3], 'wb') as outp:
  pickle.dump(transformMap,outp)

with open(sys.argv[4], 'wb') as outp:
  pickle.dump(flipState,outp)
