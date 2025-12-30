import pickle
import sys

if len(sys.argv) not in [3]:
  print("Usage: check_connectivity <neighbourmap> <bad patches>")
  exit(0)

with open(sys.argv[1], 'rb') as inp:
  neighbourMap = pickle.load(inp)  

badPatches = []
with open(sys.argv[2]) as bpFile:
  l=bpFile.readlines()
  badPatches += [int(x) for x in l]

badPatches = set(badPatches)

unvisited = set(neighbourMap.keys())-badPatches
visited = set(badPatches)

print(f"Total vertices: {len(neighbourMap)}")
print(f"Bad patches: {len(badPatches)}")


while len(unvisited)>0:
  start = min(unvisited)
  visited.add(start)
  next = [start]

  while len(next)>0:
    neighbours = [x for x in neighbourMap[next[0]] if x not in visited]
    next += neighbours
    visited.update(neighbours)
    next = next[1:]
  
  print(f"Visited: {len(visited)-len(badPatches)}")
  unvisited = unvisited - visited