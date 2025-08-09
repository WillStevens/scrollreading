import torch
import numpy as np
from math import sqrt
import time

print(torch.__version__)

VOL_OFFSET_X,VOL_OFFSET_Y,VOL_OFFSET_Z = 2688,1536,4608
MARGIN = 8
VF_SIZE = 2048

# 0.025 and 0.6 seem to result in close to critical damping - small oscillation.
# (whereas 0.025 and 0.5 converge more slowly, 0.025 and 0.7 oscillate more)
CONNECT_FORCE_CONSTANT = 0.025
FRICTION_FORCE_CONSTANT = 0.6
STEP_SIZE = 3

sizex = 399
sizey = 399

points = torch.zeros((sizex,sizey,3),dtype=torch.float32)
velocity = torch.zeros((sizex,sizey,3),dtype=torch.float32)

active = torch.zeros((sizex,sizey),dtype=torch.bool)
activeMinX = 0
activeMinY = 0
activeMaxX = 0
activeMaxY = 0

seed = (3700.0,2408.0,5632.0,1.0,0.0,0.0,0.0,0.0,1.0)

def MakeActive(x,y):
  global active,activeMinX,activeMinY,activeMaxX,activeMaxY
  active[x,y] = True
  if x<activeMinX:
    activeMinX = x
  elif x>activeMaxX:
    activeMaxX = x
  if y<activeMinY:
    activeMinY = y
  elif y>activeMaxY:
    activeMaxY = y
  
def SetVectorField(x,y):
  return

def GetDistanceAtPoint(x,y,z):
  return 0
  
def InitSeed():
  global points,velocity,active,activeMinX,activeMinY,activeMaxX,activeMaxY

  (midx,midy) = (int(sizex/2),int(sizey/2))
  (activeMinX,activeMinY,activeMaxX,activeMaxY)=(midx,midy,midx,midy)

  points[midx,midy,:] = torch.tensor([seed[0],seed[1],seed[2]])

  # Set coords for all of the points to avoid NaNs during force calculation...
  for x in range(0,sizex):
    for y in range(0,sizey):
      points[x,y,:] = torch.tensor([seed[0]+STEP_SIZE*((y-midy)*seed[3]+(x-midx)*seed[6]),seed[1]+STEP_SIZE*((y-midy)*seed[4]+(x-midx)*seed[7]),seed[2]+STEP_SIZE*((y-midy)*seed[5]+(x-midx)*seed[8])])
  
  # ...But only set 5 points active
  SetVectorField(midx,midy)
  MakeActive(midx,midy)
  SetVectorField(midx,midy+1)
  MakeActive(midx,midy+1)
  SetVectorField(midx,midy-1)
  MakeActive(midx,midy-1)
  SetVectorField(midx+1,midy)
  MakeActive(midx+1,midy)
  SetVectorField(midx-1,midy)
  MakeActive(midx-1,midy)


  
offsets = [(-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1)]

@torch.compile
def ForcesAndMove():
  global points,velocity,active,activeMinX,activeMinY,activeMaxX,activeMaxY
  
  for (xo,yo,expectedDistance) in [(xo,yo,STEP_SIZE*sqrt(xo*xo+yo*yo)) for (xo,yo) in offsets]:
    diff = points[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:] - points[activeMinX+xo:activeMaxX+1+xo,activeMinY+yo:activeMaxY+1+yo,:]

    velocity[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:] += diff * torch.where( torch.logical_and(active[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1],active[activeMinX+xo:activeMaxX+1+xo,activeMinY+yo:activeMaxY+1+yo]), (expectedDistance / torch.linalg.norm(diff,dim=2) - 1) * CONNECT_FORCE_CONSTANT, 0.0)[:,:,None]

  
  points[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:] += velocity[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:]  
  velocity[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:] = velocity[activeMinX:activeMaxX+1,activeMinY:activeMaxY+1,:] * FRICTION_FORCE_CONSTANT


dirVectorLookup = [(1,0),(0,1),(-1,0),(0,-1)]
  
