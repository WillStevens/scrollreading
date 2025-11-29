from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk
import sys
import os
import numpy as np
from PIL import Image,ImageTk

import parameters

filenameIndex = 0
patchToAdd = 0

patches = {}
patchesAlt = {} # positions derived from other connections
patchVel = {}
patchAcc = {}
connections = {}

transformLookup = {}

patchImages = {}
ImageRefs = [] # keep refs that canvas has so that they don't get GC

CONNECT_FORCE_CONSTANT = 0.01
ANGLE_FORCE_CONSTANT = 0.01
FRICTION_CONSTANT = 0.60
ANGLE_FRICTION_CONSTANT = 0.60

# Factor for converting interations into approximate patch radius
#RADIUS_FACTOR = parameters.QUADMESH_SIZE/2.0
RADIUS_FACTOR = 3

MAX_CORRECTABLE_DISTANCE = 3500

centreOffset = (675,350)
currentFocus = (0,0)

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

def LoadPatchImage(patchNum):
  global patchImages
  
  if os.path.exists("d:/pipelineOutputTry20/patch_%d.tif" % patchNum):
    image = Image.open("d:/pipelineOutputTry20/patch_%d.tif" % patchNum).convert("L")
    mask = Image.open("d:/pipelineOutputTry20/patch_%d.tifm" % patchNum).convert("L")

    # Turn image and mask into a single image with transparency
    rgba = Image.merge("RGBA",(image,image,image,mask))
  
    rgba.save("d:/pipelineOutputTry20/patch_%d.png" % patchNum)
    patchImages[patchNum] = rgba

def SortPatchCoords(fin):
  """ Sort the patch coords so that the neighbours of 0 are added first, then neighbours of next lowest etc..."""
  f = open(fin)

  outlines = []
  
  for l in f.readlines():
    spl = l.split(" ")
    if spl[0]=='ABS':
      outlines += [(0,0,l)]
    elif spl[0]=='REL':
      patchNum = int(spl[1])
      other = int(float(spl[4]))
      outlines += [(other,patchNum,l)]
 
  f.close()
  outlines.sort()

  return outlines
  
def AddPatch(sortedPatches,index):
  global renderPatches, patches, patchVel, patchAcc,connections, patchesAlt
  
  (other,patchNum,l) = sortedPatches[index]
  spl = l.split(" ")
  if spl[0]=='ABS':
    if renderPatches:
      LoadPatchImage(patchNum)
    radius = int(spl[3])*RADIUS_FACTOR
    x,y,a = float(spl[4]),float(spl[5]),float(spl[6])
    patches[patchNum]= (x,y,a,radius,0,False)
    patchVel[patchNum]= (0,0,0)
    patchAcc[patchNum]= (0,0,0)
    transformLookup[patchNum] = (1,0,0,0,1,0)
  elif spl[0]=='REL':
    flip = (spl[2]=='1')
    radius = int(spl[3])*RADIUS_FACTOR
    ta = float(spl[11])
    tb = float(spl[12])
    tc = float(spl[13])
    td = float(spl[14])
    te = float(spl[15])
    tf = float(spl[16])
      
    if other in transformLookup.keys():
      otherTransform = transformLookup[other]
    else:
      return False

    transform = ComposeTransforms(otherTransform,(ta,tb,tc,td,te,tf))
                        
    (offsetX,offsetY) = (transform[2]-otherTransform[2],transform[5]-otherTransform[5])
        
    locationAngle = atan2(offsetX,offsetY)
    angle = atan2(transform[0],transform[3]) # global orientation of this patch
    distance = sqrt(offsetX*offsetX+offsetY*offsetY)

    if patchNum not in patches.keys():
      if renderPatches:
        LoadPatchImage(patchNum)
      transformLookup[patchNum] = transform
      patches[patchNum]= (transform[2],transform[5],0.0,radius,angle,flip)
      patchVel[patchNum]= (0,0,0)
      patchAcc[patchNum]= (0,0,0)
    else:
      print("For patch %d, existing angle = %f, angle inferred via patch %d is %f" % (patchNum,patches[patchNum][4],other,angle))
      patchesAlt[(patchNum,other)] = (transform[2],transform[5],0.0,radius,angle,flip)
    connections[(other,patchNum)] = (distance,AddAngle(pi,locationAngle),locationAngle)
  else:
    print("Unexpected token in LoadPatches:"+str(spl[0]))
    exit(0)
      
  #print(patches)
  
def SavePatches():
  print("Saving positions...")
  f = open("d:/pipelineOutput/patchPositions.txt","w")
  
  if f:
    for (patchNum,patchIndex) in patchIndexLookup.items():
      (x,y,a,r,ga) = patches[patchIndex]
      f.write("%d %f %f %f\n" % (patchNum,x,y,AddAngle(a,ga)))

  f.close()
  print("Saved")
  
