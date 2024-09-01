# Will Stevens, August 2024
# 
# Simple user interface for viewing x,y-plane slices of cubic volumes of the Herculanium Papyri.
#
# Loading and processing of scroll volumes is done by scrollprocess.dll, compiled from C source code and
# loaded by this program.
# 
# Move up and down through the volume with the +/- buttons.
# Other buttons can be used to carry out various operations.
#
# This is a constantly evolving experimental program, used to tinker with different ideas, so not all
# buttons will be functional at any given release. Buttons that may be non-functional are commented out in the
# source code.
#
# At the time of writing only the +/- navigation buttons and the 'pipeline' button are being used regularly
# 
# The 'plugs' variable can be initialised with a list of x,y,z coordinates where will make those voxels
# unfillable. 'plugs' is produced by holefiller.c
#
# Released under GNU Public License V3

from ctypes import *
import pathlib
import numpy
import datetime
import tkinter as tk
from PIL import ImageGrab,ImageTk,Image

# To be able to load scrollprocess.dll and dependencies, the following need to be in PATH
# C:\cygwin64\usr\x86_64-w64-mingw32\sys-root\mingw\bin

folderSuffix = b'005'
zOffset = 5800
output_folder = "s" + folderSuffix.decode('utf-8') # Location where rendering output will be placed
plugFileName = "plugs_v005_4.csv"

plugs = []

print("Loading plugs")
with open(plugFileName) as plugFile:
  for line in plugFile:
    if line[0]=='[':
      line=line[1:-3]
      pt = [int(s) for s in line.split(',')]
      plugs += [[pt[0],pt[1],pt[2]]] 
print("Finished loading plugs")


# Meanings of different values in 'processed'
# Must match definitions in scrollprocess.c 
PR_EMPTY = 0
PR_SCROLL = 1
PR_PLUG = 2
PR_TMP = 3
PR_FILL_START = 4
PR_FILL_END = 2147483647

volume_size = 512
volume_z = 0


colour_palette = [0,0,0,255,255,255]

while len(colour_palette) != 3*65536:
  r=colour_palette[-3]
  g=colour_palette[-2]
  b=colour_palette[-1]
  r=(r+127)%256
  g=(g+83)%256
  b=(b+53)%256
  colour_palette += [r,g,b]
  
if __name__ == "__main__":
  libname = pathlib.Path(r"scrollprocess.dll")
  c_lib = CDLL(libname,winmode=0)

c_lib.getVoxels.argtypes = [c_int,c_int,c_int,POINTER(c_uint8)]
c_lib.getVoxels.restype = c_int

c_lib.getSlice.argtypes = [c_int]
c_lib.getSlice.restype = POINTER(c_uint8)

c_lib.getSliceP.argtypes = [c_int]
c_lib.getSliceP.restype = POINTER(c_uint8)

c_lib.getSliceR.argtypes = [c_int]
c_lib.getSliceR.restype = POINTER(c_uint8)

c_lib.getSliceSobel.argtypes = [c_int]
c_lib.getSliceSobel.restype = POINTER(c_uint8)

c_lib.setFolderSuffixAndZOffset.argtypes = [c_char_p,c_uint]

c_lib.setFolderSuffixAndZOffset(c_char_p(folderSuffix),zOffset)  
c_lib.loadTiffs()


#for z in range(0,volume_size):
#  im=Image.open("..\\construct\\e002\\e002_0%d.tif" % (5800+z))
#  imarray = numpy.uint8(numpy.array(im)/256)
    
#  for y in range(0,volume_size):
#    for x in range(0,volume_size,8):
#      #c_lib.setVoxels(x,y,z,int(imarray[y,x]),int(imarray[y,x+1]),int(imarray[y,x+2]),int(imarray[y,x+3]),int(imarray[y,x+4]),int(imarray[y,x+5]),int(imarray[y,x+6]),int(imarray[y,x+7]))

#for z in range(0,32):
#  for y in range(0,32):
#    for x in range(0,32):
#      print(c_lib.getVoxel(x,y,z),end='')
#    print()
#  print("---")



windll.shcore.SetProcessDpiAwareness(2) # windows 10
           