def TryToFill(xp,yp):
  if not active[xp,yp]:
    for i in range(0,4):
      xd = dirVectorLookup[i][0]
      yd = dirVectorLookup[i][1]
      
      (xo1,yo1) = (xp+xd,yp+yd)
      (xo2,yo2) = (xp+xd+xd,yp+yd+yd)

      # corner point (RH)
      (xc1,yc1) = (xp+xd-yd,yp+xd+yd)
      
      # corner point the other side of xo1,yo1  (LH)  
      (xd1,yd1) = (xp+xd+yd,yp+yd-xd)

      # right angles to xo1,yo1 (going right)
      (xa1,ya1) = (xp-yd,yp+xd)

      # If xo2,yo2 is in bound, then no need to check xo1,yo1
      if xo2>=0 and xo2<sizex and yo2>=0 and yo2<sizey and active[xo1,yo1] and active[xo2,yo2] and xc1>=0 and xc1<sizex and yc1>=0 and yc1<sizey and xd1>=0 and xd1<sizex and yd1>=0 and yd1<sizey and active[xc1,yc1] and active[xd1,yd1]:
        # Adjacent point, point beyond that, and points either side of adjacent point are all active
        rp0 = 2*points[xo1,yo1,0] - points[xo2,yo2,0]
        rp1 = 2*points[xo1,yo1,1] - points[xo2,yo2,1]
        rp2 = 2*points[xo1,yo1,2] - points[xo2,yo2,2]
        return (True,rp0,rp1,rp2)

      # If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if xc1>=0 and xc1<sizex and yc1>=0 and yc1<sizey and active[xo1,yo1] and active[xa1,ya1] and active[xc1,yc1]:
        # Get midpoint of xo1,yo1,xa1,ya1
        rp0 = (points[xo1,yo1,0]+points[xa1,ya1,0])/2;
        rp1 = (points[xo1,yo1,1]+points[xa1,ya1,1])/2;
        rp2 = (points[xo1,yo1,2]+points[xa1,ya1,2])/2;
        # Extend from the corner through the midpoint
        rp0 = 2*rp0 - points[xc1,yc1,0];
        rp1 = 2*rp1 - points[xc1,yc1,1];
        rp2 = 2*rp2 - points[xc1,yc1,2];
        return (True,rp0,rp1,rp2)

  return (False,0,0,0);
  
def MakeNewPoints():
  newPts = []
  for x in range(activeMinX-1,activeMaxX+2):
    for y in range(activeMinY-1,activeMaxY+2):
      (success,np0,np1,np2) = TryToFill(x,y)
      if success:
        okayToFill = True
        px = int(np0)
        py = int(np1)
        pz = int(np2)
        if px-VOL_OFFSET_X>=MARGIN and px-VOL_OFFSET_X<VF_SIZE-MARGIN and py-VOL_OFFSET_Y>=MARGIN and py-VOL_OFFSET_Y<VF_SIZE-MARGIN and pz-VOL_OFFSET_Z>=MARGIN and pz-VOL_OFFSET_Z<VF_SIZE-MARGIN:
          if GetDistanceAtPoint(px,py,pz)==0:
            newPts += [(x,y,np0,np1,np2)]
        
  return newPts 

def AddNewPoints(newPts):
  for (x,y,np0,np1,np2) in newPts:
    points[x,y,0] = np0
    points[x,y,1] = np1
    points[x,y,2] = np2
	  
    MakeActive(x,y)
    SetVectorField(x,y);
  
InitSeed()

startTime = time.time_ns()
for i in range(0,150):
  print("--- "+str(i)+" ---")
  print("ForcesAndMove:",end="")
  fstartTime = time.time_ns()
  for j in range(0,100):
    ForcesAndMove()
  fendTime = time.time_ns()
  print(str((fendTime-fstartTime)/1000000.0))  
  print("MakeNewPoints and AddNewPoints:",end="")
  pstartTime = time.time_ns()
  newPts = MakeNewPoints()
  AddNewPoints(newPts) 
  pendTime = time.time_ns()
  print(str((pendTime-pstartTime)/1000000.0))  
  print("%d points added" % len(newPts))
endTime = time.time_ns()

print(str((endTime-startTime)/1000000.0))