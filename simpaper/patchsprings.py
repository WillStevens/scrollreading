from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk

# x,y,orientation,radius
patches = [(50,50,0,20),(50,100,0,20),(100,50,0,20),(100,100,0,20)]
patchVel = [(0,0,0),(0,0,0),(0,0,0),(0,0,0)]
patchAcc = [(0,0,0),(0,0,0),(0,0,0),(0,0,0)]

# patch0, patch1, distance, angle0, angle1
connections = [(0,1,50,0.0,pi),
               (0,2,50,pi/2.0,-pi/2.0),
               (1,3,50,pi/2.0,-pi/2.0),
               (2,3,50,0.2,-pi+0.2)]

patchIndexLookup = {}

translationLookup = {}
rotationLookup = {}

CONNECT_FORCE_CONSTANT = 0.01
ANGLE_FORCE_CONSTANT = 0.01
FRICTION_CONSTANT = 0.90
ANGLE_FRICTION_CONSTANT = 0.90

# Turning iterations into radius
# Each iteration is half a radius. Then multiply by step size
RADIUS_FACTOR = 3

def ComposeTransforms(t1,t2):
  a = t1[0]*t2[0]+t1[1]*t2[3]
  b = t1[0]*t2[1]+t1[1]*t2[4]
  c = t1[0]*t2[2]+t1[1]*t2[5]+t1[2]
  d = t1[3]*t2[0]+t1[4]*t2[3]
  e = t1[3]*t2[1]+t1[4]*t2[4]
  f = t1[3]*t2[2]+t1[4]*t2[5]+t1[5]
  return (a,b,c,d,e,f)

# For this nice inversion formula see
# https://negativeprobability.blogspot.com/2011/11/affine-transformations-and-their.html
def InvertTransform(t):
  a = t[0]
  b = -t[1]
  d = -t[3]
  e = t[4]

  c = -t[2]*t[0]+t[5]*t[1]
  f = -t[5]*t[0]-t[2]*t[1]
  return (a,b,c,d,e,f)

    
def Distance(x1,y1,x2,y2):
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))

def AddAngle(a,b):
  r = a+b
  if r>pi:
    return r-2.0*pi
  if r<=-pi:
    return r+2.0*pi
  return r

