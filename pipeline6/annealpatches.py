from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk
import sys
from PIL import Image
import copy
import random
 
import parameters

iterationCount = 0

# x,y,orientation,radius,angle (global orientation of patch)
patches = [(50,50,0,20,0),(50,100,0,20,0),(100,50,0,20,0),(100,100,0,20,0)]
patchVel = [(0,0,0),(0,0,0),(0,0,0),(0,0,0)]
patchAcc = [(0,0,0),(0,0,0),(0,0,0),(0,0,0)]

# patch0, patch1, distance, angle0, angle1
connections = [(0,1,50,0.0,pi),
               (0,2,50,pi/2.0,-pi/2.0),
               (1,3,50,pi/2.0,-pi/2.0),
               (2,3,50,0.2,-pi+0.2)]



CONNECT_FORCE_CONSTANT = 0.01
ANGLE_FORCE_CONSTANT = 0.01
FRICTION_CONSTANT = 0.60
ANGLE_FRICTION_CONSTANT = 0.60

# Factor for converting interations into approximate patch radius
#RADIUS_FACTOR = parameters.QUADMESH_SIZE/2.0
RADIUS_FACTOR = 4

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

def MakeTransform(offset):
  (x,y,angle) = offset
  return (cos(angle),-sin(angle),x,sin(angle),cos(angle),y)
  
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
  image = Image.open("d:/pipelineOutput/patch_%d.tif" % patchNum).convert("L")
  mask = Image.open("d:/pipelineOutput/patch_%d.tifm" % patchNum).convert("L")

  # Turn image and mask into a single image with transparency
  rgba = Image.merge("RGBA",(image,image,image,mask))
  
  rgba.save("d:/pipelineOutput/patch_%d.png" % patchNum)

def SavePatchCoords(sortedPatchLinks,state,fout):
  (includes,offsets,bonds) = state

  f = open(fout,"w")
  
  for (other,patchNum,l) in sortedPatchLinks:
    spl = l.split(" ")
    if other==patchNum:
      f.write("ABS "+str(patchNum)+" 0 "+" ".join(spl[3:])+"\n")
    else:
      if (other,patchNum) not in bonds.keys() or patchNum not in includes.keys() or other not in includes.keys():
        continue
      ta = float(spl[11])
      tb = float(spl[12])
      tc = float(spl[13])
      td = float(spl[14])
      te = float(spl[15])
      tf = float(spl[16])
      
      transform = ComposeTransforms((ta,tb,tc,td,te,tf),MakeTransform(offsets[patchNum]))

      f.write(" ".join(spl[:-6])+" " + " ".join([str(x) for x in transform])+"\n")

      
def SortPatchCoords(fin):
  """ Sort the patch coords so that the neighbours of 0 are added first, then neighbours of next lowest etc..."""
  f = open(fin)

  outlines = []
  
  for l in f.readlines():
    spl = l.split(" ")
    if spl[0]=='ABS':
      outlines += [(0,0,l)]
    elif spl[0]=='REL':
      patchNum = int(spl[1])
      other = int(float(spl[4]))
      outlines += [(other,patchNum,l)]
 
  f.close()
  outlines.sort()

  return outlines
  
def InitialiseState(links):
  # includes = map from patch num to boolean 
  # offsets = map from patch num to (x,y,angle) offsets
  # bonds = map from (a,b) to boolean indicating whether or not a bond should be used

  includes = {}
  offsets = {}
  bonds = {}
  
  for (other,patchNum,l) in links:
    spl = l.split(" ")
    if spl[0]=='ABS':
      includes[patchNum] = True
      offsets[patchNum] = (0.0,0.0,0.0)
    elif spl[0]=='REL':      
      if patchNum not in includes.keys():
        includes[patchNum] = True
      if patchNum not in offsets.keys():
        offsets[patchNum] = (0.0,0.0,0.0)
      if (other,patchNum) not in bonds.keys():
        bonds[(other,patchNum)] = True
  return (includes,offsets,bonds)  
    
def MutateState(state,temperature):
  (includes,offsets,bonds) = copy.deepcopy(state)
  RATE_FACTOR = 1
  INCLUDE_MUTATE_RATE = 0.00002*RATE_FACTOR
  BOND_MUTATE_RATE = 0.00002*RATE_FACTOR
  OFFSET_MUTATE_RATE = 0.001*RATE_FACTOR
  OFFSET_MUTATE_SCALE = 10.0
  OFFSET_MUTATE_ANGLE_SCALE = 0.1
  
  for k in includes.keys():
    if random.random()<INCLUDE_MUTATE_RATE*temperature and k!=0:
      includes[k] = not includes[k] 
      if includes[k]:
        offsets[k] = (0.0,0.0,0.0)
        
  for k in bonds.keys():
    if random.random()<BOND_MUTATE_RATE*temperature:
      bonds[k] = not bonds[k]   
      
  for k in offsets.keys():
    if random.random()<OFFSET_MUTATE_RATE*temperature:
      offsets[k] = (offsets[k][0]+(random.random()-0.5)*OFFSET_MUTATE_SCALE*temperature,offsets[k][1]+(random.random()-0.5)*OFFSET_MUTATE_SCALE*temperature,AddAngle(offsets[k][2],(random.random()-0.5)*OFFSET_MUTATE_ANGLE_SCALE*temperature))  
      
  return (includes,offsets,bonds)
  