class ToolWin(tk.Toplevel):
    def __init__(self):
        global plugs
        tk.Toplevel.__init__(self)
        self._offsetx = 0
        self._offsety = 0
        self.wm_attributes('-topmost',1)
        self.penSelect = tk.BooleanVar()
        self.overrideredirect(1)
        self.geometry('200x700')
        self.penModeId = None
        self.bind('<ButtonPress-1>',self.clickTool) 
        self.bind('<B1-Motion>',self.moveTool) # bind move event
        root.bind("<Button-1>", self.mouseClick)
        
        draw = tk.Checkbutton(self,text="Pen",command=self.penDraw,variable=self.penSelect)
        draw.pack()
        cancel = tk.Button(self,text="Quit",command=root.destroy)
        cancel.pack()

        nextImg = tk.Button(self,text="+1",command=self.nextImg)
        nextImg.pack()
        prevImg = tk.Button(self,text="-1",command=self.prevImg)
        prevImg.pack()

        next20Img = tk.Button(self,text="+20",command=self.next20Img)
        next20Img.pack()
        prev20Img = tk.Button(self,text="-20",command=self.prev20Img)
        prev20Img.pack()

        next100Img = tk.Button(self,text="+100",command=self.next100Img)
        next100Img.pack()
        prev100Img = tk.Button(self,text="-100",command=self.prev100Img)
        prev100Img.pack()
		
# Comment out buttons that are not currently used
        """
        prop = tk.Button(self,text="Prop",command=self.prop)
        prop.pack()
        prop20 = tk.Button(self,text="Prop x 20",command=self.prop20)
        prop20.pack()

        fill = tk.Button(self,text="Fill",command=self.fill)
        fill.pack()

        plug = tk.Button(self,text="Plug",command=self.plug)
        plug.pack()

        render = tk.Button(self,text="Render",command=self.render)
        render.pack()

        processed = tk.Button(self,text="Processed",command=self.processed)
        processed.pack()

        dilate110 = tk.Button(self,text="Dilate 110",command=self.dilate110)
        dilate110.pack()

        sobel = tk.Button(self,text="Sobel",command=self.sobel)
        sobel.pack()

        filter = tk.Button(self,text="Filter",command=self.filter)
        filter.pack()

        adaptive = tk.Button(self,text="Adaptive",command=self.adaptive)
        adaptive.pack()

        findAndFill = tk.Button(self,text="FindAndFill",command=self.findAndFill)
        findAndFill.pack()

        dilateFilled = tk.Button(self,text="Dilate filled",command=self.dilateFilled)
        dilateFilled.pack()

        dilateFilled10 = tk.Button(self,text="Dilate filled x 10",command=self.dilateFilled10)
        dilateFilled10.pack()
        """
        pipeline = tk.Button(self,text="Pipeline",command=self.pipeline)
        pipeline.pack()
        
        self.l = tk.Label(self, text = "Click for coords")
        self.l.config(font =("Courier", 14))        
        self.l.pack()

        self.z_disp = tk.Label(self, text = "z="+str(volume_z))
        self.z_disp.config(font =("Courier", 14))        
        self.z_disp.pack()

        self.v = tk.Label(self, text = "Click for value")
        self.v.config(font =("Courier", 14))        
        self.v.pack()

        self.clickMode="FILL"
        self.plugSize=2
        self.plugSquares = []
        
        self.fill_value=PR_FILL_START
        self.render_value=PR_FILL_START
        
        self.fillStatus = tk.Label(self,text = "Ready")
        self.fillStatus.pack()
        
        self.foundStatus = tk.Label(self,text = "NA")
        self.foundStatus.pack()
        
    def fill(self):
      self.clickMode="FILL"

    def plug(self):
      self.clickMode="PLUG"
             
    def prop(self):
      t = "Finished" if c_lib.floodFillContinue(100000)==0 else "Ongoing";
      self.fillStatus.config(text=t)
      self.updateImg()

    def prop20(self):
      t = "Finished" if c_lib.floodFillContinue(2000000)==0 else "Ongoing";
      self.fillStatus.config(text=t)
      self.updateImg()
      
    def nextImg(self):
      global volume_z
      if volume_z < volume_size:
        volume_z += 1
        self.updateImg()
      
    def prevImg(self):
      global volume_z
      if volume_z > 0:
        volume_z -= 1
        self.updateImg()

    def next20Img(self):
      global volume_z
      if volume_z < volume_size-20:
        volume_z += 20
        self.updateImg()
      
    def prev20Img(self):
      global volume_z
      if volume_z >= 20:
        volume_z -= 20
        self.updateImg()

    def next100Img(self):
      global volume_z
      if volume_z < volume_size-100:
        volume_z += 100
        self.updateImg()
      
    def prev100Img(self):
      global volume_z
      if volume_z >= 100:
        volume_z -= 100
        self.updateImg()
        
    def updateImg(self):
      global image_on_canvas,fullCanvas,img,volume_size,volume_z,plugs

      coordString = "z="+str(volume_z)
      self.z_disp.config(text=coordString)

#      a = numpy.zeros((volume_size,volume_size),dtype=numpy.uint8)

