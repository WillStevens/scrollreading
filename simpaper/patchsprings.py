from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk

patches = [(50,50,0),(50,100,0),(100,50,0)]
patchVel = [(5,0,0),(0,0,0),(0,0,0)]
patchAcc = [(0,0,0),(0,0,0),(0,0,0)]

connections = [(0,1,50,0.0,pi),
               (0,2,50,pi/2.0,-pi/2.0)]

CONNECT_FORCE_CONSTANT = 0.01
ANGLE_FORCE_CONSTANT = 0.0
FRICTION_CONSTANT = 0.90
ANGLE_FRICTION_CONSTANT = 0.90


def Distance(x1,y1,x2,y2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))

def AddAngle(a,b):
  r = a+b
  if r>pi:
    return r-2.0*pi
  if r<=-pi:
    return r+2.0*pi
  return r
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1) = patches[p1]  
    (x2,y2,a2) = patches[p2]
    
    (reqx2,reqy2) = (x1+sin(angle1)*dist,y1+cos(angle1)*dist)
    (reqx1,reqy1) = (x2+sin(angle2)*dist,y2+cos(angle2)*dist)
          
    # Which direction is p2 from p1?
    angle12 = atan2(x2-x1,y2-y1)
    # Angle of p1 + angle that connection is expected to be:
    requiredAngle12 = AddAngle(patches[p1][2],angle1)	  
    
    adiff1 = AddAngle(requiredAngle12,-angle12) 
    
    patchAcc[p1] = ( patchAcc[p1][0]+(reqx1-x1)*CONNECT_FORCE_CONSTANT,
	                 patchAcc[p1][1]+(reqy1-y1)*CONNECT_FORCE_CONSTANT,
					 patchAcc[p1][2]+adiff1*ANGLE_FORCE_CONSTANT)
    patchAcc[p2] = ( patchAcc[p2][0]+(reqx2-x2)*CONNECT_FORCE_CONSTANT,
	                 patchAcc[p2][1]+(reqy2-y2)*CONNECT_FORCE_CONSTANT, 
					 patchAcc[p2][2]+0)
  print("angular accel"+ str(patchAcc[0][2]))

def Move():
  global patches,patchVel,patchAcc,connections

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*ANGLE_FRICTION_CONSTANT)
    patchAcc[i] = (0.0,0.0,0.0)

def RunIteration():
  ConnectionForces()
  Move()
  canvas.delete('all')
  for (x,y,a) in patches:
    rad = 20
    canvas.create_oval(x-rad,y-rad,x+rad,y+rad, fill='yellow')
    canvas.create_line(x,y,x+rad*sin(a),y+rad*cos(a))
  window.after(100,RunIteration)
    
window = tk.Tk()
window.geometry('800x600')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=800, height=600, bg='white')
canvas.pack()
window.after(100,RunIteration)
window.mainloop()
    
