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
patchIndexLookup = {}

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

def AffineTxToXYA(ta,tb,tc,td,te,tf):
  return (atan2(td,ta),tc,tf)
  
def Distance(x1,y1,x2,y2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))

def Distance3D(x1,y1,z1,x2,y2,z2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1))

def AddAngle(a,b):
  r = a+b
  if r>pi:
    return r-2.0*pi
  if r<=-pi:
    return r+2.0*pi
  return r

def LoadPatches(alignmentOrder):
  global patches, patchVel, patchAcc,connections, patchIndexLookup
  patchNums = set()
  patches = []
  patchVel = []
  patchAcc = []
  connections = []
  
  badPatch = False
  newPatch = True
  for l in alignmentOrder:
    radius = float(l[1])
    other = int(l[2])
    (ta,tb,tc,td,te,tf) = [float(x) for x in l[3:]]
    
    if other==-1:
      patchNum = int(l[0])
      (x,y,a) = AffineTxToXYA(ta,tb,tc,td,te,tf)
      patches += [(x,y,a,radius,0,x,y,a)]
      patchVel += [(0,0,0)]
      patchAcc += [(0,0,0)]
      transformLookup[patchNum] = (1,0,0,0,1,0)
      patchIndexLookup[patchNum]=len(patches)-1
      patchNums.add(patchNum)
    else:
      if int(l[0]) != patchNum:
        badPatch = False
        newPatch = True
      else:
        newPatch = False
      if badPatch:
        continue
        
      patchNum = int(l[0])
      if patchNum not in patchNums and len(patchNums)==patchLimit:
        print("Stopped loading when %d encountered" % patchNum)
        print(l)
        print(f"Max patch number:{max(patchNums)}")

        return
      
      if other in transformLookup.keys():
        otherTransform = transformLookup[other]

        transform = ComposeTransforms(otherTransform,(ta,tb,tc,td,te,tf))
                
        #print((transform[2],transform[5]))        
        
        (offsetX,offsetY) = (transform[2]-otherTransform[2],transform[5]-otherTransform[5])
        
        locationAngle = atan2(offsetY,offsetX)
        angle = atan2(transform[3],transform[0]) # global orientation of this patch
        distance = sqrt(offsetX*offsetX+offsetY*offsetY)

        #print("Location angle is %f (%f degress)" % (locationAngle,locationAngle*360/(2.0*pi)))
        #print("Angle is %f (%f degress)" % (angle,angle*360/(2.0*pi)))
        #print("Distance is %f" % distance)
      
        #print(str(other))
        #print(patchIndexLookup)
        otherIndex = patchIndexLookup[other]
        #print("len patches="+str(len(patches))+" otherIndex="+str(otherIndex))

        if patchNum not in patchNums:
          transformLookup[patchNum] = transform # this had previous been before the if statement, which could have led to inconsistent results
          patchNums.add(patchNum)
          patchIndexLookup[patchNum] = len(patches)
          patches += [(transform[2],transform[5],0.0,radius,angle,transform[2],transform[5],0.0)]
          patchVel += [(0,0,0)]
          patchAcc += [(0,0,0)]
        else:
          newDistance = Distance(patches[-1][0],patches[-1][1],transform[2],transform[5])
          print("Patchnum %d, other %d, distance %f" % (patchNum,other,newDistance))
          if newDistance>MAX_CORRECTABLE_DISTANCE:
            print("Patch %d is a bad patch\n" % patchNum)
            badPatch = True
            while connections[-1][0]==len(patches)-1:
              connections = connections[:-1]
            patches = patches[:-1]
            del patchIndexLookup[patchNum]
        if not badPatch:
          connections += [(len(patches)-1,otherIndex,distance,
                           AddAngle(pi,locationAngle),
                           locationAngle)] 
      else:
        print("Patch %d can't find other patch %d (perhaps other was a bad patch)" % (patchNum,other))
       
  print(f"Max patch number:{max(patchNums)}")
  
  
    
