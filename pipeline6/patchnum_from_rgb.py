r=246
g=168
b=147

for ci in range(0,10000000):
  rd = 255-80*(ci%3)-(ci//105)%79
  gd = 255-40*((3*ci)%5)-(ci//105)%37
  bd = 255-26*((5*ci)%7)-(ci//105)%23
  if rd==246 and gd==168:
    print((rd,gd,bd))
  if r==rd and g==gd and b==bd:
    print(ci)
    break