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
  
print(RGB(174,214,228),"")
print(RGB(92,132,122),"")
print(RGB(174,254,228),"")
print(RGB(171,171,199),"")
print(RGB(91,131,225),"")
print(RGB(165,125,219),"")
print(RGB(173,173,149),"")
print(RGB(86,246,220),"")
print(RGB(248,208,92),"")
print(RGB(166,86,168),"")
print(RGB(92,132,122),"")
