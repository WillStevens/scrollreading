from math import pi,sqrt,sin,cos,atan2
import sys
import os
import numpy as np
import random
import parameters
import pickle

# Produce a random walk for a specified number of patches, starting from a random patch

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
 
  
def AddPatch(order,transfoms,transformMap,flipState,index):
  global patches
  
  patchNum = order[index]

  if index==0:
    radius = 0 # don't need radius anyway
    x,y,a = 0.0,0.0,0.0
    patches[patchNum]= (x,y,a,0,flipState[patchNum])
    transformLookup[patchNum] = (1,0,0,0,1,0)
  else:
    (other,patchNum) = transforms[index-1]
    l = transformMap[transforms[index-1]]
    spl = l.split(" ")
  
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
        patches[patchNum]= (transform[2],transform[5],0.0,angle,flipState[patchNum])
    elif patchNum in transformLookup.keys():
      patchNumTransform = transformLookup[patchNum]
   
      transform = ComposeTransforms(patchNumTransform,InvertTransform((ta,tb,tc,td,te,tf)))
                        
      (offsetX,offsetY) = (transform[2]-patchNumTransform[2],transform[5]-patchNumTransform[5])
        
      locationAngle = atan2(offsetX,offsetY)
      angle = atan2(transform[0],transform[3]) # global orientation of this patch
      distance = sqrt(offsetX*offsetX+offsetY*offsetY)

      if other not in patches.keys():
        transformLookup[other] = transform
        patches[other]= (transform[2],transform[5],0.0,angle,flipState[other])
    
    else:
      print("Neither patch had transform:"+str(other)+" "+str(patchNum))
      exit(0)
      
def SavePatches(i):
  # print("Saving positions...")
  f = open(sys.argv[1]+"_"+str(i)+".txt","w")
  
  if f:
    for (patchNum,(x,y,a,ga,flip)) in patches.items():
      f.write("%d %f %f %f %d\n" % (patchNum,x,y,AddAngle(a,ga),1 if flip else 0))

  f.close()
  # print("Saved")
      
def truncate(x):
  if x<0.0:
    return 0.0
  if x>1.0:
    return 1.0
  return x

    
if len(sys.argv) not in [8]:
  print("Usage: patch_pairs <patch positions prefix> <neighbourmap> <transformmap> <flipstate> <bad patches> <start patch> <number of patches>")
  exit(0)

with open(sys.argv[2], 'rb') as inp:
  neighbourMap = pickle.load(inp)  
with open(sys.argv[3], 'rb') as inp:
  transformMap = pickle.load(inp)
with open(sys.argv[4], 'rb') as inp:
  flipState = pickle.load(inp)

badPatches = []
with open(sys.argv[5]) as bpFile:
  l=bpFile.readlines()
  badPatches += [int(x) for x in l]

badPatches = set(badPatches)

generatedOrders = []

startPatch = int(sys.argv[6])
numPatches = int(sys.argv[7])

for i in range(startPatch,startPatch+numPatches):
  # Is i actually a patch number?
  if i in neighbourMap.keys() and i not in badPatches:
    for n in neighbourMap[i]:
      if n not in badPatches and i<n:
        # Should be just one
        possibleTransforms = [(x,y) for (x,y) in transformMap.keys() if x==i and y==n or y==i and x==n]
        transforms = [random.choice(possibleTransforms)]
        generatedOrders += [([i,n],transforms)]
  
generatedOrders.sort()

i = 0
for (order,transforms) in generatedOrders:
  patches = {}
  transformLookup = {}

  print(order)
  #print(transforms)

  for j in range(0,len(order)):
    AddPatch(order,transforms,transformMap,flipState,j)

  #for p in patches.items():
  #  print(p)

  SavePatches(i)
  i += 1