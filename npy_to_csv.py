import numpy as np

vectorField = np.load("vectorfield_test_smooth_100.npy")

vfSize = 100

f = open("vectorfield_test_smooth_100.csv","w")

for z in range(0,vfSize):
  for y in range(0,vfSize):
    for x in range(0,vfSize):
      f.write("%f,%f,%f\n" % (vectorField[z,y,x,0],vectorField[z,y,x,1],vectorField[z,y,x,2]))