def ConnectionForces():
  global patches,patchVel,patchAcc,connections

  for (p1,p2,dist,angle1,angle2) in connections:
    (x1,y1,a1,r1,ga1) = patches[p1]  
    (x2,y2,a2,r2,ga2) = patches[p2]

    currentAngle12 = AddAngle(patches[p1][2],angle1)      
    currentAngle21 = AddAngle(patches[p2][2],angle2)      
    
    (reqx2,reqy2) = (x1+sin(currentAngle12)*dist,y1+cos(currentAngle12)*dist)
    (reqx1,reqy1) = (x2+sin(currentAngle21)*dist,y2+cos(currentAngle21)*dist)
          
    
    actualAngle12 = atan2(x2-x1,y2-y1)
    adiff1 = AddAngle(actualAngle12,-currentAngle12) 

    actualAngle21 = atan2(x1-x2,y1-y2)
    adiff2 = AddAngle(actualAngle21,-currentAngle21) 

    #print("%f %f %f %f" % (x1,y1,x2,y2))
    #print("%f %f %f %f" % (reqx1,reqy1,reqx2,reqy2))
    #print("%f %f" % (actualAngle12,actualAngle21))
    #print("%f %f" % (currentAngle12,currentAngle21))
        
    patchAcc[p1] = ( patchAcc[p1][0]+(reqx1-x1)*CONNECT_FORCE_CONSTANT-(reqx2-x2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][1]+(reqy1-y1)*CONNECT_FORCE_CONSTANT-(reqy2-y2)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p1][2]+adiff1*ANGLE_FORCE_CONSTANT-adiff2*ANGLE_FORCE_CONSTANT)
    patchAcc[p2] = ( patchAcc[p2][0]+(reqx2-x2)*CONNECT_FORCE_CONSTANT-(reqx1-x1)*CONNECT_FORCE_CONSTANT,
                     patchAcc[p2][1]+(reqy2-y2)*CONNECT_FORCE_CONSTANT-(reqy1-y1)*CONNECT_FORCE_CONSTANT, 
                     patchAcc[p2][2]+adiff2*ANGLE_FORCE_CONSTANT-adiff1*ANGLE_FORCE_CONSTANT)
                     
    #print(patchAcc[p1])
    #print(patchAcc[p2])

def Move():
  global patches,patchVel,patchAcc,connections

  for i in range(0,len(patches)):
    patches[i] = (patches[i][0]+patchVel[i][0],patches[i][1]+patchVel[i][1],patches[i][2]+patchVel[i][2],patches[i][3],patches[i][4])
    patchVel[i] = (patchAcc[i][0]+patchVel[i][0],patchAcc[i][1]+patchVel[i][1],patchAcc[i][2]+patchVel[i][2])
    patchVel[i] = (patchVel[i][0]*FRICTION_CONSTANT,patchVel[i][1]*FRICTION_CONSTANT,patchVel[i][2]*ANGLE_FRICTION_CONSTANT)
    patchAcc[i] = (0.0,0.0,0.0)
  patchAcc[0] = (0.0,0.0,0.0)
  
def from_rgb(rgb):
    """translates an rgb tuple of int to a tkinter friendly color code
    """
    return "#%02x%02x%02x" % rgb 
    
def truncate(x):
  if x<0.0:
    return 0.0
  if x>1.0:
    return 1.0
  return x

