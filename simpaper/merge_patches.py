import sys
import os
from subprocess import Popen, PIPE

def MultiplyTransform(x,y):
  (a,b,c,d,e,f) = x
  (ad,bd,cd,dd,ed,fd) = y
  
  return (a*ad+b*dd,a*bd+b*ed,a*cd+b*fd+c,d*ad+e*dd,d*bd+e*ed,d*cd+e*fd+f)

# Do two extents overlap
def Overlap(e1,e2):
  (x1min,y1min,z1min,x1max,y1max,z1max) = e1
  (x2min,y2min,z2min,x2max,y2max,z2max) = e1
  return True

def FindExtents(patches):
  extents = {}

  #print("Loading extents")
  for patch in patches:
    #print(patch)
    f = open("outputs/"+patch+".ext","r")
    for line in f:
      #print(line)
      extents[patch] = tuple( [float(x) for x in line.split(",")] )

  return extents
  
# Return boolean and transformation if the two patches can be aligned
def CallAlign(p1,p2):
  process = Popen(["./align_patches.exe", "outputs/"+p1+".csv", "outputs/"+p2+".csv"], stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  
  if exit_code == 0:
    #print("align_patches output")
    #print(output)
    output = output.decode("ascii")
    #print(output)
    return (True,tuple( [float(x) for x in output.split(" ")] ))
  else:
    return (False,(0,0,0,0,0,0))
  
def FindAllOverlapping(p,used):
  r = []
  pe = patchExtent[p]
  
  for (po,poe) in patchExtent.items():
    if po not in used and Overlap(pe,poe):
	  # call align for the two patches, and store the output
      (aligned,transform) = CallAlign(p,po)
      if aligned:
        r += [(po,transform)]
  return r

def RecursivelyFindOverlapping(p,transformIn,used):
  print("#"+p)

  overlapping = FindAllOverlapping(p,used)
  
  toAdd = []
  
  if len(overlapping)>0:
    used.update([x[0] for x in overlapping])
    for (po,transform) in overlapping:
      more = RecursivelyFindOverlapping(po,transform,used)
      used.update([x[0] for x in more])
      toAdd += more
  overlapping += toAdd
  
  # Multiply transform by the input transform
  for i in range(0,len(overlapping)):
    overlapping[i] = (overlapping[i][0],MultiplyTransform(transformIn,overlapping[i][1]))
	
  return overlapping
  
if len(sys.argv) != 3:
  print("Usage: merge_patches <directory> <start>")
  print("Merge all patches in a directory into a single descriptive file")
  exit(-1)
  
print("./transform_patch outputs/" + sys.argv[2] + ".csv 1 0 0 0 1 0 > outputs/test.csv")

patches = []

# Load all .ext files in the directory  
for file in os.listdir(sys.argv[1]):
  if file.endswith(".ext"):
    patches += [file[:-4]]

#print("Patches")
#print(patches)

# Map from patch name to extent of patch
patchExtent = FindExtents(patches)
#print(patchExtent)

used = set([sys.argv[2]])
  
overlapping = RecursivelyFindOverlapping(sys.argv[2],(1,0,0,0,1,0),used)

for (p,t) in overlapping:
  print("./transform_patch outputs/%s.csv %f %f %f %f %f %f >> outputs/test.csv\n" % (p,t[0],t[1],t[2],t[3],t[4],t[5])) 