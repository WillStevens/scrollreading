# Suspect that after a long run, the boundary has lots of small holes that patches can grow in, and these holes aren't removed over time
# If so then expect to find duplicate patches, from the same seed
# Can be solved by removing seed point after it is chosen

# Confirmed the above : in pipelineOutputS4_2025_12_16, after 137000 patches, 43935 were duplicates, and at that point about 40 out of
# every 100 new patches were duplicate - solving this problem will result in a significant performance increase

import os
from subprocess import Popen,PIPE

# Run an external program, and return the output
def CallOutput(arguments):
  process = Popen(arguments, stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  return output.decode("ascii")  

xyzCount = {}

dir = r"d:/pipelineOutputS4_2025_12_16"
fileNameList = os.listdir(dir)

i = 0
dupCount = 0
for fileName in fileNameList:
  if fileName[-4:] == ".bin":
    if i%100==0:
      print(f"{i} {dupCount}")
#    print(fileName)
    centre = CallOutput(["./patch_centre",dir+"/"+fileName])
    xyz = centre.split(',')
#    print(xyz)	
    xyz = (int(float(xyz[0])),int(float(xyz[1])),int(float(xyz[2])))
    if xyz not in xyzCount.keys():
      xyzCount[xyz] = 0
    else:
#      print("Duplicate found " + fileName)
      dupCount += 1
    xyzCount[xyz] += 1
    i += 1
	
for (xyz,count) in xyzCount.items():
  if count>1:
    print((xyz,count))  
  