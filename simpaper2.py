import numpy as np
from math import sqrt
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
from PIL import Image

vectorField = np.load("vectorfield_test_smooth.npy")

"""
# For testing overwrite vectorField with vectors that point towards a diagonal plane
for z in range(0,50):
  for y in range(0,50):
    for x in range(0,50):
      if z<30:
        if z<y:
          vectorField[z,y,x,0] = 0.0
          vectorField[z,y,x,1] = -0.707
          vectorField[z,y,x,2] = 0.707
        elif z>y:
          vectorField[z,y,x,0] = 0.0
          vectorField[z,y,x,1] = 0.707
          vectorField[z,y,x,2] = -0.707
        else:
          vectorField[z,y,x,0] = 0.0
          vectorField[z,y,x,1] = 0.0
          vectorField[z,y,x,2] = 0.0
      else:
        if y<30:
          vectorField[z,y,x,0] = 0.0
          vectorField[z,y,x,1] = 1.0
          vectorField[z,y,x,2] = 0.0
        else:
          vectorField[z,y,x,0] = 0.0
          vectorField[z,y,x,1] = -1.0
          vectorField[z,y,x,2] = 0.0
"""     
paperSize = (51,51)

SPRING_FORCE_CONSTANT = 0.025
FRICTION_CONSTANT = 0.9
GRAVITY_FORCE = 0.01
VECTORFIELD_CONSTANT = 0.05

dirVectorLookup = [(1,0),(0,1),(-1,0),(0,-1)]

# The active array stores which points are currently in use
active = np.zeros((paperSize[0],paperSize[1]),dtype=bool)
mobile = np.ones((paperSize[0],paperSize[1],3))
paperPos = np.empty((paperSize[0],paperSize[1],3))
paperVel = np.zeros_like(paperPos)
paperAcc = np.zeros_like(paperPos)

# Initialize the coordinates of the first active point

paperPos[25,25,0] = 25
paperPos[25,25,1] = 25
paperPos[25,25,2] = 25

paperPos[25,26,0] = 25.7
paperPos[25,26,1] = 24.3
paperPos[25,26,2] = 25

paperPos[26,25,0] = 25
paperPos[26,25,1] = 25
paperPos[26,25,2] = 26

paperPos[26,26,0] = 25.7
paperPos[26,26,1] = 24.3
paperPos[26,26,2] = 26

"""

paperPos[25,25,0] = 25
paperPos[25,25,1] = 25
paperPos[25,25,2] = 25
"""
active[25,25] = True
active[25,26] = True
active[26,25] = True
active[26,26] = True
#mobile[25,25,0] = 0.0
#mobile[25,25,1] = 0.0
#mobile[25,25,2] = 0.0

"""
for xo in range(-1,2):
  for yo in range(-1,2):
    if not (xo==0 and yo==0):
      paperPos[25+xo,25+yo,0] = paperPos[25,25,0]+xo
      paperPos[25+xo,25+yo,1] = paperPos[25,25,1]
      paperPos[25+xo,25+yo,2] = paperPos[25,25,2]+yo
      active[25+xo,25+yo] = True
"""      
# Trilinear interpolation to get vector field value
def VectorFieldAtCoord(x,y,z):
  return vectorField[int(z),int(y),int(x)]
  (x0,y0,z0) = (int(x),int(y),int(z))
  (x1,y1,z1) = (x0+1,y0+1,z0+1)
  if x1>=50:
    x1 = 49
  if y1>=50:
    y1 = 49
  if z1>=50:
    z1 = 49

  (xd,yd,zd)=(x-x0,y-y0,z-z0)
  
  c00 = vectorField[z0,y0,x0,:]*(1-xd)+vectorField[z0,y0,x1,:]*xd
  c01 = vectorField[z1,y0,x0,:]*(1-xd)+vectorField[z1,y0,x1,:]*xd
  c10 = vectorField[z0,y1,x0,:]*(1-xd)+vectorField[z0,y1,x1,:]*xd
  c11 = vectorField[z1,y1,x0,:]*(1-xd)+vectorField[z1,y1,x1,:]*xd
  
  c0 = c00*(1-yd)+c10*yd
  c1 = c01*(1-yd)+c11*yd
  
  return c0*(1-zd)+c1*zd

