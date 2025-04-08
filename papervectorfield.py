import zarr
from PIL import ImageGrab,ImageTk,Image
import numpy as np

za = zarr.open(r'D:\1213_aug_erode_threshold-ome.zarr\0',mode='r')

#print(za.shape)

#print(za.attrs.asdict())

# e.g. s02512_03988_01500 : y,x,z

yp = 2512
xp = 3477
zp = 2522

vfSize = 50
vfRad=3

size = vfSize + 2*vfRad + 1

# Index as z,y,x
k = za[zp:zp+size,yp:yp+size,xp:xp+size]

for i in range(0,size):
  tmp = np.array(k[i,vfRad:vfRad+vfSize,vfRad:vfRad+vfSize])
  print(type(tmp))
  print(tmp.shape)
  j = Image.fromarray(tmp)
  j.save("tmp\\papervectorfield_test_surface_%03d.tif"%i)

# Make an array of vectors that can be mutiplied by a mask and then summed
vectorTemplate = np.empty((vfRad*2+1,vfRad*2+1,vfRad*2+1,3))
for z in range(0,vfRad*2+1):
  for y in range(0,vfRad*2+1):
    for x in range(0,vfRad*2+1):
      v = np.array([x-vfRad,y-vfRad,z-vfRad])
      norm = np.linalg.norm(v)
	  # Scale so that those nearest 0,0 count most
      scale = 1.0/norm if norm > 0.0 else 0.0
      vectorTemplate[z,y,x,0] = scale*(x-vfRad)/norm if norm>0.0 else 0.0
      vectorTemplate[z,y,x,1] = scale*(y-vfRad)/norm if norm>0.0 else 0.0
      vectorTemplate[z,y,x,2] = scale*(z-vfRad)/norm if norm>0.0 else 0.0
	  
vf = np.empty((vfSize,vfSize,vfSize,3))

print("Calculating vector field...")
for z in range(vfRad,size-vfRad-1):
  print(z)
  for y in range(vfRad,size-vfRad-1):
    for x in range(vfRad,size-vfRad-1):
      vx = np.sum(np.multiply(vectorTemplate[:,:,:,0],k[z-vfRad:z+vfRad+1,y-vfRad:y+vfRad+1,x-vfRad:x+vfRad+1]//255))
      vy = np.sum(np.multiply(vectorTemplate[:,:,:,1],k[z-vfRad:z+vfRad+1,y-vfRad:y+vfRad+1,x-vfRad:x+vfRad+1]//255))
      vz = np.sum(np.multiply(vectorTemplate[:,:,:,2],k[z-vfRad:z+vfRad+1,y-vfRad:y+vfRad+1,x-vfRad:x+vfRad+1]//255))
      vf[z-vfRad,y-vfRad,x-vfRad,0] = vx
      vf[z-vfRad,y-vfRad,x-vfRad,1] = vy
      vf[z-vfRad,y-vfRad,x-vfRad,2] = vz
      #print(v)
      
print("Outputting...")
for i in range(0,vfSize):
  j = Image.fromarray(vf[i,:,:,0]/40.0+0.5)
  j.save("tmp\\papervectorfield_test_x_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,1]/40.0+0.5)
  j.save("tmp\\papervectorfield_test_y_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,2]/40.0+0.5)
  j.save("tmp\\papervectorfield_test_z_%03d.tif"%i)

np.save("vectorfield_test.npy",vf)