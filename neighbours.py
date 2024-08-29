# Do two volumes neighbour each other?

# Input parameters - block ref
#                    volume index 1
#                    volume index 2

# Output - yes/no

import csv
import os
import time
from PIL import Image

volumes = [ 
            [("s005",(0,0,0)),    ("s003",(512,0,0)),    ("s004",(1024,0,0))    ,("s006",(1536,0,0))],
            [("s105",(0,0,-512)), ("s103",(512,0,-512)), ("s104",(1024,0,-512)) ,("s106",(1536,0,-512))],
            [("s205",(0,0,-1024)),("s203",(512,0,-1024)),("s204",(1024,0,-1024)),("s206",(1536,0,-1024))],
		  ]

def LookupVolumeImageCoords(vol):
  for v in volumes:
    for w in v:
      if w[0][1:]==vol:
        return (w[1][0],(zsize-1)*512+w[1][2])
  return (-1,-1)
  

xsize = len(volumes[0])
zsize = len(volumes)

directoryPairs = []

for x in range(0,xsize):
  for z in range(0,zsize):
    if x<xsize-1:
      directoryPairs += [(("../construct/"+volumes[z][x][0],volumes[z][x][1]),("../construct/"+volumes[z][x+1][0],volumes[z][x+1][1]))]
    if z<zsize-1:
      directoryPairs += [(("../construct/"+volumes[z][x][0],volumes[z][x][1]),("../construct/"+volumes[z+1][x][0],volumes[z+1][x][1]))]

matches = []
	  
for ((d1,off1),(d2,off2)) in directoryPairs:
  files1 = []
  files2 = []
  
  for file in os.listdir(d1):
    if file.endswith(".csv") and file[0]=="x":
      files1 += [file]

  for file in os.listdir(d2):
    if file.endswith(".csv") and file[0]=="x":
      files2 += [file]
  
  # Make extents and pointsets for all flies in files2 so that we can reuse them without reloading
  extentsFiles2 = {}
  for f2 in files2:
    with open(d2+"/"+f2) as csvfile:
      extreader = csv.reader(csvfile)
      for row in extreader:
        extentsFiles2[f2] = [int(s) for s in row]

  pointsetsFiles2 = {}
  
  print("Loading volumes for files2")
  for f2 in files2:
    pointset2 = set()
    with open(d2+"/v"+f2[1:]) as csvfile:
      volreader = csv.reader(csvfile)
      for row in volreader:
        pt = [int(s) for s in row]
        pointset2.add( (pt[0]+off2[0],pt[1]+off2[1],pt[2]+off2[2]) )
    pointsetsFiles2[f2] = pointset2
  print("Finished loading volumes for files2")
		
  for f1 in files1:
    print(f1)
    with open(d1+"/"+f1) as csvfile:
      for line in csvfile:
        extent1 = [int(s) for s in line.split(',')]

    pointset1 = set()  
    with open(d1+"/v"+f1[1:]) as csvfile:
      for line in csvfile:
        pt = [int(s) for s in line.split(',')]
        pointset1.add( (pt[0]+off1[0],pt[1]+off1[1],pt[2]+off1[2]) )

      pointset1.update( [(x+1,y,z) for (x,y,z) in pointset1] ,
                        [(x-1,y,z) for (x,y,z) in pointset1] ,
                        [(x,y+1,z) for (x,y,z) in pointset1] ,
                        [(x,y-1,z) for (x,y,z) in pointset1] ,
                        [(x,y,z+1) for (x,y,z) in pointset1] ,
                        [(x,y,z-1) for (x,y,z) in pointset1] )
		
    xmin1 = extent1[0]+off1[0]
    ymin1 = extent1[1]+off1[1]
    zmin1 = extent1[2]+off1[2]
    xmax1 = extent1[3]+off1[0]+1
    ymax1 = extent1[4]+off1[1]+1
    zmax1 = extent1[5]+off1[2]+1

    print("About to iterate over f2")	
    for f2 in files2:
      extent2 = extentsFiles2[f2]
    
      xmin2 = extent2[0]+off2[0]
      ymin2 = extent2[1]+off2[1]
      zmin2 = extent2[2]+off2[2]
      xmax2 = extent2[3]+off2[0]+1
      ymax2 = extent2[4]+off2[1]+1
      zmax2 = extent2[5]+off2[2]+1

      overlap = False

      if (xmin2 >= xmin1 and xmin2 <= xmax1 or xmax2 >= xmin1 and xmax2 <= xmax1) and \
         (ymin2 >= ymin1 and ymin2 <= ymax1 or ymax2 >= ymin1 and ymax2 <= ymax1) and \
         (zmin2 >= zmin1 and zmin2 <= zmax1 or zmax2 >= zmin1 and zmax2 <= zmax1):
        overlap = True
   
      if (xmin1 >= xmin2 and xmin1 <= xmax2 or xmax1 >= xmin2 and xmax1 <= xmax2) and \
         (ymin1 >= ymin2 and ymin1 <= ymax2 or ymax1 >= ymin2 and ymax1 <= ymax2) and \
         (zmin1 >= zmin2 and zmin1 <= zmax2 or zmax1 >= zmin2 and zmax1 <= zmax2):
        overlap = True
  
      boundary = set()

      if overlap:
        pointset2 = pointsetsFiles2[f2]
        boundary = pointset1.intersection(pointset2)
		
#        for (x,y,z) in pointset1:
#          for xo in [-1,0,1]:
#            if (x+xo,y,z) in pointset2:
#              boundary.add((x,y,z))
#          for yo in [-1,0,1]:
#            if (x,y+yo,z) in pointset2:
#              boundary.add((x,y,z))
#          for zo in [-1,0,1]:
#            if (x,y,z+zo) in pointset2:
#              boundary.add((x,y,z))
#          if len(boundary)>0:
#            break
			
        if (len(boundary)>0):
          matches += [( (f1[1:4],f1[5:-4]) , (f2[1:4],f2[5:-4]) ,len(boundary) )]
          print(matches[-1])

    print("Finished iterating over f2")	
		  
imageSets = []

for ((folder1,image1) , (folder2,image2), boundary) in matches:
  addedToExisting = False
  for i in range(0,len(imageSets)):
    if (folder1,image1) in imageSets[i]:
      imageSets[i].add( (folder2,image2) )
      addedToExisting = True
    elif (folder2,image2) in imageSets[i]:
      imageSets[i].add( (folder1,image1) )
      addedToExisting = True
  if not addedToExisting:
    imageSets += [set([(folder1,image1),(folder2,image2)])]

i = 0
for k in imageSets:
  print(k)

  dst = Image.new('L', (xsize*512,zsize*512))

  for (folder,image) in k:
    im = Image.open('../construct/s'+folder+'/' + image + '_6.png')
    (x,y) = LookupVolumeImageCoords(folder)
    if x != -1:
      mask = im.copy()
      mask = mask.convert('L')
      mask.point( lambda p:255 if p>0 else 0 )
      dst.paste(im, (x, y), mask = mask)

  dst.save('../construct/out_' + str(i) + '.png')
  
  i += 1