#      v = c_uint8()
#      for y in range(0,volume_size):
#        for x in range(0,volume_size):
#          c_lib.getVoxels(x,y,volume_z,byref(v))
#          a[y,x]=numpy.uint8(v)

      p = c_lib.getSlice(volume_z)

      a = numpy.ctypeslib.as_array(p, shape=(volume_size,volume_size))
      
      img=ImageTk.PhotoImage(Image.fromarray(a))

      fullCanvas.itemconfig(image_on_canvas, image = img)

      # Look for plugs with z = volume_z and draw them in red

      for s in self.plugSquares:
        fullCanvas.delete(s)
      self.plugSquares=[]
        
      for (x,y,z) in plugs:
        if volume_z==z and volume_z==z:
          colour = 'red'
          self.plugSquares += [fullCanvas.create_rectangle(x-self.plugSize,y-self.plugSize,x+self.plugSize,y+self.plugSize,fill=colour)]

      self.processed()
      
    def mouseClick(self,event):
        coordString = "x="+str(event.x)+",y="+str(event.y)
        self.l.config(text=coordString)
        valueString = "v="+str(c_lib.getVoxel(event.x,event.y,volume_z))
        if self.clickMode=="FILL":
          c_lib.floodFillStart(event.x,event.y,volume_z,self.fill_value)
          self.fill_value+=1
          if self.fill_value>1000000:
            self.fill_value=PR_FILL_START       
        elif self.clickMode=="PLUG":
          for xo in range(event.x-self.plugSize,event.x+self.plugSize+1):
            for yo in range(event.y-self.plugSize,event.y+self.plugSize+1):
              for zo in range(volume_z-self.plugSize,volume_z+self.plugSize+1):
                print("Setting %d %d %d" % (xo,yo,zo))
                c_lib.setVoxel(xo,yo,zo,254);
        
        self.v.config(text=valueString)
        self.updateImg()
        
    def moveTool(self,event):
        self.geometry("200x700+{}+{}".format(self.winfo_pointerx()-self._offsetx,self.winfo_pointery()-self._offsety))

    def clickTool(self,event):
        self._offsetx = event.x
        self._offsety = event.y

    def penDraw(self):
        if self.penSelect.get():
            self.penModeId = root.bind("<B1-Motion>",Draw)
        else:
            root.unbind('<B1-Motion>',self.penModeId)

    def render(self):
        global render_on_canvas,fullCanvas,img_render,volume_size,output_folder
		
        for offset in [6]:
          c_lib.render(1,offset,self.render_value)
        
          p = c_lib.getSliceR(0)

          a = numpy.ctypeslib.as_array(p, shape=(volume_size,volume_size))

          i = Image.fromarray(a)
        
          img_render=ImageTk.PhotoImage(i)

          img_pil = ImageTk.getimage(img_render)
          img_pil.save("../construct/" + output_folder + "/"+str(self.render_value)+"_"+str(offset)+".png")
          img_pil.close()

        self.render_value += 1
        
        fullCanvas.itemconfig(render_on_canvas, image = img_render)

    def sobel(self):
        global render_on_canvas,fullCanvas,img_render,volume_size
        c_lib.Sobel()
        
        p = c_lib.getSliceSobel(volume_z)

        a = numpy.ctypeslib.as_array(p, shape=(volume_size,volume_size))

        i = Image.fromarray(a)
        
        img_render=ImageTk.PhotoImage(i)
        
        fullCanvas.itemconfig(render_on_canvas, image = img_render)
        
    def renderOld(self):
        global render_on_canvas,fullCanvas,img_render,volume_size
        
        a = numpy.zeros((volume_size,volume_size),dtype=numpy.uint8)
        for x in range(0,volume_size):
          print("x=%d" % x)
          for z in range(0,volume_size):
            for y in reversed(range(0,volume_size)):
              v = c_lib.getVoxelP(x,y,z)
              if v!=0 and v!=255:
                t = 0
                q = 1
                o = 6
                for yo in range(y+o,y+o+q):
                  if yo>=0 and yo<=512:
                    t += c_lib.getVoxel(x,yo,z)
                  else:
                    t += c_lib.getVoxel(x,0,z)
                t = t/q             
                a[z,x]=t
                break

        img_render=ImageTk.PhotoImage(Image.fromarray(a))

        fullCanvas.itemconfig(render_on_canvas, image = img_render)

    def processed(self):
        global render_on_canvas,fullCanvas,img_render,volume_size,colour_palette
        
#        a = numpy.zeros((volume_size,volume_size),dtype=numpy.uint8)