def ScoreState(links,state):
  (includes,offsets,bonds) = state
  patches = {}
  connections = []
  transformLookup = {}

  patchesAdded = 0
  bondsAdded = 0
  largestDist = 0
  
  MISSING_COST = 1000.0
  DISTANCE_COST = 1.0
  MISSING_BOND_COST = 200.0
  error = 0.0

  for (other,patchNum,l) in links:
    spl = l.split(" ")
    if spl[0]=='ABS':
      radius = int(spl[3])*RADIUS_FACTOR
      x,y,a = float(spl[4]),float(spl[5]),float(spl[6])
      x,y,a = x+offsets[patchNum][0],y+offsets[patchNum][1],a+offsets[patchNum][2]
      patches[patchNum]= [(x,y,a,radius,0)]
      transformLookup[patchNum] = MakeTransform(offsets[patchNum])
      patchesAdded += 1
    elif spl[0]=='REL':        
      if not includes[patchNum] or not includes[other]:
        continue
      if not bonds[(other,patchNum)]:
        continue
      radius = int(spl[3])*RADIUS_FACTOR
      ta = float(spl[11])
      tb = float(spl[12])
      tc = float(spl[13])
      td = float(spl[14])
      te = float(spl[15])
      tf = float(spl[16])
      
      if other not in transformLookup.keys():
        continue

      otherTransform = transformLookup[other]

      transform = ComposeTransforms(ComposeTransforms(otherTransform,(ta,tb,tc,td,te,tf)),MakeTransform(offsets[patchNum]))
        
                
      (offsetX,offsetY) = (transform[2]-otherTransform[2],transform[5]-otherTransform[5])
        
      locationAngle = atan2(offsetX,offsetY)
      angle = atan2(transform[0],transform[3]) # global orientation of this patch
      distance = sqrt(offsetX*offsetX+offsetY*offsetY)

      if patchNum not in patches.keys():
        patches[patchNum]= (transform[2],transform[5],0.0,radius,angle)
        transformLookup[patchNum] = transform
                  
        patchesAdded+=1
        bondsAdded+=1
      else:
        dist = Distance(patches[patchNum][0],patches[patchNum][1],transform[2],transform[5])

        if dist>largestDist:
          largestDist = dist

#       if dist>50:
#          print("From %d, patch %d is out by %f" % (other,patchNum,dist))
        error += dist*DISTANCE_COST
        bondsAdded+=1
    else:
      printf("Unexpected tokan in LoadPatches:"+str(spl[0]))
      exit(0)

  for k in offsets.keys():
    if includes[k]:
      dist = Distance(offsets[k][0],0,offsets[k][1],0)
      error += dist*DISTANCE_COST
        
    if dist>largestDist:
      largestDist = dist
      
#  print(str(len(bonds))+" "+str(bondsAdded))
  print("Missing patches:%06d, missing bonds %06d, largest distance: %06d" % (len(includes)-patchesAdded,len(bonds)-bondsAdded,largestDist)) 
  error += (len(includes)-patchesAdded)*MISSING_COST
  error += (len(bonds)-bondsAdded)*MISSING_BOND_COST  
  return error
  
def SavePatches():
  print("Saving positions...")
  f = open("d:/pipelineOutput/patchPositions.txt","w")
  
  if f:
    for (patchNum,patchIndex) in patchIndexLookup.items():
      (x,y,a,r,ga) = patches[patchIndex]
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

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2],patches[i][3],patches[i][4])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*ANGLE_FRICTION_CONSTANT)
    patchAcc[i] = (0.0,0.0,0.0)
  patchAcc[0] = (0.0,0.0,0.0)
  
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
  offset=(700,300)
  scale=0.03
  patchi = 0
  patchesLen = len(patches)
  for (x,y,a,rad,ga) in patches:
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

def RunGrowShow():
  global patchesToShow,filenameIndex
  LoadPatches(patchesToShow,False)

  for i in range(0,50):  
    ConnectionForces()
    Move()
  
  Show(False)
  filename = "d:/pipelineOutput/patchgrowanim/patches_%06d"%filenameIndex
  canvas.postscript(file=filename+".eps",colormode='color')
  img = Image.open(filename + '.eps') 
  img.save(filename + '.png', 'png') 
  
  filenameIndex += 1
  if patchesToShow < patchLimit:
    patchesToShow += 2
  window.after(10,RunGrowShow)

sortedPatchLinks = SortPatchCoords("d:/pipelineOutputTry20/patchCoords.txt")
# Initialise the state - a tuple of include,offsets,bonds
# includes = boolean vector with length equal to number of patches
# offset = vector of (x,y,angle) offsets with  length equal to number of patches
# bonds = map from (a,b) to boolean indicating whether or not a bond should be used
state = InitialiseState(sortedPatchLinks)
currentScore = ScoreState(sortedPatchLinks,state)

temperature = 3.0
for i in range(0,2000):
  print("Score:%07f, temp:%07f" % (currentScore,temperature))
  newState = MutateState(state,temperature)
  newScore = ScoreState(sortedPatchLinks,newState)
  if newScore < currentScore:
    state = newState
    currentScore = newScore
  temperature = temperature * 0.9995
print(currentScore)

SavePatchCoords(sortedPatchLinks,state,"d:/pipelineOutputTry20/patchCoordsAnneal.txt")
exit(0)
  
if len(sys.argv) not in [2,3]:
  print("Usage: patchsprings.py <number of patches> <patch detail? 0 or 1>")
  exit(0)
  
if len(sys.argv)>1:
  patchLimit = int(sys.argv[1])

renderPatches = len(sys.argv)>2 and int(sys.argv[2])==1
  
#exit(0)
window = tk.Tk()
window.geometry('1200x700')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=1200, height=700, bg='white')
canvas.pack()

if True:
  LoadPatches(patchLimit,renderPatches)
  window.after(50,RunIteration)
else:
  filenameIndex = 0
  patchesToShow = 10
  window.after(50,RunGrowShow)

window.mainloop()
    
