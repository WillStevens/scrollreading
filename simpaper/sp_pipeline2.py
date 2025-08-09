from PIL import Image,ImageFilter
import sys
import os
from subprocess import Popen, PIPE
import numpy as np
from random import randint
import csv
import datetime
import time

from mask_add import MaskAdd

def Step(name):
  print(str(datetime.datetime.now()) + " : " + name)
  
# Points must be a numpy array of 3 arrays: np.array([ [x-coord] , [y-coords] , [z-coords] ])
def FitPlane(points):
  svd = np.linalg.svd(points)

  # Extract the left singular vectors
  left = svd[0]
  
  # Return the best fitting plane and the short axis, and the long axis
  return (left[:, -1],left[:,-2],left[:, -3])

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

# Return boolean and transformation if the two patches can be aligned
# Also return the variance in the transforms found, which is used to determine if the patch is flipped over or not
def CallAlign(p1,p2):
  process = Popen(["./align_patches4.exe", p1,p2], stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  
  if exit_code == 0:
    output = output.decode("ascii")
    outputNumbers = [float(x) for x in output.split(" ")]
    return (True,tuple( outputNumbers[6:12] ),tuple( outputNumbers[0:6] ))
  else:
    return (False,(0,0,0,0,0,0),(0,0,0,0,0,0))
  
globalStartTime = time.time_ns()

outputDir = "d:/pipelineOutput"

currentStress = outputDir+"/stress.csv"
currentSurface = outputDir+"/surface.bp"
currentSurfaceTif = outputDir+"/surface.tif"
temporaryFile = outputDir+"/transformed.bin" # used for transformed patch
areaLogFile = outputDir+"/area.txt"

VOL_OFFSET_X = 2688
VOL_OFFSET_Y = 1536
VOL_OFFSET_Z = 4608

# Voxel size in microns
VOXEL_SIZE = 7.91

# A seed consists of x,y,z coords + coords of two vectors that give its orientation
seed = (3700,2408,5632,1,0,0,0,0,1)

currentRenderOffset = (0,0)
patchNum = 0
restart = False

#Restarting partway through
#currentRenderOffset = (324,-5887)
#patchNum = 1138
#restart = True

while True:
  print("Patch number "+str(patchNum))
  needToRender = False

  patch = outputDir+"/patch_"+str(patchNum)+".csv"
  patchi = outputDir+"/patch_"+str(patchNum)+"_i.csv"
  patchif = outputDir+"/patch_"+str(patchNum)+"_if.csv"

  if not restart:

    # Call simpaper7 with current seed to produce a patch
    Step("simpaper7")
    Call(["./simpaper7"] + [str(x) for x in seed] + ["d:/pvfs_2048_chunk_32_v2.zarr",patch,outputDir+"/stress_"+str(patchNum)+".csv"])
    Step("interpolate")
    Call(["./interpolate",patch,patchi])
    #Step("render_from_zarr3")
    #Call(["./render_from_zarr3",patchi])
    #Step("extent_patch")
    #Call(["./extent_patch",patchi],outputDir+"/patch_"+str(patchNum)+"_i.ext")

  if patchNum==0:
    # Initialise current surface and current stress map
    Call(["./addtobigpatch",currentSurface,patchi,str(patchNum)])
    Step("render_from_zarr4 (patch mask)")
    currentRenderOffset = CallOutput(["./render_from_zarr4",currentSurface,"1"])
    currentRenderOffset = currentRenderOffset.split(" ")
    currentRenderOffset = (int(currentRenderOffset[0]),int(currentRenderOffset[1]))
    print("currentRenderOffset:"+str(currentRenderOffset))
    needToRender = False
  else:
    # Merge this patch into the current surface
    Step("align_patches")
    (aligned,transform,variance) = CallAlign(currentSurface,outputDir+"/patch_"+str(patchNum)+"_i.csv")
  
    if (aligned):
      print("Variance after aligning is: " + str(variance))
      if variance[2]>10000 or variance[5]>10000:
        print("Flipping patch")
        Step("flip_patch")
        Call(["./flip_patch2",patchi,patchif])
        #Step("extent_patch")
        #Call(["./extent_patch",patchif],outputDir+"/patch_"+str(patchNum)+"_if.ext")
        Step("align_patches")
        (aligned,transform,variance) = CallAlign(currentSurface,outputDir+"/patch_"+str(patchNum)+"_if.csv")
        print("Variance after flipping is: " + str(variance))
        if variance[2]<=10000 and variance[5]<=10000:
          Step("transform_patch")
          Call(["./transform_patch",outputDir+"/patch_"+str(patchNum)+"_if.csv",temporaryFile] + [str(x) for x in transform])
          needToRender = True
          Call(["./addtobigpatch",currentSurface,temporaryFile,str(patchNum)])
          # Delete the interpolated patch and te flipped patch because they take up a lot of space
        else:
          print("Unable to align, even after flipping")
        Call(["rm",patchif])
      else:
        Step("transform_patch")
        Call(["./transform_patch",outputDir+"/patch_"+str(patchNum)+"_i.csv",temporaryFile] + [str(x) for x in transform])
        needToRender = True
        Call(["./addtobigpatch",currentSurface,temporaryFile,str(patchNum)])
        # Delete the interpolated patch because it takes up a lot of space
    else:
      print("No alignment of patch could be done")
    # Merge the stress output into the current stress map

  Call(["rm",patchi])        

  if needToRender:  
    Step("render_from_zarr4 (patch mask)")
    patchRenderOffset = CallOutput(["./render_from_zarr4",temporaryFile,"1"])
    patchRenderOffset = patchRenderOffset.split(" ")
    patchRenderOffset = (int(patchRenderOffset[0]),int(patchRenderOffset[1]))
    print("Offset when rendering patch:" + str(patchRenderOffset))
    currentSurfaceImage = Image.open(currentSurfaceTif)
    newPatchImage = Image.open(temporaryFile[:-4]+".tif")
    (currentSurfaceImage,currentRenderOffset) = MaskAdd(currentSurfaceImage,currentRenderOffset,newPatchImage,patchRenderOffset)  
    currentSurfaceImage.save(currentSurfaceTif)
    # Make a surface mask
    #Step("render_from_zarr4 (mask)")
    #renderOffset = CallOutput(["./render_from_zarr4",currentSurface,"1"])
    #renderOffset = renderOffset.split(" ")
    #print("Offset when rendering:" + str(renderOffset))
    #currentRenderOffset = (int(renderOffset[0]),int(renderOffset[1]))
    
  # Find all boundary points on the surface mask
  Step("boundary")
  boundary = Image.open(currentSurface[:-3]+".tif")
  boundary = boundary.convert("L")
  # Calculate the total surface area so far
  
  bwImage = np.array(boundary)  
  whitePixels = np.sum(bwImage==255)
  globalTimeNow = time.time_ns()-globalStartTime
  areaLog = open(areaLogFile,"a")
  # Output time in seconds and area in cm2
  areaLog.write("Time=%f, Area=%f\n" % (globalTimeNow/1000000000.0,whitePixels*VOXEL_SIZE*VOXEL_SIZE/100000000.0))
  areaLog.close()
	
  boundary = boundary.filter(ImageFilter.FIND_EDGES)
  boundary.save(currentSurface[:-3]+"_boundary.tif")
  
  boundary = np.array(boundary)
  
  yb, xb = (boundary > 0).nonzero()

  xb += currentRenderOffset[0]
  yb += currentRenderOffset[1]
  
  print("%d boundary points " % xb.shape[0])
  
  # We want to find a point on the bounary that, when a patch is grown, is likely to result in the smallest
  # increase in boundary size - this is most likely to lead to a blob shaped surface rather than a forking surface.
  
  # One proxy for this is to look for the boundary point which has as large as possible number of points within a typical patch sized
  # radius
  
  
  #mode = randint(0,10)
  # WS 2025-06-28 - don't bother with the modes that seek to extent the range
  mode = 4
  Step("seeking next point")
  while True: 
    n = randint(0,xb.shape[0]-1)
    x,y = xb[n],yb[n]
    print("Initially selected point is %d,%d" % (x,y))

    print("Point seek mode " + str(mode))
  
    if mode < 4:
      for i in range(0,100):
        n = randint(0,xb.shape[0]-1)
        xn,yn = xb[n],yb[n]
        if mode==0 and xn<x:
          x,y = xn,yn   
        elif mode==1 and xn>x:
          x,y = xn,yn
        elif mode==2 and yn<y:
          x,y = xn,yn
        elif mode==3 and yn>y:
          x,y = xn,yn
    
    print("Final selected point is %d,%d" % (x,y))
  
    # Open current surface and find the grid points within a radius of 5 of x,y (the first returned will be the nearest)
    Step("find_nearest")
    Call(["./find_nearest3",currentSurface,str(x),str(y),"5"],outputDir + "/nearest_tmp.csv")

    points = []

    with open(outputDir + "/nearest_tmp.csv") as csvfile:
      print("Opened nearest point file")
      pointreader = csv.reader(csvfile)
      for row in pointreader:
        points += [[float(row[0]),float(row[1]),float(row[2])]]

    print("Found %d nearest points" % len(points))
 
    seed = (points[0][0],points[0][1],points[0][2])

    # After all of that, if we find that the seed is near the edge of the volume, go back and pick another one  
    if seed[0]-VOL_OFFSET_X>8 and seed[0]-VOL_OFFSET_X<2048-8 and seed[1]-VOL_OFFSET_Y>8 and seed[1]-VOL_OFFSET_Y<2048-8 and seed[2]-VOL_OFFSET_Z>8 and seed[2]-VOL_OFFSET_Z<2048-8:
      break
    else:
      mode = 4
      
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
