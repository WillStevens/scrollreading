import pickle
import os

workingPaths = ["D:\workingS4_2026_01_24_01_30",
                "D:\workingS4_2026_01_24_03_30",
                "D:\workingS4_2026_01_24_05_30",
                "D:\workingS4_2026_01_24_06_30",
                "D:\workingS4_2026_01_24_07_30",
                "D:\workingS4_2026_01_24_08_30",
                "D:\workingS4_2026_01_24_09_30",
                "D:\workingS4_2026_01_24_10_30",
				]

badScoreCounter = {}

for path in workingPaths:
  badScoreFilename = path+"/badscore.csv" # it's actually a pkl file, not csv
  with open(badScoreFilename,'rb') as f:
    badScore = pickle.load(f)

  patchCounter = {}
	
  # Find all patchPositions files
  ppList = []
  for file in os.listdir(path):
    if file.endswith(".txt"):
      ppList += [file]
  print(f"For {path}, length of ppList is {len(ppList)}")
  print(ppList[0])
  i=0
  for pp in ppList:
    if i%100==0:
      print(i)
    with open(path+"/"+pp) as f:
      lines = f.readlines()
      for l in lines:
        patch = int(l.split(' ')[0])
        if patch not in patchCounter.keys():
          patchCounter[patch] = 0
        patchCounter[patch]+= 1
    i+=1
	
  for (tot,patchNum) in badScore:
    if patchNum not in badScoreCounter.keys():
      badScoreCounter[patchNum] = (patchCounter[patchNum],patchCounter[patchNum]*tot)
    else:
      (oldCount,oldBad) = badScoreCounter[patchNum]
      badScoreCounter[patchNum] = (oldCount+patchCounter[patchNum],oldBad+patchCounter[patchNum]*tot)

badScore = []	  
for (patchNum,(count,bad)) in badScoreCounter.items():
  tot = bad/count
  badScore += [(tot,patchNum,count,bad)]

badScore.sort()
print(badScore[-30:])

with open("d:/s4_explore/bpscoretot_30_2026_01_24.pkl",'wb') as f:
  pickle.dump(badScore,f)