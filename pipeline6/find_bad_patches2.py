from PIL import Image,ImageFilter
import sys
import os
from subprocess import Popen, PIPE
import numpy as np
from random import randint,seed
import csv
import datetime
import time
import pickle

# Run an external program, and return the output
def CallOutput(arguments):
  process = Popen(arguments, stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  return output.decode("ascii")  

patchCounter = {}
badPatchCounter = {}  
pathsWithBad = []

if len(sys.argv) != 7:
  print("find_bad_patches <pipelineOutput> <patchCoords> <working dir> <distance threshhold> <starting patch> <number of patches>")
  exit(0)

neighbourMapFile = sys.argv[3]+"/neighbourmap.pkl"
transformMapFile = sys.argv[3]+"/transformmap.pkl"
flipstateFile = sys.argv[3]+"/flipstate.pkl"
badPatchFile = sys.argv[3]+"/badpatch.csv"
badScoreFile = sys.argv[3]+"/badscore.csv"

DISTANCE_THRESHHOLD = int(sys.argv[4])


o = CallOutput(["python","make_neighbourmap.py",sys.argv[2],neighbourMapFile,transformMapFile,flipstateFile])

startPatch = int(sys.argv[5])
N = int(sys.argv[6])

print("Generating patch pairs...")
patchPosFilePrefix = sys.argv[3]+"/patchPositions"
paths = CallOutput(["python","patch_pairs.py",patchPosFilePrefix,neighbourMapFile,transformMapFile,flipstateFile,badPatchFile,str(startPatch),str(N)]).splitlines()
print("Finished generating patch pairs...")
print(len(paths))
for i in range(len(paths)):
  patchPath = []
  l=paths[i]
  if not (l[0]=='[' and l[1]!='('):
    print("Bad output line from patch_pairs.py")
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
  o = CallOutput(["./find_mismatch",sys.argv[1],patchPosFile,str(DISTANCE_THRESHHOLD)])
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

badScore = []
for a in sorted(patchCounter.keys()):
  print(f"{a},{patchCounter[a]},{badPatchCounter[a] if a in badPatchCounter.keys() else ''},",end="")
  tot = badPatchCounter[a]/patchCounter[a] if a in badPatchCounter.keys() else 0
  print(tot)
  badScore += [(tot,a)]
 
#for (tot,a) in badScore:
#  if tot>0:
#    print(a)  
badScore.sort()

with open(badScoreFile,'wb') as f:
  pickle.dump(badScore,f)

for x in badScore[-30:]:
  if x[0]>0:
    print(x[1])
