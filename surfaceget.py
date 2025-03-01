import zarr
from PIL import ImageGrab,ImageTk,Image

za = zarr.open(r'D:\1213_aug_erode_threshold-ome.zarr\0',mode='r')

#print(za.shape)

#print(za.attrs.asdict())

# e.g. s02512_03988_01500 : y,x,z

yp = 2512
xp = 3477
zp = 2522

# Index as z,y,x

k = za[zp:zp+512,yp:yp+512,xp:xp+512]

zSize = 512

for i in range(0,zSize):
  j = Image.fromarray(k[i])
  j.save("d:/scroll1_surfaces/02512_03477_02522/%05d.tif" % (i))

for z in range(0,512):
  for x in range(0,512):
    count=0
    y=0
    for s in k[z,:,x]:
      if s==255:
        count+=1
        if count<=2:
          print("%d,%d,%d" % (x,y,z))
      else:
        count=0	    
      y+=1
	  
#    for y in range(0,512):
#      if k[z,y,x]==255:
#        count+=1
#        if count<=2:
#          print("%d,%d,%d" % (x,y,z))
#      else:
#        count=0