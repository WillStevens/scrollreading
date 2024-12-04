import sys
from pathlib import Path
import numpy
from PIL import Image

# Given a Z coordinate, return the x and y coordinates of the umbilicus
def UmbilicusXY(z):
  return (1950,1770)

if len(sys.argv) != 7:
  print("Usage: calc_surface_and_normal.py VECTOR-INDEX COORD-INDEX X_START Y_START Z_START SIZE")
  exit(-1)

vector_index = sys.argv[1]
coord_index = sys.argv[2]
x_start = int(sys.argv[3])
y_start = int(sys.argv[4])
z_start = int(sys.argv[5])
size = int(sys.argv[6])

cx = x_start+size/2
cy = y_start+size/2
cz = z_start+size/2

(ux,uy) = UmbilicusXY(cz);

nx = ux-cx
ny = uy-cy
nz = 0

px = -ny 
py = nx
pz = 0

qx = 0
qy = 0
qz = 1000

rv = [[px,py,pz],[qx,qy,qz],[nx,ny,nz]]

print( rv[vector_index][coord_index] ) 