expectedDistanceLookup = np.empty((3,3))
for x in range(0,3):
  for y in range(0,3):
    expectedDistanceLookup[x,y] = sqrt((x-1)*(x-1)+(y-1)*(y-1))
    
def Forces():
  global  paperAcc, paperVel
  
  paperAcc = np.zeros_like(paperPos)
  largestForce = 0.0
  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      if active[x,y]:
        
        (px,py,pz) = (paperPos[x,y,0],paperPos[x,y,1],paperPos[x,y,2])
        if px>=0 and px<50 and py>=0 and py<50 and pz>=0 and pz<50:
          vForce = VectorFieldAtCoord(px,py,pz)*VECTORFIELD_CONSTANT
          paperAcc[x,y] += vForce
        for xo in range(max(0,x-1),min(x+2,paperSize[0])):
          for yo in range(max(0,y-1),min(y+2,paperSize[1])):
            if active[xo,yo] and not (xo == x and yo == y):
              expectedDistance = expectedDistanceLookup[x-xo+1,y-yo+1]
              direction = paperPos[x,y,:]-paperPos[xo,yo,:]
              actualDistance = np.linalg.norm(direction)
              direction /= actualDistance
                
              force = (expectedDistance - actualDistance)*SPRING_FORCE_CONSTANT

              forceVector = direction * force
                
              paperAcc[x,y,:] += forceVector
        
        forceMag = np.linalg.norm(paperAcc[x,y,:])
        if forceMag > largestForce and (x!=25 or y!=25):
          largestForce = forceMag
          largestForceCoords = (x,y)          
  paperVel *= FRICTION_CONSTANT
#  print("Largest force = %g at %d,%d. largestVForce = %g, largestSpringForce = %g" % (largestForce,largestForceCoords[0],largestForceCoords[1],largestVForce,largestSpringForce))
  return largestForce

def DrawIt(i):
  # Output the points
  pointsOut = np.zeros((50,50,50))

  for x in range(0,50):
    for y in range(0,50):
      pointsOut[25,y,x] = np.linalg.norm(vectorField[25,y,x])/5.0
      pointsOut[10,y,x] = np.linalg.norm(vectorField[10,y,x])/5.0
      pointsOut[40,y,x] = np.linalg.norm(vectorField[40,y,x])/5.0

  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      (px,py,pz) = (paperPos[x,y,0],paperPos[x,y,1],paperPos[x,y,2])
      (px,py,pz) = (int(px),int(py),int(pz))
      if pz>=0 and pz<50 and py>=0 and py<50 and px>=0 and px<50:
        pointsOut[pz,py,px] = 1.0
    

  j = Image.fromarray(pointsOut[25,:,:])
  j.save("simpaper_out\\progress_25_%03d.tif"%i)
  j = Image.fromarray(pointsOut[10,:,:])
  j.save("simpaper_out\\progress_10_%03d.tif"%i)
  j = Image.fromarray(pointsOut[40,:,:])
  j.save("simpaper_out\\progress_40_%03d.tif"%i)
  
def PlotIt(i):
        
  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  ax = plt.axes(projection='3d')

  ax.set_xlim([-1,51])
  ax.set_ylim([-1,51])
  ax.set_zlim([-1,51])
  
  # Creating array points

  
  xList = []
  yList = []
  zList = []
  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      xList += [paperPos[x,y,0]]
      yList += [paperPos[x,y,1]]
      zList += [paperPos[x,y,2]]
      
  xa = np.array(xList)
  ya = np.array(yList)
  za = np.array(zList)

  # To create a scatter graph
  ax.scatter(xa,ya,za)

  """
  ax.plot_surface(paperPos[:,:,0],paperPos[:,:,1],paperPos[:,:,2])
  """
  """
  x = np.linspace(0,50,50)
  y = np.linspace(0,50,50)
  z = np.linspace(0,50,50)
  
  X,Y,Z = np.meshgrid(x,y,z)
  
  ax.quiver(X,Y,Z,vectorField[:,:,:,0],vectorField[:,:,:,1],vectorField[:,:,:,2])
  """
  # turn off/on axis
  plt.axis('on')

  plt.savefig("simpaper_out//simpaper_test_%05i.png" % i,bbox_inches='tight')

  plt.close()
  # show the graph
  #plt.show()

