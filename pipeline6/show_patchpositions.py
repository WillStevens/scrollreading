from math import pi,sqrt,sin,cos,atan2
from time import sleep
import tkinter as tk
import sys
import os
import numpy as np
from PIL import Image,ImageTk

import parameters

RADIUS_FACTOR = parameters.QUADMESH_SIZE//2

patchImages = {}
ImageRefs = [] # keep refs that canvas has so that they don't get GC

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

def LoadPatchImage(pipelineOutput,patchNum):
  global patchImages
  
  if os.path.exists(pipelineOutput+"/patch_%d.tif" % patchNum):
    image = Image.open(pipelineOutput+"/patch_%d.tif" % patchNum).convert("L")
    mask = Image.open(pipelineOutput+"/patch_%d.tifm" % patchNum).convert("L")

    # Turn image and mask into a single image with transparency
    rgba = Image.merge("RGBA",(image,image,image,mask))
  
    rgba.save(pipelineOutput+"/patch_%d.png" % patchNum)
    patchImages[patchNum] = rgba

def LoadPatches(pipelineOutput,fin):
  f = open(fin)
  
  patches = []
  
  for l in f.readlines():
    spl = l.split(" ")
    patchNum = int(spl[0])
    x = float(spl[1])
    y = float(spl[2])
    angle = float(spl[3])
    flip = int(spl[4])
    
    patches += [(patchNum,x,y,angle,flip)]
 
    LoadPatchImage(pipelineOutput,patchNum)
  f.close()

  return patches
    
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

def Show():
  global patchImages,ImageRefs
  canvas.delete('all')
  offset=(750,300)
  scale=0.5

  first = True
  for (patchNum,x,y,angle,flip) in patches:
    rgba = patchImages[patchNum]
    if flip:
      rgba = rgba.transpose(Image.FLIP_LEFT_RIGHT)
    scale_factor = scale*RADIUS_FACTOR*2
    rgba = rgba.resize((int(rgba.size[0]*scale_factor),int(rgba.size[1]*scale_factor)),Image.Resampling.LANCZOS)
    if not first:
      rgba = rgba.rotate(-90+angle*360.0/(2*3.141592))
    else:
      rgba = rgba.rotate(0)
      first = False
    rgba = ImageTk.PhotoImage(rgba)
    ImageRefs += [rgba]
    canvas.create_image(offset[0]+x*scale,offset[1]+y*scale, image = rgba, anchor = tk.CENTER)

def keydown(e):
  return
    
if len(sys.argv) not in [3]:
  print("Usage: show_patchpositions.py <pipelineOutput> <patchPositions>")
  exit(0)
  

#exit(0)
window = tk.Tk()
window.geometry('1350x700')
window.title('L paint')

# Create a canvas
canvas = tk.Canvas(window, width=1350, height=700, bg='white')
canvas.bind("<KeyPress>", keydown)
canvas.focus_set()
canvas.pack()

if True:
  patches = LoadPatches(sys.argv[1],sys.argv[2])
  Show()
  
window.mainloop()
    
