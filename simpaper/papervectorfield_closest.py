import zarr
from PIL import ImageGrab,ImageTk,Image
import numpy as np
from math import sqrt

za = zarr.open(r'D:\1213_aug_erode_threshold-ome.zarr\0',mode='r')

#print(za.shape)

#print(za.attrs.asdict())

# e.g. s02512_03988_01500 : y,x,z

yp = 2323-25
xp = 4206-25
zp = 2522-25

vfSize = 100
vfRad=2

size = vfSize + 2*vfRad + 1

# Index as z,y,x
k = za[zp:zp+size,yp:yp+size,xp:xp+size]

"""
# Make a hole
for zo in range(-5,6):
  for yo in range(-7,8):
    for xo in range(-5,6):
      k[25+zo,38+yo,25+xo]=0
"""
for i in range(0,size):
  tmp = np.array(k[i,vfRad:vfRad+vfSize,vfRad:vfRad+vfSize])
  print(type(tmp))
  print(tmp.shape)
  j = Image.fromarray(tmp)
  j.save("tmp\\papervectorfield_test_surface_%03d.tif"%i)
  
vf = np.zeros((vfSize,vfSize,vfSize,3))

sortedDistances = []
for zo in range(-vfRad,vfRad+1):
  for yo in range(-vfRad,vfRad+1):
    for xo in range(-vfRad,vfRad+1):
      if zo !=0 or yo != 0 or xo != 0:
        dist = sqrt(zo*zo+xo*xo+yo*yo)
        sortedDistances += [(dist,xo,yo,zo,np.array((xo,yo,zo))/dist)]
      
sortedDistances.sort(key = lambda tup : tup[0])
print(sortedDistances)
print("Calculating vector field...")
for z in range(vfRad,size-vfRad-1):
  print(z)
  for y in range(vfRad,size-vfRad-1):
    for x in range(vfRad,size-vfRad-1):
      if k[z,y,x] != 255:
        minVec = []
        lastDist = None
        for (dist,xo,yo,zo,v) in sortedDistances:
          if k[z+zo,y+yo,x+xo] == 255:
            if dist != lastDist:
              if len(minVec)>0:
                break
              lastDist = dist
            minVec += [v]
        """
        minVec = None
        minDist = None
        for zo in range(-vfRad,vfRad+1):
          for yo in range(-vfRad,vfRad+1):
            for xo in range(-vfRad,vfRad+1):
              if k[z+zo,y+yo,x+xo] == 255:
                dist = sqrt(xo*xo+yo*yo+zo*zo)
                if minDist is None or dist < minDist:
                  minDist = dist
                  minVec = [np.array((xo,yo,zo))/dist]
                elif dist==minDist:
                  minVec += [np.array((xo,yo,zo))/dist]
        """       
        if minVec is not None and len(minVec)>0:
          minVec = sum(minVec)/len(minVec)
          vf[z-vfRad,y-vfRad,x-vfRad,0] = minVec[0]
          vf[z-vfRad,y-vfRad,x-vfRad,1] = minVec[1]
          vf[z-vfRad,y-vfRad,x-vfRad,2] = minVec[2]
      
print("Outputting...")
for i in range(0,vfSize):
  j = Image.fromarray(vf[i,:,:,0]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_x_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,1]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_y_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,2]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_z_%03d.tif"%i)

np.save("vectorfield_test_100.npy",vf)
