import zarr
from PIL import ImageGrab,ImageTk,Image

# This zarr has coordinates X=1961-5393, y=2135-5280, z=7000-11249

# I want to extract x=3400, y=4200 z=7336

(xOffset,yOffset,zOffset)=(1961,2135,7000)
 
ySlice = 0

(xWant,yWant,zWant)=(3400,4200+ySlice,7336)

(xSize,ySize,zSize)=(512,512,512) 
 
z = zarr.open('https://dl.ash2txt.org/community-uploads/bruniss/Fiber%20Following/Fiber%20Predictions/7000_11249_predictions.zarr/',mode='r')

#print(z.shape)

#print(z.attrs.asdict())

k = z[zWant-zOffset:zWant-zOffset+zSize, yWant-yOffset:yWant-yOffset+ySize, xWant-xOffset:xWant-xOffset+xSize]

print(type(k))

for i in range(0,zSize):
  j = Image.fromarray(k[i])
  j.save("../test/%05d.tif" % (i))
 
#print(k[0:10,0:10,0:10])
# iterate through all, picking out the distinct fibres

#fibrePoints = {}

#for z in range(0,zSize):
#  print(z)
#  for y in range(0,ySize):
#    for x in range(0,xSize):
#      v = k[z,y,x]
#      if v != 0:
#        if v not in fibrePoints:
#          fibrePoints[v] = []
#        fibrePoints[v] += [(x,y+ySlice,z)]		  

#for k in fibrePoints.keys():
#  print("%d : %d" % (k,len(fibrePoints[k])))
#print(fibrePoints.keys())


#dict_keys([155548, 216204, 215787, 155704, 216303, 216060, 216179, 155773, 216345, 155326, 216012, 215653, 215648, 215591, 215873, 215983, 215714, 155650, 215821, 215599, 215497, 215479, 155731, 215967, 155235, 216005, 155665, 155632, 215569, 216059, 215965, 215788, 215920, 215877, 215557, 216032, 216834, 156875, 156761, 216497, 216965, 156843, 217220, 216665, 216766, 216676, 216757, 216975, 216954, 156938, 156863, 217155, 217157, 216732, 216609, 156732, 217061, 216692, 216471, 216475, 156436, 216817, 216780, 216951, 216635, 216509, 216527, 217191, 217051, 217010, 156834, 217047, 216470, 216896, 216569, 156942, 217170, 156809, 216905, 217147, 217196, 156817, 216529, 216532, 216640, 217120, 217138, 216832, 156814, 156877, 156888, 156884, 217004, 217025, 217181, 217184, 216784, 216952, 216637, 216702, 156836, 156406, 216867, 216879, 216865, 156849, 217109])

#for (x,y,z) in fibrePoints[1]:
#  print("[%d,%d,%d]," %(x,y,z))
  
