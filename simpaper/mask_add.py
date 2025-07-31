from PIL import Image

def MaskAdd(mask,maskxy,patch,patchxy):
  maskEndxy = (maskxy[0]+mask.size[0],maskxy[1]+mask.size[1])

  patchEndxy = (patchxy[0]+patch.size[0],patchxy[1]+patch.size[1])

  newxy = (patchxy[0] if patchxy[0]<maskxy[0] else maskxy[0],patchxy[1] if patchxy[1]<maskxy[1] else maskxy[1])
  newEndxy = (patchEndxy[0] if patchEndxy[0]>maskEndxy[0] else maskEndxy[0],patchEndxy[1] if patchEndxy[1]>maskEndxy[1] else maskEndxy[1])

  newImageSize = (newEndxy[0]-newxy[0],newEndxy[1]-newxy[1])

  newImage = Image.new(mode="RGB",size=newImageSize)

  newImage.paste(mask,box=(maskxy[0]-newxy[0],maskxy[1]-newxy[1]))
  newImage.paste(patch,box=(patchxy[0]-newxy[0],patchxy[1]-newxy[1]),mask=patch)

  return (newImage,newxy)