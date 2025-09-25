from PIL import Image,ImageFilter
import sys
import os
from subprocess import Popen, PIPE
import numpy as np
from random import randint,seed
import csv
import datetime
import time

def StartPatchCoordFile(patch,flipped,iterations,coords):
  f = open("d:/pipelineOutput/patchCoords.txt","w")
  f.write("ABS %d %d %d %f %f %f\n" % (patch,(1 if flipped else 0),iterations,coords[0],coords[1],coords[2]))
  f.close()
  
def AddToPatchCoordFile(patch,flipped,iterations,l):
  f = open("d:/pipelineOutput/patchCoords.txt","a")
  for i in l:
    f.write("REL " + str(patch) + " " + str(1 if flipped else 0) + " " + str(iterations))
    for n in i:
      f.write(" "+str(n))
    f.write("\n")
  f.close()

def Step(name):
  print(str(datetime.datetime.now()) + " : " + name)
  
# Points must be a numpy array of 3 arrays: np.array([ [x-coord] , [y-coords] , [z-coords] ])
def FitPlane(points):
  svd = np.linalg.svd(points)

  # Extract the left singular vectors
  left = svd[0]
  
  # Return the best fitting plane and the short axis, and the long axis
  return (left[:, -1],left[:,-2],left[:, -3])

def VarianceTest(variance):
  return variance[0]<=0.01 and variance[1]<=0.01 and variance[2]<=10 and variance[3]<=0.01 and variance[4]<=0.01 and variance[5]<=10
  
# Run an external program, and optionally redirect the output to a file
def Call(arguments,output=None,append=False):
  if output is not None:
    output = open(output,"a" if append else "w")
  process = Popen(arguments, stdout=output)
  (output, err) = process.communicate()
  return process.wait()