#        for y in range(0,volume_size):
#          for x in range(0,volume_size):
#            v=c_lib.getVoxelP(x,y,volume_z)
#            a[y,x]=v
        z="""
        p = c_lib.getSliceSobel(volume_z)

        a = numpy.ctypeslib.as_array(p, shape=(volume_size,volume_size))

        i = Image.fromarray(a)
        
        img_render=ImageTk.PhotoImage(i)
        
        fullCanvas.itemconfig(render_on_canvas, image = img_render)
        """
        
        p = c_lib.getSliceP(volume_z)

        a = numpy.ctypeslib.as_array(p, shape=(volume_size,volume_size,4))

        i = Image.fromarray(a,mode="RGBX")
#        i.putpalette(colour_palette)
        
        img_render=ImageTk.PhotoImage(i)

        fullCanvas.itemconfig(render_on_canvas, image = img_render)
        
    def dilate110(self):
        c_lib.dilate(110)
        self.updateImg()

    def dilate90(self):
        c_lib.dilate(90)
        self.updateImg()

    def filter(self):
        c_lib.dilate(110)
        c_lib.dilate(110)
        c_lib.dilate(110)
        c_lib.dilate(110)
        c_lib.dilate(110)
#        c_lib.dilate(0)
#        c_lib.dilate(0)
#        c_lib.dilate(0)
        self.updateImg()

    def adaptive(self):
        c_lib.gaussianThreshhold(-10)
        self.updateImg()

    def findAndFill(self):
        r = (c_lib.findEmptyAndStartFill(self.fill_value) == 1)
        t = "Found" if r else "Not found"
        self.foundStatus.config(text=t)

        self.fill_value+=1
        if self.fill_value>1000000:
          self.fill_value=PR_FILL_START     
        self.updateImg()
        
        return r

    def dilateFilled(self):
        c_lib.dilateFilledAreas()
        self.updateImg()

    def dilateFilled10(self):
        self.dilateFilledX(10)

    def dilateFilledX(self,x):
        for i in range(0,x):
            c_lib.dilateFilledAreas()
        self.updateImg()

    def pipeline(self):
        global plugs
        startTime = str(datetime.datetime.now())
		
        self.filter()
        self.sobel()
#        c_lib.dilate(0)
#        c_lib.dilate(0)
#        c_lib.sobel_and_processed()

        for (x,y,z) in plugs:
          c_lib.setVoxelP(x,y,z,PR_PLUG);

        startFillTime = str(datetime.datetime.now())
        
        i = c_lib.findAndFillAll(300000)
        print("Found " + str(i) + " volumes")


        startUnfillTime = str(datetime.datetime.now())

        print("Unfilling...")               
        for j in range(0,i):
          if c_lib.getRegionVolume(j) <= 5000:
            print("Unfilling " + str(j))
            c_lib.unFill(j+PR_FILL_START)

		
        startDilateTime = str(datetime.datetime.now())

        print("dilateFilled 5")        
        self.dilateFilledX(5)
#        print("dilateFilled 10")        
#        self.dilateFilledX(10)
#        print("dilateFilled 10")        
#        self.dilateFilledX(10)

        startRenderTime = str(datetime.datetime.now())

        print("Rendering...")               
        for j in range(0,i):
          print(str(j) + " volume " + str(c_lib.getRegionVolume(j)))
          if c_lib.getRegionVolume(j) > 5000:
            print("Rendering " + str(j))
            self.render()
          else:
            #c_lib.unFill(self.render_value)
            self.render_value += 1
        endTime = str(datetime.datetime.now())
        print("Started: " + startTime)
        print("Fill: " + startFillTime)
        print("Unfill: " + startUnfillTime)
        print("Dilate: " + startDilateTime)
        print("Render: " + startRenderTime)
        print("Ended: " + endTime)
          
          
        
def Draw(event):# r = 3
    fullCanvas.create_oval(event.x-3,event.y-3,event.x+3,event.y+3,fill="black")

def showTool(): # the small tool window
    toolWin = ToolWin()
    toolWin.mainloop()

root = tk.Tk()
root.state('zoomed')
root.overrideredirect(1)

fullCanvas = tk.Canvas(root)

a = numpy.zeros((volume_size,volume_size),dtype=numpy.uint8)

img=ImageTk.PhotoImage(Image.fromarray(a))
image_on_canvas = fullCanvas.create_image(0,0,image=img,anchor='nw') 

img_render=ImageTk.PhotoImage(Image.fromarray(a))
render_on_canvas = fullCanvas.create_image(600,0,image=img_render,anchor='nw') 

fullCanvas.pack(expand="YES",fill="both")

root.after(100,showTool)

root.mainloop()