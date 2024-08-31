# Will Stevens, August 2024
# 
# Experiment in finding abutting surfaces in a single cubic volumes.
#
# Surface point lists are in folders call sNNN, named vNNN_X.csv, where X is an arbitrary number that got
# assigned to a surface during processing.
#
# Released under GNU Public License V3

import csv
import os
import time
from PIL import Image

volume = "s005"
d1 = "../construct/" + volume
matches = []
	  
files1 = []
  
for file in os.listdir(d1):
  if file.endswith(".csv") and file[0]=="x":
    files1 += [file]

extentsFiles1 = {}
for f1 in files1:
  with open(d1+"/"+f1) as csvfile:
    extreader = csv.reader(csvfile)
    for row in extreader:
      extentsFiles1[f1] = [int(s) for s in row]

pointsetsFiles1 = {}
  
print("Loading volumes")
for f1 in files1:
  pointset1 = set()
  with open(d1+"/v"+f1[1:]) as csvfile:
    volreader = csv.reader(csvfile)
    for row in volreader:
      pt = [int(s) for s in row]
      pointset1.add( (pt[0],pt[1],pt[2]) )
  pointset1.update( [(x+1,y,z) for (x,y,z) in pointset1] ,
                    [(x-1,y,z) for (x,y,z) in pointset1] ,
                    [(x,y+1,z) for (x,y,z) in pointset1] ,
                    [(x,y-1,z) for (x,y,z) in pointset1] ,
                    [(x,y,z+1) for (x,y,z) in pointset1] ,
                    [(x,y,z-1) for (x,y,z) in pointset1] )
  pointsetsFiles1[f1] = pointset1

print("Finished loading volumes")
		
for i in range(0,len(files1)):
  for j in range(i+1,len(files1)):
    f1 = files1[i]
    f2 = files1[j]
    if i != j:
	  
      extent1 = extentsFiles1[f1]
      extent2 = extentsFiles1[f2]
		
      xmin1 = extent1[0]
      ymin1 = extent1[1]
      zmin1 = extent1[2]
      xmax1 = extent1[3]
      ymax1 = extent1[4]
      zmax1 = extent1[5]

    
      xmin2 = extent2[0]
      ymin2 = extent2[1]
      zmin2 = extent2[2]
      xmax2 = extent2[3]
      ymax2 = extent2[4]
      zmax2 = extent2[5]

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
        pointset1 = pointsetsFiles1[f1]
        pointset2 = pointsetsFiles1[f2]
        boundary = pointset1.intersection(pointset2)
			
        if (len(boundary)>100):
          matches += [( (f1[1:4],f1[5:-4]) , (f2[1:4],f2[5:-4]) ,len(boundary) )]
          print(matches[-1])


# A set of sets
imageSets = []

# Need to do several iterations to make sure we have a disjoint list of sets
for i in range(0,10):
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

newImageSets = []

[newImageSets.append(x) for x in imageSets if x not in newImageSets]

imageSets = newImageSets

i = 0
for k in imageSets:
  print(i)
  print(k)

  dst = Image.new('L', (512,512))

  for (folder,image) in k:
    im = Image.open('../construct/s'+folder+'/' + image + '_6.png')
    mask = im.copy()
    mask = mask.convert('L')
    mask = mask.point( lambda p:255 if p>0 else 0 )
    dst.paste(im, (0, 0), mask = mask)

  dst.save('../construct/out_' + str(i) + '.png')
  
  i += 1
  
# List those that have no neighbours

print("No neighbours")
for f1 in files1:
  (folder,image) = (f1[1:4],f1[5:-4])
  found = False
  for i in imageSets:
    if (folder,image) in i:
      found = True
	  
  if not found:
    print((folder,image))