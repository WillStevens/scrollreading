from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk
import sys
from PIL import Image
import pickle
import random

import parameters

iterationCount = 0

patches = {}
patchVel = {}
patchAcc = {}
connections = {}

transformLookup = {}

CONNECT_FORCE_CONSTANT = 0.01
ANGLE_FORCE_CONSTANT = 0.01
FRICTION_CONSTANT = 0.60
ANGLE_FRICTION_CONSTANT = 0.60

# Factor for converting interations into approximate patch radius
RADIUS_FACTOR = parameters.QUADMESH_SIZE/2.0

MAX_CORRECTABLE_DISTANCE = 3500

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

def LoadPatchImage(patchNum):
  image = Image.open(parameters.OUTPUT_DIR+"/patch_%d.tif" % patchNum).convert("L")
  mask = Image.open(parameters.OUTPUT_DIR+"/patch_%d.tifm" % patchNum).convert("L")

  # Turn image and mask into a single image with transparency
  rgba = Image.merge("RGBA",(image,image,image,mask))
  
  rgba.save(parameters.OUTPUT_DIR+"/patch_%d.png" % patchNum)

def SelectNext(next):
  minNew = next[0][1]
  i = 0
  ri = 0
  for i in range(0,len(next)):
    if next[i][1]<minNew:
      minNew = next[i][1]
      ri = i
      
  return (next[:ri]+next[ri+1:],next[ri][0],next[ri][1])
  
def GeneratePatchOrder(neighbourMap,transformMap,size):
  global badPatches
  
  current = 0
  order = [current]
  transforms = []
  visited = set([current])
  next = [(current,x) for x in set(neighbourMap[current])-badPatches-visited]
  visited.update(set([y for (x,y) in next]))
  print(next)
  while len(next)>0 and len(order)<size:
    (next,current,newOne) = SelectNext(next)
    # Find transform from current to newOne - there should only be one
    possibleTransforms = [(x,y) for (x,y) in transformMap.keys() if x==current and y==newOne or y==current and x==newOne]
    transforms += [random.choice(possibleTransforms)]
    order += [newOne]
    current = newOne
    next += [(current,x) for x in set(neighbourMap[current])-badPatches-visited]
    visited.update(set([y for (x,y) in next]))
          
  return (order,transforms,transformMap)
  
def AddPatch(order,transfoms,transformMap,flipState,index):
  global patches,patchVel,patchAcc,connections
  
  patchNum = order[index]

  if index==0:
    radius = 254.0 # This will be wrong - TODO fix it
    x,y,a = 0.0,0.0,0.0
    patches[patchNum]= (x,y,a,radius,0.0,flipState[patchNum])
    patchVel[patchNum] = (0,0,0)
    patchAcc[patchNum] = (0,0,0)
    transformLookup[patchNum] = (1,0,0,0,1,0)
  else:
    (other,patchNum) = transforms[index-1]
    l = transformMap[transforms[index-1]]
    print((other,patchNum))
    #print(l)
    spl = l.split(" ")

    radius = int(spl[3])*RADIUS_FACTOR  
    flip = flipState[patchNum]
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
        patchVel[patchNum] = (0,0,0)
        patchAcc[patchNum] = (0,0,0)
        
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
        patchVel[other] = (0,0,0)
        patchAcc[other] = (0,0,0)
    
    else:
      print("Neither patch had transform:"+str(other)+" "+str(patchNum))
      exit(0)
    
def SavePatches():
  print("Saving positions...")
  f = open(parameters.OUTPUT_DIR+"/patchPositions.txt","w")
  
  if f:
    for (patchNum,(x,y,a,r,ga,flip)) in patches.items():
      f.write("%d %f %f %f\n" % (patchNum,x,y,AddAngle(a,ga)))

  f.close()
  print("Saved")
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1,r1,ga1) = patches[p1]  
    (x2,y2,a2,r2,ga2) = patches[p2]

    currentAngle12 = AddAngle(patches[p1][2],angle1)      
    currentAngle21 = AddAngle(patches[p2][2],angle2)      
    
    (reqx2,reqy2) = (x1+sin(currentAngle12)*dist,y1+cos(currentAngle12)*dist)
    (reqx1,reqy1) = (x2+sin(currentAngle21)*dist,y2+cos(currentAngle21)*dist)
          
    
    actualAngle12 = atan2(x2-x1,y2-y1)
    adiff1 = AddAngle(actualAngle12,-currentAngle12) 

    actualAngle21 = atan2(x1-x2,y1-y2)
    adiff2 = AddAngle(actualAngle21,-currentAngle21) 

    #print("%f %f %f %f" % (x1,y1,x2,y2))
    #print("%f %f %f %f" % (reqx1,reqy1,reqx2,reqy2))
    #print("%f %f" % (actualAngle12,actualAngle21))
    #print("%f %f" % (currentAngle12,currentAngle21))
        
    patchAcc[p1] = ( patchAcc[p1][0]+(reqx1-x1)*CONNECT_FORCE_CONSTANT-(reqx2-x2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][1]+(reqy1-y1)*CONNECT_FORCE_CONSTANT-(reqy2-y2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][2]+adiff1*ANGLE_FORCE_CONSTANT-adiff2*ANGLE_FORCE_CONSTANT)
    patchAcc[p2] = ( patchAcc[p2][0]+(reqx2-x2)*CONNECT_FORCE_CONSTANT-(reqx1-x1)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p2][1]+(reqy2-y2)*CONNECT_FORCE_CONSTANT-(reqy1-y1)*CONNECT_FORCE_CONSTANT, 
                     patchAcc[p2][2]+adiff2*ANGLE_FORCE_CONSTANT-adiff1*ANGLE_FORCE_CONSTANT)
                     
    #print(patchAcc[p1])
    #print(patchAcc[p2])

