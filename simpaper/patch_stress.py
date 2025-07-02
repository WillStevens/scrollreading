# Output stress plot from a patch represented as x,y,z tiffs

from PIL import Image
import numpy as np
from math import sqrt

GRID_SPACING = 20
SCALE_FACTOR = 1.5 # Strain energy scale factor for stress.tif

x = Image.open("x.tif")
y = Image.open("y.tif")
z = Image.open("z.tif")

x = np.asarray(x)
y = np.asarray(y)
z = np.asarray(z)

paperSize = (x.shape[0],x.shape[1])

paperPos = np.empty((paperSize[0],paperSize[1],3))
active = np.zeros((paperSize[0],paperSize[1]),dtype=bool)
stress = np.empty((paperSize[0],paperSize[1]))

for xi in range(0,x.shape[0]):
  for yi in range(0,x.shape[1]):
    xc = x[xi,yi]
    yc = y[xi,yi]
    zc = z[xi,yi]
    
    if xc>0 and yc>0 and zc>0:
      #print("%d,%d,%f,%f,%f" % (xi,yi,xc,yc,zc))
      paperPos[xi,yi][0] = xc
      paperPos[xi,yi][1] = yc
      paperPos[xi,yi][2] = zc
      active[xi,yi] = True
	  
def Distance(x,y,z):
  return sqrt(x*x+y*y+z*z)
  
for xi in range(0,x.shape[0]):
  for yi in range(0,x.shape[1]):
    stress[xi,yi]=1.0
    SETot,SENum = (0,0)
    for xo in [-1,0,1]:	  
      for yo in [-1,0,1]:	  
        (xio,yio)=(xi+xo,yi+yo)
        if xio>=0 and xio<x.shape[0] and yio>=0 and yio<x.shape[1] and active[xi,yi] and active[xio,yio] and not (xi==xio and yi==yio):
          dist = Distance(paperPos[xi,yi][0]-paperPos[xio,yio][0],paperPos[xi,yi][1]-paperPos[xio,yio][1],paperPos[xi,yi][2]-paperPos[xio,yio][2])
          distExp = Distance(float(xo),float(yo),0)*GRID_SPACING
          force = dist-distExp
          SETot += force*force
          SENum += 1		  
    if SENum>0:
      stress[xi,yi] = (SETot/SENum)*SCALE_FACTOR
	  
stressPlot = Image.fromarray(stress)
stressPlot.save("stress.tif")