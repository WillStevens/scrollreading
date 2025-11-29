from PIL import Image,ImageFilter
import sys
import os
from subprocess import Popen, PIPE
import numpy as np
from random import randint,seed
import csv
import datetime
import time

# Run an external program, and return the output
def CallOutput(arguments):
  process = Popen(arguments, stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  return output.decode("ascii")  

patchCounter = {}
badPatchCounter = {}  
pathsWithBad = []

if len(sys.argv) != 3:
  print("find_bad_patches <patchCoords> <working dir")
  exit(0)

neighbourMapFile = sys.argv[2]+"/neighbourmap.pkl"
transformMapFile = sys.argv[2]+"/transformmap.pkl"
flipstateFile = sys.argv[2]+"/flipstate.pkl"

o = CallOutput(["python","make_neighbourmap.py",sys.argv[1],neighbourMapFile,transformMapFile,flipstateFile])

N = 100

print("Generating random paths...")
patchPosFilePrefix = sys.argv[2]+"/patchPositions"
paths = CallOutput(["python","random_patch_path.py",patchPosFilePrefix,neighbourMapFile,transformMapFile,flipstateFile,str(N)]).splitlines()
print("Finished generating random paths...")
print(len(paths))
for i in range(0,N):
  patchPath = []
  l=paths[i]
  if not (l[0]=='[' and l[1]!='('):
    print("Bad output line from random_patch_path.py")
  patchPath = l[1:-1].split(',')
  if len(patchPath)<=1:
    continue
  print(patchPath)
  patchPath = [int(x) for x in patchPath]
  for pp in patchPath:
    if pp not in patchCounter.keys():
      patchCounter[pp] = 0
    patchCounter[pp] += 1
  
  patchPosFile = patchPosFilePrefix+"_"+str(i)+".txt"
  o = CallOutput(["./find_mismatch","d:/pipelineOutput",patchPosFile])
  for l in o.splitlines():
    if l[0]!='M':
      print("Found one, iteration:"+str(i))
      print(patchPath)
      print(l)
      pair = l.split(':')[0].split(',')
      p1 = int(pair[0])
      p2 = int(pair[1])
      firstIndex = patchPath.index(p1)
      secondIndex = patchPath.index(p2)
      if firstIndex>secondIndex:
        (firstIndex,secondIndex) = (secondIndex,firstIndex)
      pathsWithBad += [patchPath[firstIndex:secondIndex+1]]
      for p in patchPath[firstIndex:secondIndex+1]:
        if p not in badPatchCounter.keys():
          badPatchCounter[p] = 0
        badPatchCounter[p] += 1
        
print(pathsWithBad)
print(patchCounter)
print(badPatchCounter)