# TODO - if more than one possible coordinate, take the average of all?           
def TryToFill(xp,yp):
  if not active[xp,yp]:
    for (xd,yd) in dirVectorLookup:
      (xo1,xo2) = (xp+xd,xp+xd+xd)
      (yo1,yo2) = (yp+yd,yp+yd+yd)
      # If xo2,yo2 is in bound, then no need to check xo1,yo1
      if xo2>=0 and xo2<paperSize[0] and yo2>=0 and yo2<paperSize[1]:
        if active[xo1,yo1] and active[xo2,yo2]:
          result = 2*paperPos[xo1,yo1] - paperPos[xo2,yo2]
          #print("Growing straight: %.2f,%.2f%.2f and %.2f,%.2f,%.2f to %.2f,%.2f,%.2f" % (paperPos[xo2,yo2,0],paperPos[xo2,yo2,1],paperPos[xo2,yo2,2],paperPos[xo1,yo1,0],paperPos[xo1,yo1,1],paperPos[xo1,yo1,2],result[0],result[1],result[2]))
          return result
      (xa1,ya1) = (xp+yd,yp+xd)
      (xc1,yc1) = (xp+xd+yd,yp+xd+yd)
      # If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if xc1>=0 and xc1<paperSize[0] and yc1>=0 and yc1<paperSize[1]:
        if active[xo1,yo1] and active[xa1,ya1] and active[xc1,yc1]:
          #print("Growing cornder")
          # Get midpoint of xo1,yo1,xa1,ya1
          m = (paperPos[xo1,yo1]+paperPos[xa1,ya1])/2
          # Extend from the corner through the midpoint
          return 2*m - paperPos[xc1,yc1]
  return None

newPts = []
for i in range(0,50):
  print("---")
  j = 0
  while j<10 or Forces()>0.005 and j<50:
    j+=1
  #  if i<20:
  #    paperAcc[5,5,2] = 0.01
    paperVel += paperAcc
    # Only move the ones that aren't anchored
    paperPos += np.multiply(paperVel,mobile)
  """
  for (x,y,t) in newPts:
    mobile[x,y,0] = 0.0
    mobile[x,y,1] = 0.0
    mobile[x,y,2] = 0.0
  """
  newPts = []
  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      t = TryToFill(x,y)
      if t is not None:
        newPts += [(x,y,t)]

  print("%d points added" % len(newPts))     
 
  for (x,y,t) in newPts:
    paperPos[x,y]=t
    active[x,y]=True

  PlotIt(i)
  DrawIt(i)

"""
for i in range(1,100):
  print("---")
  for j in range(0,10):
    Forces()
  #  if i<20:
  #    paperAcc[5,5,2] = 0.01
    paperVel += paperAcc
    # Only move the ones that aren't anchored
    paperPos += np.multiply(paperVel,mobile)
  PlotIt(i)
"""

# Output the points
pointsOut = np.zeros((50,50,50))

f = open("simpaper_points.csv","w")
for x in range(0,paperSize[0]):
  for y in range(0,paperSize[1]):
    (px,py,pz) = (paperPos[x,y,0],paperPos[x,y,1],paperPos[x,y,2])
    f.write("%f,%f,%f\n" % (px,py,pz))
    (px,py,pz) = (int(px),int(py),int(pz))
    if pz>=0 and pz<50 and py>=0 and py<50 and px>=0 and px<50:
      pointsOut[pz,py,px] = 1.0
    
print("Outputting...")
for i in range(0,50):
  j = Image.fromarray(pointsOut[i,:,:])
  j.save("simpaper_out\\pointsout_%03d.tif"%i)

