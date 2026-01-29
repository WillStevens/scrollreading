r=150
g=122
b=249
n=44191
p=44190

r=37
g=76
b=190
n=64238
p=64237

for ci in range(0,10000000):
  rd = 255-80*(ci%3)-(ci//105)%79
  gd = 255-40*((3*ci)%5)-(ci//105)%37
  bd = 255-26*((5*ci)%7)-(ci//105)%23
  if r==rd and g==gd and b==bd:
    print(ci)
    break
	
	
	