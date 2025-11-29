from math import pi,sqrt,sin,cos,atan2
import sys
import os
import numpy as np
import random
import parameters

# starting from the same patch
# try different permutations of connections
# collect the coordinates that each patch ends up with
# some patches will get a larger range of coords than others
# perhaps some patches will have a cluster of coords and some outliers


patches = {}

transformLookup = {}

RADIUS_FACTOR = parameters.QUADMESH_SIZE/2

def ComposeTransforms(t1,t2):
  a = t1[0]*t2[0]+t1[1]*t2[3]
  b = t1[0]*t2[1]+t1[1]*t2[4]
  c = t1[0]*t2[2]+t1[1]*t2[5]+t1[2]
  d = t1[3]*t2[0]+t1[4]*t2[3]
  e = t1[3]*t2[1]+t1[4]*t2[4]
  f = t1[3]*t2[2]+t1[4]*t2[5]+t1[5]
  return (a,b,c,d,e,f)

# For this nice inversion formula see
# https://negativeprobability.blogspot.com/2011/11/affine-transformations-and-their.html
def InvertTransform(t):
  a = t[0]
  b = -t[1]
  d = -t[3]
  e = t[4]

  c = -t[2]*t[0]+t[5]*t[1]
  f = -t[5]*t[0]-t[2]*t[1]
  return (a,b,c,d,e,f)

    
def Distance(x1,y1,x2,y2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))

def AddAngle(a,b):
  r = a+b
  if r>pi:
    return r-2.0*pi
  if r<=-pi:
    return r+2.0*pi
  return r


 
def ScramblePatchCoords(fin):
  f = open(fin)
  
  neighbourMap = {}
  transformMap = {}
  
  for l in f.readlines():
    spl = l.split(" ")
    if spl[0]=='ABS':
      transformMap[(0,0)]=l
    elif spl[0]=='REL':
      patchNum = int(spl[1])
      other = int(float(spl[4]))
      if other not in neighbourMap.keys():
        neighbourMap[other] = set()
      if patchNum not in neighbourMap.keys():
        neighbourMap[patchNum] = set()
      neighbourMap[other].add(patchNum)
      neighbourMap[patchNum].add(other)
      transformMap[(other,patchNum)] = l
      
  f.close()

  order = [(0,0)]
  visited = set([0])
  current = 0
  possibleNext = set(neighbourMap[0])
  
  while len(possibleNext)>0:
    newOne = random.choice(list(possibleNext))
    possibleNext.remove(newOne)
    # Given that we want to visit the newOne, find a transform that starts from one of the visited ones and ends at the new one
    possibleTransforms = [(x,y) for (x,y) in transformMap.keys() if x in visited and y==newOne or y in visited and x==newOne]
    order += [random.choice(possibleTransforms)]
    visited.add(newOne)
    
    neighbours = set(neighbourMap[newOne])
    neighbours = neighbours-visited
    possibleNext.update(neighbours)
          
  return (order,transformMap)
  
def AddPatch(order,transformMap,index):
  global patches
  
  (other,patchNum) = order[index]
  l = transformMap[order[index]]

  spl = l.split(" ")
  if spl[0]=='ABS':
    radius = int(spl[3])*RADIUS_FACTOR
    x,y,a = float(spl[4]),float(spl[5]),float(spl[6])
    patches[patchNum]= (x,y,a,radius,0,False)
    transformLookup[patchNum] = (1,0,0,0,1,0)
  elif spl[0]=='REL':
    flip = (spl[2]=='1')
    radius = int(spl[3])*RADIUS_FACTOR
    ta = float(spl[11])
    tb = float(spl[12])
    tc = float(spl[13])
    td = float(spl[14])
    te = float(spl[15])
    tf = float(spl[16])
      
    if other in transformLookup.keys():
      otherTransform = transformLookup[other]
   
      transform = ComposeTransforms(otherTransform,(ta,tb,tc,td,te,tf))
                        
      (offsetX,offsetY) = (transform[2]-otherTransform[2],transform[5]-otherTransform[5])
        
      locationAngle = atan2(offsetX,offsetY)
      angle = atan2(transform[0],transform[3]) # global orientation of this patch
      distance = sqrt(offsetX*offsetX+offsetY*offsetY)

      if patchNum not in patches.keys():
        transformLookup[patchNum] = transform
        patches[patchNum]= (transform[2],transform[5],0.0,radius,angle,flip)
    elif patchNum in transformLookup.keys():
      patchNumTransform = transformLookup[patchNum]
   
      transform = ComposeTransforms(patchNumTransform,InvertTransform((ta,tb,tc,td,te,tf)))
                        
      (offsetX,offsetY) = (transform[2]-patchNumTransform[2],transform[5]-patchNumTransform[5])
        
      locationAngle = atan2(offsetX,offsetY)
      angle = atan2(transform[0],transform[3]) # global orientation of this patch
      distance = sqrt(offsetX*offsetX+offsetY*offsetY)

      if other not in patches.keys():
        transformLookup[other] = transform
        patches[other]= (transform[2],transform[5],0.0,radius,angle,flip)
    
    else:
      print("Neither patch had transform:"+str(other)+" "+str(patchNum))
      exit(0)
  else:
    print("Unexpected token in LoadPatches:"+str(spl[0]))
    exit(0)
      
  
def truncate(x):
  if x<0.0:
    return 0.0
  if x>1.0:
    return 1.0
  return x

    
if len(sys.argv) not in [1]:
  print("Usage: patchpos_permute")
  exit(0)
  
(order,transformMap) = ScramblePatchCoords("d:/pipelineOutput/patchCoords.txt")

for i in range(0,10):
  AddPatch(order,transformMap,i)

for p in patches.items():
  print(p)
