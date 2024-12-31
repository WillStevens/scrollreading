from math import sqrt

for x in range(0,100):
  for y in range(0,100):
    d = abs(50-x)//2
    xd = sqrt(float((50-x)*(50-x)-d*d))
    if x<50:
      xd = 50-xd
    else:
      xd = 50+xd
    if x not in range(30,70) or y not in range(30,70):
      print("%f,%f,%f" % (xd,float(100+d),float(y)))
#    if x not in range(30,70) or y not in range(30,70):
#      print("%f,%f,%f" % (float(x),float(0),float(y)))
#    if x in range(30,70) and y in range(30,70):
#      print("%f,%f,%f" % (float(x),float(0),float(y)))
