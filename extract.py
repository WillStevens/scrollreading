import numpy
from PIL import Image

#for i in range(5800,6312):
#for i in range(5800-512,6312-512):
for i in range(7336,7336+512):
  im = Image.open('d:/vesuvius/0'+str(i)+'.tif')

  imarray = numpy.array(im)

  # coord ranges are y,x
  
  #e002
  #rect = imarray[4200:4712,3900:4412]
  #Image.fromarray(rect).save('../construct/e002/e002_0'+str(i)+'.tif')

  #e203
  #rect = imarray[4200:4712,3400:3912]
  #Image.fromarray(rect).save('../construct/e203/e203_0'+str(i)+'.tif')

  #e204 (x+512 neighbour of e103
  #rect = imarray[4200:4712,3912:3912+512]
  #Image.fromarray(rect).save('../construct/e204/e204_0'+str(i)+'.tif')

  #e205 (x-512 neighbour of e003
  #rect = imarray[4200:4712,3400-512:3400]
  #Image.fromarray(rect).save('../construct/e205/e205_0'+str(i)+'.tif')

  #e206 (x+1024 neighbour of e203
  rect = imarray[4200:4712,3400:3400+512]
  Image.fromarray(rect).save('../construct/03400_04200_07336/0'+str(i)+'.tif')

  del im
  del imarray
  del rect
  