def SavePatches():
  print("Saving positions...")
  f = open(parameters.OUTPUT_DIR+"/patchPositions.txt","w")
  
  if f:
    for patchNum,patchIndex in patchIndexLookup.items():
      (x,y,a,r,ga) = patches[patchIndex][0:5]
      f.write("%d %f %f %f\n" % (patchNum,x,y,AddAngle(a,ga)))
      
  f.close()
  print("Saved")
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1,r1,ga1) = patches[p1][0:5]  
    (x2,y2,a2,r2,ga2) = patches[p2][0:5]

    currentAngle12 = AddAngle(patches[p1][2],angle1)      
    currentAngle21 = AddAngle(patches[p2][2],angle2)      
    
    (reqx2,reqy2) = (x1+cos(currentAngle12)*dist,y1+sin(currentAngle12)*dist)
    (reqx1,reqy1) = (x2+cos(currentAngle21)*dist,y2+sin(currentAngle21)*dist)
          
    
    actualAngle12 = atan2(y2-y1,x2-x1)
    adiff1 = AddAngle(actualAngle12,-currentAngle12) 

    actualAngle21 = atan2(y1-y2,x1-x2)
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

# Two patches should not be nearer in 2D space than they are in 3D space - apply a replusive force if so
def ExclusionForcesV1():
  global patches,patchAcc,patchVolCoords, patchIndexLookup

  acounter = 0  
  #Naive implementation - look at every pair of patches

  patchNums = list(patchIndexLookup.keys())

  for i in range(0,len(patchNums)):
    for j in range(i+1,len(patchNums)):
      iindex = patchIndexLookup[patchNums[i]]
      jindex = patchIndexLookup[patchNums[j]]     
      ipos2D = patches[iindex][0:2]
      jpos2D = patches[jindex][0:2]
      ipos3D = patchVolCoords[patchNums[i]]
      jpos3D = patchVolCoords[patchNums[j]]
      dist2D = Distance(ipos2D[0],ipos2D[1],jpos2D[0],jpos2D[1])*parameters.QUADMESH_SIZE
      dist3D = Distance3D(ipos3D[0],ipos3D[1],ipos3D[2],jpos3D[0],jpos3D[1],jpos3D[2])

      if dist2D>0 and dist2D < dist3D:
        acounter+=1
        force = (dist3D-dist2D)*0.001

        #print(f"Exclusion force {force} between {patchNums[i]} and {patchNums[j]}. 2D={dist2D}, 3D={dist3D}")
        
        forceVector = ((ipos2D[0]-jpos2D[0])*force/dist2D,(ipos2D[1]-jpos2D[1])*force/dist2D);

        patchAcc[iindex] = (patchAcc[iindex][0]+forceVector[0],patchAcc[iindex][1]+forceVector[1],patchAcc[iindex][2])
        patchAcc[jindex] = (patchAcc[jindex][0]-forceVector[0],patchAcc[jindex][1]-forceVector[1],patchAcc[jindex][2])

  print(f"Number found={acounter}")
		
