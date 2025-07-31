from math import pi,sqrt,sin,cos
from time import sleep

textPlot = []
for y in range(0,50):
  textPlot += [[]]
  for x in range(0,100):
    textPlot[y] += [0]

patches = [(50,50,0),(50,100,0),(100,50,0)]
patchVel = [(1,1,0),(0,0,0),(0,0,0)]
patchAcc = [(0,0,0),(0,0,0),(0,0,0)]

connections = [(0,1,50,0.0,pi),
               (0,2,50,pi/2.0,3.0*pi/2.0)]

CONNECT_FORCE_CONSTANT = 0.003
FRICTION_CONSTANT = 0.95

def Cls():
  for y in range(0,50):
    for x in range(0,100):
      textPlot[y][x] = 0

def Plot(x,y):
  textPlot[int(y)][int(x)] = 1
  
def Show():
  for y in range(0,50):
    for x in range(0,100):
      print(' ' if textPlot[y][x]==0 else 'O',end='')
    print('')	  

def Distance(x1,y1,x2,y2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1) = patches[p1]  
    (x2,y2,a2) = patches[p2]
    
    (reqx2,reqy2) = (x1+sin(angle1)*dist,y1+cos(angle1)*dist)
    (reqx1,reqy1) = (x2+sin(angle2)*dist,y2+cos(angle2)*dist)
    
    dist1 = Distance(reqx1,reqy1,x1,y1)
    dist2 = Distance(reqx2,reqy2,x2,y2)
    
	
    force1 = dist1*CONNECT_FORCE_CONSTANT
    force2 = dist2*CONNECT_FORCE_CONSTANT
    
    patchAcc[p1] = ( (reqx1-x1)*force1,(reqy1-y1)*force1, 0)
    patchAcc[p2] = ( (reqx2-x2)*force2,(reqy2-y2)*force2, 0)
    
def Move():
  global patches,patchVel,patchAcc,connections

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*FRICTION_CONSTANT)

for i in range(0,100):
  ConnectionForces()
  Move()
  #print("-----")
  #print(patches)
  #print(patchVel)
  Cls()
  for (x,y,a) in patches:
    Plot(x/4,y/4)
  Show()  
  sleep(0.1)