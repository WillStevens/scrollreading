# Given a pointset and a triangulation
# Deform the pointset when the triangulation is deformed

# A list of points
pointSet = [(0.2,0.2),(0.5,0.0),(0.0,0.5)]

# A list of points that will be used for the triangulation
vertices = [(0.0,0.0),(1.0,0.0),(0.0,1.0)]

# A list of triples
triangulation = [(0,1,2)]

deformedVertices = [(0.0,0.0),(1.5,0.0),(0.0,1.0)]

def TriangleArea(a,b,c):
  return abs( a[0]*b[1]-b[0]*a[1]+b[0]*c[1]-c[0]*b[1]+c[0]*a[1]-a[0]*c[1] )/2.0
  
def InTriangle(p,a,b,c):
  A1 = TriangleArea(p,a,b)
  A2 = TriangleArea(p,a,c)
  A3 = TriangleArea(p,b,c)
  B = TriangleArea(a,b,c)
  
  print("Equality test")
  print((A1,A2,A3))
  print(B)
  
  return (A1+A2+A3)==B
  
def FindTriangle(p,vertices,triangulation):
 
  for (a,b,c) in triangulation:
    if InTriangle(p,vertices[a],vertices[b],vertices[c]):
      return (a,b,c)
     
  return None    

deformedPointSet = []
for (x,y) in pointSet:
  t = FindTriangle((x,y),vertices,triangulation)
    
  if t is not None:
    # We should be able to express (x,y) in the form:
    #  (x,y) = a + k*(b-a)+l*(c-a)
    
    #  (x,y)-a = k*(b-a)+l*(c-a)
    #   x - ax = k*(bx-ax)+l*(cx-ax)
    #   y - ay = k*(by-ay)+l*(cy-ay)

    #   (x - ax)*(cy-ay) = k*(bx-ax)*(cy-ay)+l*(cx-ax)*(cy-ay)
    #   (y - ay)*(cx-ax) = k*(by-ay)*(cx-ax)+l*(cy-ay)*(cx-ax)
    
    #    k*( (bx-ax)*(cy-ay) - (by-ay)*(cx-ax) ) = (x-ax)*(cy-ay)-(y-ay)*(cx-ax)
    #    l easily found now

    (ai,bi,ci)=t
    (a,b,c) = (vertices[ai],vertices[bi],vertices[ci])
    k = ( (x-a[0])*(c[1]-a[1]) - (y-a[1])*(c[0]-a[0]) ) / ( (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0]) )
    if (c[0]-a[0]) > (c[1]-a[1]):
      l = ( (x-a[0])-k*(b[0]-a[0]) ) / (c[0]-a[0])    
    else:
      l = ( (y-a[1])-k*(b[1]-a[1]) ) / (c[1]-a[1])    
    
    # Now make the deformed point
    (a,b,c) = (deformedVertices[ai],deformedVertices[bi],deformedVertices[ci])
    xn = a[0]+k*(b[0]-a[0])+l*(c[0]-a[0])
    yn = a[1]+k*(b[1]-a[1])+l*(c[1]-a[1])
    
    deformedPointSet += [(xn,yn)]
  else:
    print((x,y))
    print("Not in triangle")
	
print(pointSet)
print(deformedPointSet)