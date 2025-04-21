import zarr
from PIL import ImageGrab,ImageTk,Image
import numpy as np
from math import sqrt

vectorField = np.load("vectorfield_test_100.npy")
vf = np.empty_like(vectorField)

vfSize = 100
window = 1

print("Calculating vector field...")
for z in range(0,vfSize):
  print(z)
  for y in range(0,vfSize):
    for x in range(0,vfSize):
      vSum = np.zeros_like(vectorField[z,y,x])
      tot = 0
      for zo in range(max(z-window,0),min(z+window+1,vfSize)):
        for yo in range(max(y-window,0),min(y+window+1,vfSize)):
          for xo in range(max(x-window,0),min(x+window+1,vfSize)):
            vSum += vectorField[zo,yo,xo]
            tot += 1
      vf[z,y,x]=vSum/tot

np.save("vectorfield_test_smooth_100.npy",vf)