def Show(highlightedIndex,links=True):
  global renderPatches,ImageRefs,sortedPatches,currentOffset,currentFocus,mode,filenameIndex
  ImageRefs = []
  canvas.delete('all')
  offset=(centreOffset[0]-currentFocus[0],centreOffset[1]-currentFocus[1])
  scale=0.3
  patchi = 0
  patchesLen = len(patches)
  focus=currentFocus
  for patchNum in patches.keys():
    alternate = (sortedPatches[highlightedIndex][1],sortedPatches[highlightedIndex][0])
    if patchNum == alternate[0] and alternate in patchesAlt.keys():
      (x,y,a,rad,ga,flip) = patchesAlt[alternate]
    else:
      (x,y,a,rad,ga,flip) = patches[patchNum]

    patchif = pi*float(patchi)/float(patchesLen)
    
    red = 1.0+cos(patchif)
    green = 1.0-cos(patchif)
    blue = 1.0*sin(patchif)
    (red,green,blue)=(truncate(red/2),truncate(green/2),truncate(blue))
    
    rgb = from_rgb((int(red*255),int(green*255),int(blue*255)))
    
    patchi+=1
    
    (y,x)=(-x,y)

    if not renderPatches:
      canvas.create_oval(offset[0]+x*scale-rad*scale,offset[1]+y*scale-rad*scale,offset[0]+x*scale+rad*scale,offset[1]+y*scale+rad*scale, fill=rgb)
      #canvas.create_line(offset[0]+x*scale[0],offset[1]+y*scale[1],offset[0]+x*scale[0]+rad*sin(a),offset[1]+y*scale[1]+rad*cos(a))
    if renderPatches and patchNum in patchImages.keys():
      rgba = patchImages[patchNum]
      if patchNum == sortedPatches[highlightedIndex][1] or patchNum == sortedPatches[highlightedIndex][0]:
        arr = np.array(rgba,dtype=np.float32)
        if patchNum == sortedPatches[highlightedIndex][1]:
          arr[...,0] *= 1.5
          focus = (x*scale,y*scale)
        if patchNum == sortedPatches[highlightedIndex][0]:
          arr[...,1] *= 1.5
        np.clip(arr,0,255,out=arr)
        arr = arr.astype(np.uint8,copy=False)
        rgba = Image.fromarray(arr)
      if flip:
        rgba = rgba.transpose(Image.FLIP_LEFT_RIGHT)
      scale_factor = scale*RADIUS_FACTOR*2
      rgba = rgba.resize((int(rgba.size[0]*scale_factor),int(rgba.size[1]*scale_factor)),Image.Resampling.LANCZOS)
      if patchNum != 0:
        rgba = rgba.rotate(ga*360.0/(2*3.141592))
      else:
        rgba = rgba.rotate(90)
      rgba = ImageTk.PhotoImage(rgba)
      ImageRefs += [rgba]
      canvas.create_image(offset[0]+x*scale-rad*scale,offset[1]+y*scale-rad*scale, image = rgba, anchor = tk.NW)
    if renderPatches and patchNum not in patchImages.keys():
      print("Patch image not loaded %d" % patchNum) 
  if links and not renderPatches:
    for (other,patchNum) in connections.keys():
      (dist,a0,a1) = connections[(other,patchNum)]
      # TODO - need to do (y,x)=(-x,y)
      y,x,a,rad,ga = patches[patchNum]
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a0)*scale,offset[1]+y*scale+rad*0.8*sin(a+a0)*scale)
      y,x,a,rad,ga = patches[other]
      canvas.create_line(offset[0]+x*scale,offset[1]+y*scale,offset[0]+x*scale+rad*0.8*cos(a+a1)*scale,offset[1]+y*scale+rad*0.8*sin(a+a1)*scale)

  canvas.create_text(50,15,text=str(highlightedIndex),font=("Arial",16),fill="black")
  canvas.create_text(50,35,text=str(sortedPatches[highlightedIndex][1]),font=("Arial",16),fill="red")
  canvas.create_text(50,55,text=str(sortedPatches[highlightedIndex][0]),font=("Arial",16),fill="green")
  canvas.create_text(50,75,text=mode,font=("Arial",16),fill="green")

  
  filename = "d:/pipelineOutputTry20/patchgrowanim2/patches_%06d"%filenameIndex
  canvas.postscript(file=filename+".eps",pagewidth="18.75i",pageheight="9.72222i",colormode='color')
  img = Image.open(filename + '.eps') 
  img.save(filename + '.png', 'png') 
  filenameIndex += 1
  
  
  print(currentFocus)
  print(focus)
  if Distance(focus[0],focus[1],currentFocus[0],currentFocus[1])>200:
    currentFocus = ((4*currentFocus[0]+focus[0])/5,(4*currentFocus[1]+focus[1])/5)
    return False
  else:
    return True
    
def RunIteration():
  global iterationCount
  ConnectionForces()
  Move()
  
  iterationCount += 1
  
  if iterationCount == 50:
    SavePatches()
  Show()    
  window.after(10,RunIteration)


def DoShow():
  if Show(patchToAdd):
    window.after(500,AddPatches)
  else:
    window.after(100,DoShow)
  
def AddPatches():
  global patchToAdd
  if mode=='WAIT':
    return
  patchToAdd+=1
  print("(other,patchNum) = (" + str(sortedPatches[patchToAdd][0]) + "," + str(sortedPatches[patchToAdd][1]) + ")")
  AddPatch(sortedPatches,patchToAdd)
  DoShow()

def keydown(e):
  global mode
  print("'"+e.char+"'")  
  print(mode)
  if e.char == ' ' and mode=="WAIT":
    mode='ADDING'
    AddPatches()
  elif e.char == ' ' and mode=='ADDING':
    mode='WAIT'
	
if len(sys.argv) not in [2,3]:
  print("Usage: patchsprings.py <number of patches> <patch detail? 0 or 1>")
  exit(0)
  
if len(sys.argv)>1:
  patchLimit = int(sys.argv[1])

renderPatches = len(sys.argv)>2 and int(sys.argv[2])==1

#exit(0)
window = tk.Tk()
window.geometry('1350x700')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=1350, height=700, bg='white')
canvas.bind("<KeyPress>", keydown)
canvas.focus_set()
canvas.pack()

mode = "WAIT"

if True:
  sortedPatches = SortPatchCoords("d:/pipelineOutputTry20/patchCoords.txt")
  AddPatch(sortedPatches,patchToAdd)
  Show(patchToAdd)
  

window.mainloop()
    
