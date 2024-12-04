import sys
from pathlib import Path
import numpy
from PIL import Image

if len(sys.argv) != 7:
  print("Usage: extract_general.py IMAGES_DIR CUBES_DIR X_START Y_START Z_START SIZE")
  exit(-1)
  
images_dir = sys.argv[1]
cubes_dir = sys.argv[2]
x_start = int(sys.argv[3])
y_start = int(sys.argv[4])
z_start = int(sys.argv[5])
size = int(sys.argv[6])

for (x,y,z) in [(x_start,y_start,z_start)]:
  sx = '0'*(5-len(str(x)))+str(x)
  sy = '0'*(5-len(str(y)))+str(y)
  sz = '0'*(5-len(str(z)))+str(z)
  
  dir = cubes_dir + '/'+sy+'_'+sx+'_'+sz
  Path(dir).mkdir(parents=True,exist_ok=True)
  for i in range(z,z+size):
    image_prefix = '0'*(5-len(str(i)))+str(i)
    im = Image.open(images_dir+'/'+image_prefix+'.tif')

    imarray = numpy.array(im)
    rect = imarray[y:y+size,x:x+size]
    Image.fromarray(rect).save(dir+'/'+image_prefix+'.tif')

    del im
    del imarray
    del rect
  
