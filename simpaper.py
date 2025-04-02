import numpy as np
from math import sqrt
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# We need a 3-d array where the first 2 dimensions correspond to x and y on a sheet of paper, and the 3rd dimension is of length 3, for
# the coordinates of a point

paperSize = (10,10)

SPRING_FORCE_CONSTANT = 0.1
FRICTION_CONSTANT = 0.9

paperPos = np.empty((paperSize[0],paperSize[1],3))
paperVel = np.zeros_like(paperPos)
paperAcc = np.zeros_like(paperPos)

# Initialize the coordinates

for x in range(0,paperSize[0]):
  for y in range(0,paperSize[1]):
    paperPos[x,y,0] = x
    paperPos[x,y,1] = y
    paperPos[x,y,2] = 0

def Forces():
  global  paperAcc, paperVel
  
  paperAcc = np.zeros_like(paperPos)
  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      for xo in range(x-1,x+2):
        for yo in range(y-1,y+2):
          if xo>=0 and xo<paperSize[0]:
            if yo>=0 and yo<paperSize[1]:
              if not (xo == x and yo == y):
                expectedDistance = sqrt( (x-xo)*(x-xo)+(y-yo)*(y-yo) )
                direction = paperPos[x,y,:]-paperPos[xo,yo,:]
                actualDistance = np.linalg.norm(direction)
                direction /= actualDistance
                
                force = (expectedDistance - actualDistance)*SPRING_FORCE_CONSTANT

                forceVector = direction * force
                
                paperAcc[x,y,:] += forceVector
				
  paperVel *= FRICTION_CONSTANT

def PlotIt(i):
        
  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  ax = plt.axes(projection='3d')

  ax.set_xlim([-5,15])
  ax.set_ylim([-5,15])
  ax.set_zlim([-5,5])
  
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
  ca = xa + ya + za

  # To create a scatter graph
  ax.scatter(xa,ya,za, c=ca)

  # turn off/on axis
  plt.axis('on')

  plt.savefig("simpaper_test_%05i.png" % i,bbox_inches='tight')

  plt.close()
  # show the graph
  #plt.show()

                
for i in range(0,40):
  Forces()
  if i<20:
    paperAcc[5,5,2] = 0.01
  paperVel += paperAcc
  paperPos += paperVel   
  PlotIt(i)