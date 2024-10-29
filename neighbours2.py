# Will Stevens, October 2024
# 
# Experiment in finding abutting surfaces in neighbouring cubic volumes.
##
# Released under GNU Public License V3



import csv
import os
import time
from PIL import Image


folders = ["01488_03476_01500","01488_03988_01500","01488_04500_01500",
                                                   "02000_04500_01500",
           "02512_03476_01500","02512_03988_01500","02512_04500_01500"]

dirPrefix="../construct/s"
dirSuffix="/nonint/output/"

SIZE=512
POINTS_LIMIT=131072


def NeighboursInFolders(f1,x1,y1,z1,f2,x2,y2,z2):
  numPointsMapList = []
  edgePointsMapList = []
  
  print("Testing neighbouring dirs")
  
  for (dir,x,y,z) in [(f1,x1,y1,z2),(f2,x2,y2,z2)]:
    print("In dir "+dir)
    numPointsMap = {}
    edgePointsMap = {}
    for file in os.listdir(dirPrefix+dir+dirSuffix):
      if file.endswith(".csv") and file[0]=="v":
        print("-",end="",flush=True)
        with open(dirPrefix+dir+dirSuffix+"/"+file) as csvfile:
          numPoints = 0
          edgePoints = set()
          volreader = csv.reader(csvfile)
          for row in volreader:
            pt = [int(s) for s in row]
            numPoints += 1
            if pt[0]==0 or pt[0]==SIZE-1 or pt[1]==0 or pt[1]==SIZE-1 or pt[2]==0 or pt[2]==SIZE-1:
              edgePoints.add( (pt[0]+x,pt[1]+y,pt[2]+z) )
        numPointsMap[file] = numPoints
        edgePointsMap[file] = edgePoints
      
    numPointsMapList += [numPointsMap]
    edgePointsMapList += [edgePointsMap]
    print()
    
  for file1 in numPointsMapList[0].keys():
    for file2 in numPointsMapList[1].keys():
        
      # Only look for neighbours if numPoints is higher than some threshhold
      if numPointsMapList[0][file1]>=POINTS_LIMIT and numPointsMapList[1][file2]>=POINTS_LIMIT:
#        print("Considering "+file1 + " " + file2)
        edgeMatch = 0
        for (ex1,ey1,ez1) in edgePointsMapList[0][file1]:
          for (ex2,ey2,ez2) in edgePointsMapList[1][file2]:
            if (abs(ex2-ex1)<=1 and abs(ey2-ey1)<=1 and abs(ez2-ez1)<=1):
              edgeMatch += 1
        smallestEdge = len(edgePointsMapList[0][file1]) if len(edgePointsMapList[0][file1])<len(edgePointsMapList[1][file2]) else len(edgePointsMapList[1][file2])
    
        if smallestEdge < edgeMatch*20:
          print(f1+"/"+file1+ " matches "+f2+"/"+file2)
        elif edgeMatch>0:
          print("smallestEdge:%d, edgeMatch:%d"%(smallestEdge,edgeMatch))
#        else:
#          print("---")
        
for i in range(0,len(folders)):
  for j in range(i+1,len(folders)):
    f1 = folders[i]
    f2 = folders[j]
    
    (y1,x1,z1) = (int(f1[0:5]),int(f1[6:11]),int(f1[12:17]))
    (y2,x2,z2) = (int(f2[0:5]),int(f2[6:11]),int(f2[12:17]))
    
    if x1==x2 and y1==y2 and abs(z1-z2)==SIZE or x1==x2 and z1==z2 and abs(y1-y2)==SIZE or y1==y2 and z1==z2 and abs(x1-x2)==SIZE:
      NeighboursInFolders(f1,x1,y1,z1,f2,x2,y2,z2)