# Two patches should not be nearer in 2D space than they are in 3D space - apply a replusive force if so
def ExclusionForces():
  global patches,patchAcc,patchVolCoords, patchIndexLookup
  
  acounter = 0
  
  #More efficient implementation
  uGridSizeX = 10
  uGridSizeY = 10

  # map from (ux//uGridSizeX,uy//uGridSizeY) to [(uminx,uminy,umaxx,umaxy),(vminx,vminy,vminz,vmaxx,vmaxy,vmaxz),{patchNum}]
  gridLookup = {}
  
  patchNums = list(patchIndexLookup.keys())

  for i in range(0,len(patchNums)):
    iindex = patchIndexLookup[patchNums[i]]
    ipos2D = patches[iindex][0:2]
    ipos3D = patchVolCoords[patchNums[i]]
  
    gridKey = (ipos2D[0]//uGridSizeX,ipos2D[1]//uGridSizeY)
  
    if gridKey not in gridLookup.keys():
      gridLookup[gridKey] = [(ipos2D[0],ipos2D[1],ipos2D[0],ipos2D[1]),(ipos3D[0],ipos3D[1],ipos3D[2],ipos3D[0],ipos3D[1],ipos3D[2]),set([patchNums[i]])]
    else:
      current = gridLookup[gridKey][0:2]
      if ipos2D[0]<current[0][0]:
        current = [(ipos2D[0],current[0][1],current[0][2],current[0][3]),current[1]]
      if ipos2D[0]>current[0][2]:
        current = [(current[0][0],current[0][1],ipos2D[0],current[0][3]),current[1]]
      if ipos2D[1]<current[0][1]:
        current = [(current[0][0],ipos2D[1],current[0][2],current[0][3]),current[1]]
      if ipos2D[1]>current[0][3]:
        current = [(current[0][0],current[0][1],current[0][2],ipos2D[1]),current[1]]

      if ipos3D[0]<current[1][0]:
        current = [current[0],(ipos3D[0],current[1][1],current[1][2],current[1][3],current[1][4],current[1][5])]
      if ipos3D[0]>current[1][3]:
        current = [current[0],(current[1][0],current[1][1],current[1][2],ipos3D[0],current[1][4],current[1][5])]
      if ipos3D[1]<current[1][1]:
        current = [current[0],(current[1][0],ipos3D[1],current[1][2],current[1][3],current[1][4],current[1][5])]
      if ipos3D[1]>current[1][4]:
        current = [current[0],(current[1][0],current[1][1],current[1][2],current[1][3],ipos3D[1],current[1][5])]
      if ipos3D[2]<current[1][2]:
        current = [current[0],(current[1][0],current[1][1],ipos3D[2],current[1][3],current[1][4],current[1][5])]
      if ipos3D[2]>current[1][5]:
        current = [current[0],(current[1][0],current[1][1],current[1][2],current[1][3],current[1][4],ipos3D[2])]

      gridLookup[gridKey][0:2] = current		
      gridLookup[gridKey][2].add(patchNums[i])
      
  gridKeys = list(gridLookup.keys())
  
  for i in range(0,len(gridKeys)):  
    for j in range(i,len(gridKeys)):
      cell1 = gridLookup[gridKeys[i]]
      cell2 = gridLookup[gridKeys[j]]
    
      # work out minimum 2D distance and max 3D distance between cells

      deltax2D=max(0,cell2[0][0]-cell1[0][2],cell1[0][0]-cell2[0][2])
      deltay2D=max(0,cell2[0][1]-cell1[0][3],cell1[0][1]-cell2[0][3])
          
      dmin2D=sqrt(deltax2D*deltax2D+deltay2D*deltay2D)
	  
      deltax3D=max(cell1[1][3],cell2[1][3])-min(cell1[1][0],cell2[1][0])
      deltay3D=max(cell1[1][4],cell2[1][4])-min(cell1[1][1],cell2[1][1])
      deltaz3D=max(cell1[1][5],cell2[1][5])-min(cell1[1][2],cell2[1][2])
          
      dmax3D=sqrt(deltax3D*deltax3D+deltay3D*deltay3D+deltaz3D*deltaz3D)
      
      if dmin2D*parameters.QUADMESH_SIZE < dmax3D:
       for id in cell1[2]:
        for jd in cell2[2]:
          if gridKeys[i]!=gridKeys[j] or jd>id:
            iindex = patchIndexLookup[id]     
            jindex = patchIndexLookup[jd]     
            ipos2D = patches[iindex][0:2]
            jpos2D = patches[jindex][0:2]
            ipos3D = patchVolCoords[id]
            jpos3D = patchVolCoords[jd]
            dist2D = Distance(ipos2D[0],ipos2D[1],jpos2D[0],jpos2D[1])*parameters.QUADMESH_SIZE
            dist3D = Distance3D(ipos3D[0],ipos3D[1],ipos3D[2],jpos3D[0],jpos3D[1],jpos3D[2])

            if dist2D>0 and dist2D < dist3D:
              acounter += 1
              force = (dist3D-dist2D)*0.001

              #print(f"Exclusion force {force} between {patchNums[i]} and {patchNums[j]}. 2D={dist2D}, 3D={dist3D}")
        
              forceVector = ((ipos2D[0]-jpos2D[0])*force/dist2D,(ipos2D[1]-jpos2D[1])*force/dist2D);

              patchAcc[iindex] = (patchAcc[iindex][0]+forceVector[0],patchAcc[iindex][1]+forceVector[1],patchAcc[iindex][2])
              patchAcc[jindex] = (patchAcc[jindex][0]-forceVector[0],patchAcc[jindex][1]-forceVector[1],patchAcc[jindex][2])
        
  print(f"Number found={acounter}")
  
def Move():
  global patches,patchVel,patchAcc,connections

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2],patches[i][3],patches[i][4],patches[i][5],patches[i][6],patches[i][7])
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
  offset=(600,300)
  scale=0.2
  rotate=-1.2
  patchi = 0
  patchesLen = len(patches)
  for (xo,yo,a,rad,ga,da,db,dc) in patches:
    x=xo*cos(rotate)-yo*sin(rotate)
    y=yo*cos(rotate)+xo*sin(rotate)
    a+=rotate
    
    patchif = pi*float(patchi)/float(patchesLen)
    
    red = 1.0+cos(patchif)
    green = 1.0-cos(patchif)
    blue = 1.0*sin(patchif)
    (red,green,blue)=(truncate(red/2),truncate(green/2),truncate(blue))
    
    rgb = from_rgb((int(red*255),int(green*255),int(blue*255)))
    
    patchi+=1
    
    #(y,x)=(x,y)
    canvas.create_oval(offset[0]+x*scale-rad*scale,offset[1]+y*scale-rad*scale,offset[0]+x*scale+rad*scale,offset[1]+y*scale+rad*scale, fill=rgb)
    #canvas.create_line(offset[0]+x*scale[0],offset[1]+y*scale[1],offset[0]+x*scale[0]+rad*sin(a),offset[1]+y*scale[1]+rad*cos(a))
  if links:
    for (p0,p1,dist,a0,a1) in connections:
      xo,yo,a,rad,ga = patches[p0][0:5]
      x=xo*cos(rotate)-yo*sin(rotate)
      y=yo*cos(rotate)+xo*sin(rotate)
      a+=rotate
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a0)*scale,offset[1]+y*scale+rad*0.8*sin(a+a0)*scale)
      xo,yo,a,rad,ga = patches[p1][0:5]
      x=xo*cos(rotate)-yo*sin(rotate)
      y=yo*cos(rotate)+xo*sin(rotate)
      a+=rotate
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a1)*scale,offset[1]+y*scale+rad*0.8*sin(a+a1)*scale)
  
