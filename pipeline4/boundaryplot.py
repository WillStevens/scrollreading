import numpy as np
from math import sqrt,cos,sin
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D

def PlotPoints(xList,yList,zList,i):
        
  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  ax = plt.axes(projection='3d')

  ax.set_xlim([3600,5300])
  ax.set_ylim([1800,3500])
  ax.set_zlim([4800,6500])
  
  # Creating array points
      
  xa = np.array(xList)
  ya = np.array(yList)
  za = np.array(zList)
  ca = xa + ya + za

  # To create a scatter graph
  ax.scatter(xa,ya,za, c=ca)

  plt.axis('on')

  plt.savefig("d:/pipelineOutput/boundary.anim2/boundary_%05d.png" % i,bbox_inches='tight')

  plt.close()


outi = 0
rangeset = list(range(0,38))
for i in rangeset:
  f = open("d:/pipelineOutput/boundary_"+str(i)+".txt")
  
  xList,yList,zList = ([],[],[])

  xmid = 4000
  ymid = 2800
  zmid = (4800+6500)/2
  theta = (1.0-cos(3.141592*float(i)/38.0))*2.0
  for l in f.readlines():
    if len(l)>4:
      spl = l.split(" ")
      x = float(spl[2])
      y = float(spl[3])
      z = float(spl[4])
      
      """
      x = x-xmid
      y = y-ymid
      z = z-zmid
      
      xd = x*cos(theta)+y*sin(theta)+xmid
      yd = y*cos(theta)-x*sin(theta)+ymid
      zd = z+zmid
      """
      (xd,yd,zd)=(x,y,z)
      
      xList += [float(xd)]
      yList += [float(yd)]
      zList += [float(zd)]
  PlotPoints(xList,yList,zList,outi)
  outi+=1
