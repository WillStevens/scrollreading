import sys
import PIL

if len(sys.argv) != 3:
  print("Usage: mask_add.py <mask> mx my <patch-mask> px py")
  print("Add patch mask into mask")
  exit(-1)
  
mask = PIL.Image.open(sys.argv[1])
maskxy = (int(sys.argv[2]),int(sys.argv[3])) # offset of mask into global coord system
maskEndxy = (maskxy[0]+mask.size[0],maskxy[1]+mask.size[1])

patchMask = PIL.Image.Load(sys.argv[4])
patchxy = (int(sys.argv[5]),int(sys.argv[6])) # offset of patch into global coord system
patchEndxy = (patchxy[0]+patchMask.size[0],patchxy[1]+patchMask.size[1])

newxy = (patchxy[0] if patchxy[0]<maskxy[0] else maskxy[0],patchxy[1] if patchxy[1]<maskxy[1] else maskxy[1])
newEndxy = (patchEndxy[0] if patchEndxy[0]>maskEndxy[0] else maskEndxy[0],patchEndxy[1] if patchEndxy[1]>maskEndxy[1] else maskEndxy[1])

newImageSize = (newEndxy[0]-newxy[0],newEndxy[1]-newxy[1])

newImage = PIL.Image.new(mode="RGB",size=newImageSize)

newImage.paste(mask,box=(maskxy[0]-newxy[0],maskxy[1]-newxy[1]))
newImage.paste(patch,box-(patchxy[0]-newxy[0],pathxy[1]-newxy[1]))

newImage.Save("test.tif")
