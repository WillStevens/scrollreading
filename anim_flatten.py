# Import libraries
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import csv

def PlotIt(i):
  # Load points
  points = set()
  with open("C:\\Users\\Will\\Downloads\\vesuvius\\code\\anim_test_%06d.csv" % i) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [float(s) for s in row]
      points.add( (pt[0],pt[1],pt[2]) )
        

  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  # Generating a 3D sine wave
  ax = plt.axes(projection='3d')

  ax.set_xlim([20,90])
  ax.set_ylim([50,120])
  ax.set_zlim([0,70])
  
  # Creating array points using 
  # numpy
  x = np.array([p[0]/1.0 for p in points])
  y = np.array([p[1]/1.0 for p in points])
  z = np.array([p[2]/1.0 for p in points])
  c = x + y + z

  # To create a scatter graph
  ax.scatter(x-128, z-128, y, c=c)

  # turn off/on axis
  plt.axis('on')

  plt.savefig("anim_test_%06d.png" % i,bbox_inches='tight')

  plt.close()
  # show the graph
  #plt.show()

for i in range(0,12000,100):
  PlotIt(i)
