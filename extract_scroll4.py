from pathlib import Path
import numpy
from PIL import Image

for (y,x,z) in [(2200,700,5000),(2200,700+512,5000),(2200,700+1024,5000),(2200,700+1536,5000)]:
  sx = '0'*(5-len(str(x)))+str(x)
  sy = '0'*(5-len(str(y)))+str(y)
  sz = '0'*(5-len(str(z)))+str(z)
  
  dir = 'd:/construct/scroll4/'+sy+'_'+sx+'_'+sz
  Path(dir).mkdir(parents=True,exist_ok=True)
  for i in range(z,z+512):
    im = Image.open('d:/scroll4/0'+str(i)+'.tif')

    imarray = numpy.array(im)
    rect = imarray[y:y+512,x:x+512]
    Image.fromarray(rect).save(dir+'/0'+str(i)+'.tif')

    del im
    del imarray
    del rect
  