def RunIteration():
  global iterationCount
  ConnectionForces()
  #ExclusionForces()
  Move()
  
  iterationCount += 1
  
  if iterationCount == 50:
    SavePatches()
    movements = []
    for patchNum,patchIndex in patchIndexLookup.items():
      (xo,yo,a,rad,ga,orig_x,orig_y,orig_a)=patches[patchIndex]
      movements += [(patchNum,Distance(orig_x,orig_y,xo,yo))]
      
    movements = sorted(movements,key=lambda x:x[1])
    
    for (patchNum,distance) in movements[-100:]:    
      print(f"Patch {patchNum} moved {distance}")
    
  Show()    
  window.after(10,RunIteration)
  
if len(sys.argv) not in [4]:
  print("Usage: patchsprings.py <alignmentOrder> <patchVolCoords> <N>")
  print("alignmentOrder is a file containing the relationships between all patches to be placed, ordered so that a patch has at least one already-placed neighbour at initial placement")
  exit(0)

# TODO Load transforms from rel.csv file
alignmentOrder = []
with open(sys.argv[1],"r") as f:
  for l in f.readlines():
    alignmentOrder += [l.split(' ')[:-1]]

patchVolCoords = {}
with open(sys.argv[2],"r") as f:
  for l in f.readlines():
    row = l.split(',')
    if len(row)==4:
      patchVolCoords[int(row[0])] = (float(row[1]),float(row[2]),float(row[3]))

print("patchVolCoords")   
print(patchVolCoords)

patchLimit = int(sys.argv[3])
  
#exit(0)
window = tk.Tk()
window.geometry('1350x700')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=1350, height=700, bg='white')
canvas.pack()

LoadPatches(alignmentOrder)

#print(patches)

window.after(50,RunIteration)

window.mainloop()
    
