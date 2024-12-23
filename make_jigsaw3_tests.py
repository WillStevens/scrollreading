
dir="C:/Users/Will/Downloads/vesuvius/jigsaw3_test/"

def WritePatch(xo,yo,zo,suffix):
  f = open(dir+"v"+suffix,"w")

  for x in range(0,101):
    for z in range(0,101):
      f.write(str(x+xo)+","+str(yo)+","+str(z+zo)+"\n")
	
  f = open(dir+"x"+suffix,"w")
  f.write(str(xo)+","+str(yo)+","+str(zo)+","+str(xo+100)+","+str(yo)+","+str(zo+100))
  
WritePatch(0,100,0,"1_1.csv")
WritePatch(100,100,0,"1_2.csv")
WritePatch(200,100,0,"1_3.csv")

WritePatch(0,150,0,"1_4.csv")
WritePatch(100,150,0,"1_5.csv")
WritePatch(200,150,0,"1_6.csv")

suffix="1_7.csv"
f = open(dir+"v"+suffix,"w")

for y in range(100,151):
  for z in range(0,100):
    f.write("150,"+str(y)+","+str(z)+"\n")
	
f = open(dir+"x"+suffix,"w")
f.write("150,100,0,150,150,100")
