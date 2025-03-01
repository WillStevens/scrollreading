import sys
from math import sqrt

# Given a Z coordinate, return the x and y coordinates of the umbilicus
def UmbilicusXY(z):
  scroll1Umbilicus = [(1500,3937,2324),
                      (1750,3925,2312),
                      (2000,3912,2284),
                      (2250,3880,2248),
                      (2500,3863,2215),
                      (2750,3785,2181),
                      (3000,3787,2140),
                      (3250,3780,2153)
                     ]

  scrollUmbilicus = scroll1Umbilicus
  
  (prevZu,prevXu,prevYu) = scrollUmbilicus[0]
  
  if z<=prevZu:
    return (prevXu,prevYu)
    
  for i in range(1,len(scrollUmbilicus)):
    (curZu,curXu,curYu) = scrollUmbilicus[i]
    if z>=prevZu and z<=curZu:
       wCur = (z-prevZu)/(curZu-prevZu)
       wPrev = (curZu-z)/(curZu-prevZu)
       return (int(prevXu*wPrev + curXu*wCur),int(prevYu*wPrev + curYu*wCur))
    (prevZu,prevXu,prevYu) = (curZu,curXu,curYu)
    
  return (curXu,curYu)

if len(sys.argv) != 7:
  print("Usage: calc_surface_and_normal.py VECTOR-INDEX COORD-INDEX X_START Y_START Z_START SIZE")
  exit(-1)

vector_index = int(sys.argv[1])
coord_index = int(sys.argv[2])
x_start = int(sys.argv[3])
y_start = int(sys.argv[4])
z_start = int(sys.argv[5])
size = int(sys.argv[6])

cx = x_start+size/2
cy = y_start+size/2
cz = z_start+size/2

(ux,uy) = UmbilicusXY(cz);
#print("Umbilicus for " + str(cz) + " = " + str(ux) + "," + str(uy))

nx = ux-cx
ny = uy-cy
nz = 0

mn = sqrt(nx*nx+ny*ny+nz*nz)/1000
nx = nx / mn
ny = ny / mn
nz = nz / mn

px = -ny 
py = nx
pz = 0

qx = 0
qy = 0
qz = 1000

rv = [[round(px),round(py),round(pz)],[round(qx),round(qy),round(qz)],[round(nx),round(ny),round(nz)]]

print( rv[vector_index][coord_index] ) 
