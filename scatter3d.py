import numpy as np
from math import sqrt
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
from PIL import Image
import csv

xList = []
yList = []
zList = []

with open("simpaper_out_cpp//testout_252.csv") as csvfile:
  volreader = csv.reader(csvfile)
  for row in volreader:
    xList += [float(row[0])]
    yList += [float(row[1])]
    zList += [float(row[2])]
        
# Change the Size of Graph using 
# Figsize
fig = plt.figure(figsize=(20, 20))

ax = plt.axes(projection='3d')

ax.set_xlim([-1,256])
ax.set_ylim([-1,256])
ax.set_zlim([-1,256])
        
xa = np.array(yList)
ya = np.array(xList)
za = np.array(zList)

# To create a scatter graph
ax.scatter(xa,ya,za)

# turn off/on axis
plt.axis('on')

plt.savefig("simpaper_out_cpp//simpaper_test_252.png",bbox_inches='tight')

plt.close()