def LoadPatches():
  global patches, patchVel, patchAcc,connections, patchIndexLookup
  patches = []
  patchVel = []
  patchAcc = []
  connections = []

  f = open("d:/pipelineOutput/patchCoords.txt")
  
  for l in f.readlines():
    spl = l.split(" ")
    if spl[0]=='ABS':
      patchNum = int(spl[1])
      radius = int(spl[2])*RADIUS_FACTOR
      patchIndexLookup[patchNum] = len(patches)
      x,y,a = float(spl[3]),float(spl[4]),float(spl[5])
      patches += [(x,y,a,radius)]
      patchVel += [(0,0,0)]
      patchAcc += [(0,0,0)]
      translationLookup[patchNum] = (0,0)
      rotationLookup[patchNum] = (1,0,0,1)
    elif spl[0]=='REL':
      patchNum = int(spl[1])
      radius = int(spl[2])*RADIUS_FACTOR
      other = int(float(spl[3]))
      ta = float(spl[10])
      tb = float(spl[11])
      tc = float(spl[12])
      td = float(spl[13])
      te = float(spl[14])
      tf = float(spl[15])

      translationLookup[patchNum] = (tc,tf)
      rotationLookup[patchNum] = (ta,tb,td,te)
      
      if other in patchIndexLookup.keys() and other in translationLookup.keys():
        otherTranslation = translationLookup[other]
        
        print(otherTranslation)
        print((tc,tf))        
        (tc,tf) = (tc-otherTranslation[0],tf-otherTranslation[1])
        print((tc,tf))        
        
        locationAngle = atan2(tc,tf)
        angle = atan2(ta,td)
        distance = sqrt(tc*tc+tf*tf)

        print("Location angle is %f (%f degress)" % (locationAngle,locationAngle*360/(2.0*pi)))
        print("Angle is %f (%f degress)" % (angle,angle*360/(2.0*pi)))
        print("Distance is %f" % distance)
      
        print(str(other))
        print(patchIndexLookup)
        otherIndex = patchIndexLookup[other]
        print("len patches="+str(len(patches))+" otherIndex="+str(otherIndex))

        if patchNum >= len(patches):
          patchIndexLookup[patchNum] = len(patches)
          patches += [(patches[otherIndex][0]+tc,patches[otherIndex][1]+tf,0.0,radius)]
          patchVel += [(0,0,0)]
          patchAcc += [(0,0,0)]
        connections += [(len(patches)-1,otherIndex,distance,
                         AddAngle(AddAngle(pi,locationAngle),-patches[-1][2]),
                         AddAngle(locationAngle,-patches[otherIndex][2]))] 
    else:
      printf("Unexpected tokan in LoadPatches:"+str(spl[0]))
      
  print(patches)
  f.close()
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1,r1) = patches[p1]  
    (x2,y2,a2,r1) = patches[p2]

    currentAngle12 = AddAngle(patches[p1][2],angle1)      
    currentAngle21 = AddAngle(patches[p2][2],angle2)      
    
    (reqx2,reqy2) = (x1+sin(currentAngle12)*dist,y1+cos(currentAngle12)*dist)
    (reqx1,reqy1) = (x2+sin(currentAngle21)*dist,y2+cos(currentAngle21)*dist)
          
    
    actualAngle12 = atan2(x2-x1,y2-y1)
    adiff1 = AddAngle(actualAngle12,-currentAngle12) 

    actualAngle21 = atan2(x1-x2,y1-y2)
    adiff2 = AddAngle(actualAngle21,-currentAngle21) 

    print("%f %f %f %f" % (x1,y1,x2,y2))
    print("%f %f %f %f" % (reqx1,reqy1,reqx2,reqy2))
    print("%f %f" % (actualAngle12,actualAngle21))
    print("%f %f" % (currentAngle12,currentAngle21))
        
    patchAcc[p1] = ( patchAcc[p1][0]+(reqx1-x1)*CONNECT_FORCE_CONSTANT-(reqx2-x2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][1]+(reqy1-y1)*CONNECT_FORCE_CONSTANT-(reqy2-y2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][2]+adiff1*ANGLE_FORCE_CONSTANT-adiff2*ANGLE_FORCE_CONSTANT)
    patchAcc[p2] = ( patchAcc[p2][0]+(reqx2-x2)*CONNECT_FORCE_CONSTANT-(reqx1-x1)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p2][1]+(reqy2-y2)*CONNECT_FORCE_CONSTANT-(reqy1-y1)*CONNECT_FORCE_CONSTANT, 
                     patchAcc[p2][2]+adiff2*ANGLE_FORCE_CONSTANT-adiff1*ANGLE_FORCE_CONSTANT)
                     
    print(patchAcc[p1])
    print(patchAcc[p2])

def Move():
  global patches,patchVel,patchAcc,connections

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2],patches[i][3])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*ANGLE_FRICTION_CONSTANT)
    patchAcc[i] = (0.0,0.0,0.0)
  patchAcc[0] = (0.0,0.0,0.0)
  
def RunIteration():
  ConnectionForces()
  Move()
  canvas.delete('all')
  offset=(400,300)
  scale=0.2
  for (x,y,a,rad) in patches:
    canvas.create_oval(offset[0]+x*scale-rad*scale,offset[1]+y*scale-rad*scale,offset[0]+x*scale+rad*scale,offset[1]+y*scale+rad*scale, fill='yellow')
    #canvas.create_line(offset[0]+x*scale[0],offset[1]+y*scale[1],offset[0]+x*scale[0]+rad*sin(a),offset[1]+y*scale[1]+rad*cos(a))
  for (p0,p1,dist,a0,a1) in connections:
    x,y,a,rad = patches[p0]
    canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*sin(a+a0)*scale,offset[1]+y*scale+rad*0.8*cos(a+a0)*scale)
    x,y,a,rad = patches[p1]
    canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*sin(a+a1)*scale,offset[1]+y*scale+rad*0.8*cos(a+a1)*scale)
  window.after(10,RunIteration)
    
LoadPatches()

window = tk.Tk()
window.geometry('800x600')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=800, height=600, bg='white')
canvas.pack()
window.after(50,RunIteration)
window.mainloop()
    