# Run an external program, and return the output
def CallOutput(arguments):
  process = Popen(arguments, stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  return output.decode("ascii")  

# Returns a list of the patches and relative transform that p2 aligns with
# Empty list if nothing
def CallAlignMulti(p1,p2):
  process = Popen(["./align_patches6", p1,p2], stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  
  if exit_code == 0:
    output = output.decode("ascii")
    output = output.split("\n")
    r = []
    for o in output:
      s=o.split(" ")
      if len(o.split(" "))==13:
        r += [[float(x) for x in o.split(" ")]]
    return r
  else:
    return []


seed(123)
    
globalStartTime = time.time_ns()

outputDir = "d:/pipelineOutput"

currentBoundary = outputDir+"/boundary.bp"
currentSurface = outputDir+"/surface.bp"
currentSurfaceTif = outputDir+"/surface.tif"
areaLogFile = outputDir+"/area.txt"

# These now come from parameters.py
#VOL_OFFSET_X = 2688
#VOL_OFFSET_Y = 1536
#VOL_OFFSET_Z = 4608

# Voxel size in microns
VOXEL_SIZE = 7.91

MIN_PATCH_ITERS = 45

# A seed consists of x,y,z coords + coords of two vectors that give its orientation
seed = (3700,2408,5632,1,0,0,0,0,1)

# Some seeds near the boundary between different runs of vectorfield - are there any problems at the boundary?
#seed = (2688+526,1536+355,6680,1,0,0,0,0,1)
#seed = (2688+1067,1536+1655,6680,1,0,0,0,0,1)

patchNum = 0
restart = False

#Restarting partway through
#patchNum = 1138
#restart = True

if not restart:
  os.makedirs(outputDir+"/surface.bp/surface")
  os.makedirs(outputDir+"/boundary.bp/surface")

while True:
  print("Patch number "+str(patchNum))

  patch = outputDir+"/patch_"+str(patchNum)+".bin"
  boundary = outputDir+"/boundary_"+str(patchNum)+".bin"
  boundaryf = outputDir+"/boundary_"+str(patchNum)+"_f.bin"
  patchi = outputDir+"/patch_"+str(patchNum)+"_i.bin"
  patchif = outputDir+"/patch_"+str(patchNum)+"_if.bin"

  if not restart:

    # Call simpaper8 with current seed to produce a patch and boundary
    Step("simpaper8")
    iterations = Call(["./simpaper8"] + [str(x) for x in seed] + ["d:/pvfs_2048_chunk_32_v2.zarr",patch,boundary])
    iterations = int(iterations)
    print("Iterations:" + str(iterations))
    if iterations >= MIN_PATCH_ITERS:
      Step("interpolate")
      Call(["./interpolate",patch,patchi])

  if patchNum==0:
    if iterations < MIN_PATCH_ITERS:
      print("First patch has too few iterations")
      exit(0)
    # Initialise current surface and current boundary
    Call(["./addtobigpatch",currentSurface,patchi,str(patchNum)])
    Call(["./addtobigpatch",currentBoundary,boundary,str(patchNum)])
    StartPatchCoordFile(patchNum,False,iterations,(0,0,0))
  elif iterations < MIN_PATCH_ITERS:
    print("Not enough iterations")
  else:
    # Merge this patch into the current surface
    Step("align_patches")
    alignList = CallAlignMulti(currentSurface,patchi)
  
    doMerge = False
    
    if len(alignList)>0:
      badVarianceCount = 0
      for align in alignList:
        print(align)
        patch = align[0]
        variance = (align[1],align[2],align[3],align[4],align[5],align[6])
        transform = (align[7],align[8],align[9],align[10],align[11],align[12])
        
        if not VarianceTest(variance):
          badVarianceCount += 1
      print("Bad variance count:%d" % badVarianceCount)
      
      if badVarianceCount > 0:
        print("Flipping patch")
        Step("flip_patch and boundary")
        Call(["./flip_patch2",patchi,patchif])
        Call(["./flip_patch2",boundary,boundaryf])
        Step("align_patches")
        alignList = CallAlignMulti(currentSurface,patchif)

        if len(alignList)>0:
          badVarianceCount = 0
          for align in alignList:
            print(align)
            patch = align[0]
            variance = (align[1],align[2],align[3],align[4],align[5],align[6])
            transform = (align[7],align[8],align[9],align[10],align[11],align[12])
        
            if not VarianceTest(variance):
              badVarianceCount += 1
          print("Bad variance count:%d" % badVarianceCount)
          if badVarianceCount == 0:
            doMerge = True
            flipped = True
            patchToAdd = patchif
            boundaryToAdd = boundaryf                
          else:
            print("Unable to align, even after flipping")
      else:
        doMerge = True
        flipped = False
        patchToAdd = patchi
        boundaryToAdd = boundary
        
    if doMerge:
      AddToPatchCoordFile(patchNum,flipped,iterations,alignList)

      print("currentBoundary")
      print(len(CallOutput(["./listbigpatchpoints",currentBoundary]).split("\n")))
      print("boundaryToAdd")
      print(len(CallOutput(["./listpatchpoints",boundaryToAdd]).split("\n")))
      
      # For the boundary we need to work out:
      # Given the new patch, which points from the current boundary should we delete?
      Call(["./erasepoints",currentBoundary,patchToAdd,"0","10"])
      # Given the current suface, which points of the new boundary should not be used
      Call(["./erasepoints",currentSurface,boundaryToAdd,"1","5"])

      print("currentBoundary")
      print(len(CallOutput(["./listbigpatchpoints",currentBoundary]).split("\n")))
      print("boundaryToAdd")
      print(len(CallOutput(["./listpatchpoints",boundaryToAdd]).split("\n")))

      Call(["./addtobigpatch",currentBoundary,boundaryToAdd,str(patchNum)])
        
      Call(["./addtobigpatch",currentSurface,patchToAdd,str(patchNum)])      
    else:
      print("No alignment of patch could be done")

  Call(["rm",patchi])        
  Call(["rm",patchif])
  Call(["rm",boundary])        
  Call(["rm",boundaryf])

  #Call(["./listbigpatchpoints",currentBoundary],output=outputDir+"/boundary_"+str(patchNum)+".txt")
    
  Step("seeking next point")
  while True: 
    # Pick a random boundary point
    rb = CallOutput(["./randombigpatchpoint",currentBoundary,str(randint(0,1000000000)),str(randint(0,1000000000))])
    print(rb)
    rb = [float(x) for x in rb.split(" ")]
    
    print("Selected point is %f,%f,%f" % (rb[2],rb[3],rb[4]))
  
    # Open current surface and find the grid points within a radius of 5 of x,y,z on the same patch as x,y,z (the first returned will be the nearest)
    Step("find_nearest4")
    Call(["./find_nearest4",currentSurface,str(rb[2]),str(rb[3]),str(rb[4]),str(rb[5]),"5"],outputDir + "/nearest_tmp.csv")

    points = []

    with open(outputDir + "/nearest_tmp.csv") as csvfile:
      print("Opened nearest point file")
      pointreader = csv.reader(csvfile)
      for row in pointreader:
        points += [[float(row[0]),float(row[1]),float(row[2])]]

    print("Found %d nearest points" % len(points))
 
    seed = (points[0][0],points[0][1],points[0][2])

    # After all of that, if we find that the seed is near the edge of the volume, go back and pick another one  
    if seed[0]-parameters.VOL_OFFSET_X>8 and seed[0]-parameters.VOL_OFFSET_X<parameters.VOL_SIZE_X-8 and seed[1]-parameters.VOL_OFFSET_Y>8 and seed[1]-parameters.VOL_OFFSET_Y<parameters.VOL_SIZE_Y-8 and seed[2]-parameters.VOL_OFFSET_Z>8 and seed[2]-parameters.VOL_OFFSET_Z<parameters.VOL_SIZE_Z-8:
      break
      
  points = np.array([[x[0] for x in points],[x[1] for x in points],[x[2] for x in points]])
  
  # Calculate SVD on these points to get the two plane axes for the seed
  Step("Fit plane")
  plane = FitPlane(points)
  
  print(plane)
  
  seed = (seed[0],seed[1],seed[2],plane[1][0],plane[1][1],plane[1][2],-plane[2][0],-plane[2][1],-plane[2][2])
      
  print("Next seed:")
  print(seed)
  
  # Render the stress (needs to be RGBA)
    
  # For each boundary point, find the stress
  # Find the boundary point with minimum stress - this will be the new seed (choose at random if more than one)
    
  patchNum+=1
  restart = False
  
Call(["./render_from_zarr4",currentSurface])