def Move():
  global patches,patchVel,patchAcc,connections

  for i in patches.keys():
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2],patches[i][3],patches[i][4],patches[i][5])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*ANGLE_FRICTION_CONSTANT)
    patchAcc[i] = (0.0,0.0,0.0)

  
def from_rgb(rgb):
    """translates an rgb tuple of int to a tkinter friendly color code
    """
    return "#%02x%02x%02x" % rgb 
    
def truncate(x):
  if x<0.0:
    return 0.0
  if x>1.0:
    return 1.0
  return x

def Show(links=True):
  canvas.delete('all')
  offset=(850,300)
  scale=0.1
  patchi = 0
  patchesLen = len(patches)
  for (patchNum,(x,y,a,rad,ga,flip)) in patches.items():
    patchif = pi*float(patchi)/float(patchesLen)
    
    red = 1.0+cos(patchif)
    green = 1.0-cos(patchif)
    blue = 1.0*sin(patchif)
    (red,green,blue)=(truncate(red/2),truncate(green/2),truncate(blue))
    
    rgb = from_rgb((int(red*255),int(green*255),int(blue*255)))
    
    patchi+=1
    
    (y,x)=(x,y)
    canvas.create_oval(offset[0]+x*scale-rad*scale,offset[1]+y*scale-rad*scale,offset[0]+x*scale+rad*scale,offset[1]+y*scale+rad*scale, fill=rgb)
    #canvas.create_line(offset[0]+x*scale[0],offset[1]+y*scale[1],offset[0]+x*scale[0]+rad*sin(a),offset[1]+y*scale[1]+rad*cos(a))
  if links:
    for (p0,p1,dist,a0,a1) in connections:
      y,x,a,rad,ga = patches[p0]
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a0)*scale,offset[1]+y*scale+rad*0.8*sin(a+a0)*scale)
      y,x,a,rad,ga = patches[p1]
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a1)*scale,offset[1]+y*scale+rad*0.8*sin(a+a1)*scale)
  
def RunIteration():
  global iterationCount
  ConnectionForces()
  Move()
  
  iterationCount += 1
  
  if iterationCount == 50:
    SavePatches()
  Show()    
  window.after(10,RunIteration)

def CallAddPatch():
  global order,transforms,transformMap,addPatchIndex
  
  for i in range(0,10):
    AddPatch(order,transforms,transformMap,flipState,addPatchIndex)
  
    addPatchIndex += 1
  Show()
  window.after(10,CallAddPatch)
  

  
if len(sys.argv) not in [6]:
  print("Usage: patchsprings.py <neighbourmap> <transformmap> <flipstate> <bad patches> <N>")
  exit(0)

with open(sys.argv[1], 'rb') as inp:
  neighbourMap = pickle.load(inp)  
with open(sys.argv[2], 'rb') as inp:
  transformMap = pickle.load(inp)
with open(sys.argv[3], 'rb') as inp:
  flipState = pickle.load(inp)

badPatches = []
with open(sys.argv[4]) as bpFile:
  l=bpFile.readlines()
  badPatches += [int(x) for x in l]

badPatches = set(badPatches)
  
patchLimit = int(sys.argv[5])
  
#exit(0)
window = tk.Tk()
window.geometry('1350x700')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=1350, height=700, bg='white')
canvas.pack()

(order,transforms,transformMap) = GeneratePatchOrder(neighbourMap,transformMap,patchLimit)

print(len(order))

addPatchIndex = 0
window.after(50,CallAddPatch)

window.mainloop()

#for j in range(0,len(order)):
#  AddPatch(order,transforms,transformMap,flipState,j)

#print(patches)

#window.after(50,RunIteration)

#window.mainloop()
    
