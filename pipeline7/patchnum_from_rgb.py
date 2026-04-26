def RGBFunc(ci):
  rd = 255-80*(ci%3)-(ci//105)%79
  gd = 255-40*((3*ci)%5)-(ci//105)%37
  bd = 255-26*((5*ci)%7)-(ci//105)%23
  return (rd,gd,bd)

def RGB(r,g,b):
 ret = 0
 count = 0
 print(r,g,b)
 for ci in range(0,100000):
  rd,gd,bd = RGBFunc(ci)
  if (rd<0 or rd>255 or gd<0 or gd>255 or bd<0 or bd>255):
    print("Error")
  if r==rd and g==gd and b==bd:
    ret = ci
    count+=1
 print(count)
 return ret
  
print(RGB(175,175,125),"yellow")
print(RGB(173,133,123),"brown")

print(RGBFunc(1755))
print(RGBFunc(804))
print(RGBFunc(1295))
print(RGBFunc(3604))

print(RGB(64,64,91),"grey")
print(RGB(75,75,105),"blue grey")
