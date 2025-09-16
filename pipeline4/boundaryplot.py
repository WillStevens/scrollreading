import numpy as np
from math import sqrt
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D

def PlotPoints(xList,yList,zList,i):
        
  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  ax = plt.axes(projection='3d')

  ax.set_xlim([3400,5100])
  ax.set_ylim([1600,3300])
  ax.set_zlim([5150,6850])
  
  # Creating array points
      
  xa = np.array(xList)
  ya = np.array(yList)
  za = np.array(zList)
  ca = xa + ya + za

  # To create a scatter graph
  ax.scatter(xa,ya,za, c=ca)

  plt.axis('on')

  plt.savefig("d:/pipelineOutput/boundary.anim/boundary_%05d.png" % i,bbox_inches='tight')

  plt.close()

for i in range(0,57):
  f = open("d:/pipelineOutput/boundary_"+str(i)+".txt")
  
  xList,yList,zList = ([],[],[])

  for l in f.readlines():
    if len(l)>4:
      spl = l.split(" ")
      xList += [float(spl[2])]
      yList += [float(spl[3])]
      zList += [float(spl[4])]

  PlotPoints(xList,yList,zList,i)
