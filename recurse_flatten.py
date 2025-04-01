# Will Stevens 
# 17th March 2025
# Released under GNU Public License V3

# Program to test an idea about recursively flattening a pointset by splitting it into two parts, flattening each part, then
# aligning the two parts.

# SVD is used to fit a plane to a pointset, and also to find the plane to split along.
# Because SVD is expensive for a large pointset, pick random points from the pointset and do SVD on those.

import numpy as np
from math import sqrt,pi
import csv
import sys
"""
def TriangleArea(A,B,C):
  xAB = A[0]-B[0]
  yAB = A[1]-B[1]
  zAB = A[2]-B[2]
  xAC = A[0]-C[0]
  yAC = A[1]-C[1]
  zAC = A[2]-C[2]
  
  return 0.5*sqrt((yAB*zAC-zAB*yAC)**2+(zAB*xAC-xAB*zAC)**2+(xAB*yAC-yAB*xAC)**2)

def InTriangle(p,a,b,c):
  A1 = TriangleArea(p,a,b)
  A2 = TriangleArea(p,a,c)
  A3 = TriangleArea(p,b,c)
  B = TriangleArea(a,b,c)
  
  print("Equality test")
  print((A1,A2,A3))
  print(B)
  
  return (A1+A2+A3)==B
"""
# https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
def Barycentric(p,a,b,c):
  v0 = b - a
  v1 = c - a
  v2 = p - a
  
  d00 = v0[0,0]*v0[0,0]+v0[1,0]*v0[1,0]+v0[2,0]*v0[2,0]
  d01 = v0[0,0]*v1[0,0]+v0[1,0]*v1[1,0]+v0[2,0]*v1[2,0]
  d11 = v1[0,0]*v1[0,0]+v1[1,0]*v1[1,0]+v1[2,0]*v1[2,0]
  d20 = v2[0,0]*v0[0,0]+v2[1,0]*v0[1,0]+v2[2,0]*v0[2,0]
  d21 = v2[0,0]*v1[0,0]+v2[1,0]*v1[1,0]+v2[2,0]*v1[2,0]

  denom = d00 * d11 - d01 * d01
  v = (d11 * d20 - d01 * d21) / denom
  w = (d00 * d21 - d01 * d20) / denom
  u = 1.0 - v - w
    
  return (u,v,w)


# If p is in one of the triangles, return the indices of the triangle and the Barycentric coordinates  
def FindTriangle(p,vertices,triangulation,last):
  count = 0
  for (a,b,c) in triangulation if last is None else [last]+triangulation:
    (u,v,w) = Barycentric(p,vertices[a],vertices[b],vertices[c])
    if u>=0.0 and v>=0.0 and w>=0.0 or (a,b,c)==last:
      """
      if last:
        print("Last:")
      else:
        print("Found in %d" % count)
      print(p)
      print((vertices[a],vertices[b],vertices[c]))
      print((u,v,w))
      """
    if u>=0.0 and v>=0.0 and w>=0.0:
      return ((a,b,c),(u,v,w))
    count += 1     
  #print("Not found in %d" % count)
  return None    

# https://stackoverflow.com/questions/2827393/angles-between-two-n-dimensional-vectors-in-python/13849249#13849249
def unit_vector(vector):
    """ Returns the unit vector of the vector.  """
    return vector / np.linalg.norm(vector)

def angle_between(v1, v2):
    """ Returns the angle in radians between vectors 'v1' and 'v2'::

            >>> angle_between((1, 0, 0), (0, 1, 0))
            1.5707963267948966
            >>> angle_between((1, 0, 0), (1, 0, 0))
            0.0
            >>> angle_between((1, 0, 0), (-1, 0, 0))
            3.141592653589793
    """
    v1_u = np.reshape(unit_vector(v1),3)
    v2_u = np.reshape(unit_vector(v2),3)
    return np.arccos(np.clip(np.dot(v1_u, v2_u), -1.0, 1.0))

def Angle(a,b,c): 
  return angle_between(a-b,c-b)
  
def normalize(v):
  norm = np.linalg.norm(v)
  if norm == 0:
    return v
  return v/norm
  
# Assuming that vertices[-1] is a point that all trangles have in common, triangulate by sweeping along other vertices in angle order
def Triangulate(vertices,flatPoints):  
  farthestPoint = vertices[-1]
  
  angles = np.zeros(len(vertices)-1)
  for i in range(0,len(vertices)-1):
    angles[i] = angle_between(vertices[i]-farthestPoint,vertices[0]-farthestPoint)

  # sort indices in order of angles
  sortedIndices = np.argsort(angles)
  
  triangulation = []
  for i in range(0,len(vertices)-2):
    triangulation += [(len(vertices)-1,sortedIndices[i],sortedIndices[i+1])]

  # Now add a final triangle (if necessary) by finding the farthest point from the line that joins farthestPoint and the last sorted index, lying on the opposite side of that line to the other points handled so far
  """
  a = farthestPoint
  n = vertices[sortedIndices[-1]]
  op = vertices[0]
  maxd = 0.0
  i = 0
  maxIndex = None
  for j in range(0,len(flatPoints[0])):
    p = np.reshape(flatPoints[:,j],(3,1))
    # p is on the correct side of the line from a to n, if the angles a:p:op + n:p:op sum to less than 180 degrees 
    if Angle(a,p,op)+Angle(n,p,op) < pi:
      norm = np.linalg.norm( (a-p)-np.dot(np.reshape(a-p,3),np.reshape(n,3))*n )
      if norm>maxd:
        maxd = norm
        maxIndex = i
    i += 1
  """
  a = farthestPoint
  n = normalize(vertices[sortedIndices[-1]]-a)
  op = vertices[0]
  
  # vector perpendicular to the line a:n
  perp = (a-op)-np.dot(np.reshape(a-op,3),np.reshape(n,3))*n 

  maxd = 0.0
  maxIndex = None
  for j in range(0,len(flatPoints[0])):
    p = np.reshape(flatPoints[:,j],(3,1))
    # p is on the correct side of the line from a to n, if the dot product of p-a is negative 
    if np.dot(np.reshape(perp,3),np.reshape(p-a,3))<0:
      norm = np.linalg.norm( (a-p)-np.dot(np.reshape(a-p,3),np.reshape(n,3))*n )
      if norm>maxd:
        maxd = norm
        maxIndex = j
        print("Farthest point from line is")
        print(maxd)
        print(p)

  
  return (triangulation,sortedIndices[-1],maxIndex)
  
# Distort points in flat1
def CageTransform(originalClosePoints,farthestPoint,deformedClosePoints,flat1):
  vertices = originalClosePoints + [farthestPoint]
  deformedVertices = deformedClosePoints + [farthestPoint]
  
  (triangulation,finalTri1,finalTri2) = Triangulate(vertices,flat1)

  if finalTri2 is not None:
    z = np.reshape(flat1[:,finalTri2],(3,1))
    vertices += [z]
    triangulation += [(len(vertices)-2,finalTri1,len(vertices)-1)]  
    deformedVertices += [z]
 
  
  print("Triangulation")
  print(triangulation)
  for i in range(0,5):
    (a,b,c) = triangulation[i]
    print((a,b,c))
    print(vertices[a])
    print(vertices[b])
    print(vertices[c])

  for i in range(0,5):
    (a,b,c) = triangulation[-(i+1)]
    print((a,b,c))
    print(vertices[a])
    print(vertices[b])
    print(vertices[c])

  deformedFlat1 = np.empty_like(flat1)
  
  lastTriangle = None
  
  for i in range(0,len(flat1[0])):
    (x,y,z) = (flat1[0,i],flat1[1,i],flat1[2,i])

    t = FindTriangle(np.array([[x],[y],[z]]),vertices,triangulation,lastTriangle)
    
    if t is not None:
      lastTriangle = t[0]
      ((ai,bi,ci),(u,v,w))=t
      
      # Now make the deformed point
      (a,b,c) = (deformedVertices[ai],deformedVertices[bi],deformedVertices[ci])
    
      deformedFlat1[0,i] = a[0]*u+b[0]*v+c[0]*w
      deformedFlat1[1,i] = a[1]*u+b[1]*v+c[1]*w
      deformedFlat1[2,i] = a[2]*u+b[2]*v+c[2]*w
#      print("Y%f,%f,%f" % (x,y,z))
#      print("D%f,%f,%f" % (deformedFlat1[0,i],deformedFlat1[1,i],deformedFlat1[2,i]))
    else:
      lastTriangle = None
 #     print("N%f,%f,%f" % (x,y,z))

  return deformedFlat1    

# From here: https://stackoverflow.com/questions/45142959/calculate-rotation-matrix-to-align-two-vectors-in-3d-space
def rotation_matrix_from_vectors(vec1, vec2):
    """ Find the rotation matrix that aligns vec1 to vec2
    :param vec1: A 3d "source" vector
    :param vec2: A 3d "destination" vector
    :return mat: A transform matrix (3x3) which when applied to vec1, aligns it with vec2.
    """
    a, b = (vec1 / np.linalg.norm(vec1)).reshape(3), (vec2 / np.linalg.norm(vec2)).reshape(3)
    v = np.cross(a, b)
    if any(v): #if not all zeros then 
        c = np.dot(a, b)
        s = np.linalg.norm(v)
        kmat = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
        return np.eye(3) + kmat + kmat.dot(kmat) * ((1 - c) / (s ** 2))

    else:
        return np.eye(3) #cross of all zeros only occurs on identical directions



# Find the best-fitting plane for a set of points
# (From here: https://math.stackexchange.com/questions/99299/best-fitting-plane-given-a-set-of-points )
def FitPlane(points):
  # Take a random subset of 1000 points
  rpoints = points[:,np.random.randint(points.shape[1],size=1000)]
  svd = np.linalg.svd(rpoints)

  # Extract the left singular vectors
  left = svd[0]
  
  # Return the best fitting plane and the short axis, and the long axis
  return (left[:, -1],left[:,-2],left[:, -3])

# Metric for how the plane fits the points
def FitMetric(points,normal):
  dist = np.matmul(normal,points)
  return sqrt(sum([x*x for x in dist])/len(dist))
  
# Is it a good fit?
def GoodFit(fitMetric):
  return False
  
# Split the pointSet up into two parts (the divide is perpendicular to the long axis)
# Return the two pointSets
def Split(points,longaxis):
  condition = np.matmul(longaxis,points)<0
  return (points[:,condition],points[:,~condition])
  
# Project points onto a plane  
def Project(points,normal):
  # How far is the point from the plane? Move it that much
  dist = np.matmul(normal,points)
  offset = np.transpose([normal * d for d in dist])
  points -= offset
  return points
  
  
# Given a pointSet, return a flat pointSet in 1:1 correspondance with the original pointSet
# Also return the extraPoints flattened in the same way - used for alignment
# Also return the plane that both are flattened to
  
def Flatten(points,extraPoints,depth=0):
  print(" " * depth + "Call to Flatten")
  # subtract out the centroid
  centroid = np.mean(points, axis=1, keepdims=True)
  print(" " * depth + "centroid")
  print(" " * depth + str(centroid))
  cpoints = points - centroid
  cextraPoints = [x - centroid for x in extraPoints]

  print(" " * depth + "cextrapoints")
  print(" " * depth + str(cextraPoints))
  
  (normal,shortaxis,longaxis) = FitPlane(cpoints)
  shortaxis = np.reshape(shortaxis,(3,1))

  print(" " * depth + "longaxis")
  print(" " * depth + str(longaxis))
  print(" " * depth + "shortaxis")
  print(" " * depth + str(shortaxis))

  
  if GoodFit(FitMetric(cpoints,normal)) or depth==2:
    print(" " * depth + "returned cextrapoints at max depth")
    print(" " * depth + str([Project(x,normal) for x in cextraPoints]))
    return (Project(cpoints,normal),[Project(x,normal) for x in cextraPoints],Project(-centroid,normal),normal)
  else:
    (points0,points1) = Split(cpoints,longaxis)
     
    cextraPoints += [-centroid]
    
    # Split extrapoints into two separate pointsets depending on which half they lie in
    epCondition = [np.matmul(longaxis,x)<0 for x in cextraPoints]
    
    (extraPoints0,extraPoints1) = ([x for x in cextraPoints if np.matmul(longaxis,x)<0],
                                   [x for x in cextraPoints if np.matmul(longaxis,x)>=0])
 
    print("Extrapoints")
    print(len(cextraPoints))
    print(len(extraPoints0))
    print(len(extraPoints1))
    
    # shortaxis will be used for alignment of the two planes, so add them to both
    # sets of extrapoints
    extraPoints0 += [shortaxis]
    extraPoints1 += [shortaxis]
 
    # Find the farthest point from the dividing plane in points1
    distance = np.matmul(longaxis,points1)
    maxDistance = distance[0]
    maxDistanceIndex = 0
    for i in range(0,len(distance)):
      if distance[i]>maxDistance:
        maxDistance = distance[i]
        maxDistanceIndex = i

    # Add the farthest point to extraPoints1
    extraPoints1 += [np.reshape(points1[:,maxDistanceIndex],(3,1))]
        
    # In 1, find all of the points near the dividing plane
    closePoints1 = points1[:,np.matmul(longaxis,points1)<1.0]
    numClosePoints1 = len(closePoints1[0])
    print("Number of close points")
    print(closePoints1.shape)
    # Add them to extraPoints0 and extraPoints1
    for i in range(0,numClosePoints1):
      extraPoints0 += [np.reshape(closePoints1[:,i],(3,1))]
      extraPoints1 += [np.reshape(closePoints1[:,i],(3,1))]
  
    (flat0,flatExtraPoints0,centroid0,normal0) = Flatten(points0,extraPoints0,depth+1)
    (flat1,flatExtraPoints1,centroid1,normal1) = Flatten(points1,extraPoints1,depth+1)

    flatClosePoints1In0 = flatExtraPoints0[-numClosePoints1:]
    flatExtraPoints0 = flatExtraPoints0[:-numClosePoints1]

    flatClosePoints1In1 = flatExtraPoints1[-numClosePoints1:]
    farthestPoint1 = flatExtraPoints1[-numClosePoints1-1]
    flatExtraPoints1 = flatExtraPoints1[:-numClosePoints1-1]
    
    """
    print("Close points")
    for p in flatClosePoints1In0:
      print("%f,%f,%f" %(p[0,0],p[1,0],p[2,0]))
    
    print("flatExtrapoints")
    print(len(flatExtraPoints0))
    print(len(flatExtraPoints1))
    """
    
    # If the dot product is negative then flip normal1
    if normal0.dot(normal1)<0:
      normal1 = -normal1

    # Transformation to rotate normal1 to normal0
    rotation = rotation_matrix_from_vectors(normal1,normal0)

    # Rotate all points1, and also normal1
    flat1 = np.matmul(rotation,flat1)
    normal1 = np.matmul(rotation,normal1)
    centroid1 = np.matmul(rotation,centroid1)
    print("flatExtraPoints1")
    print(len(flatExtraPoints1))
    print(flatExtraPoints1)
    flatExtraPoints1 = [np.matmul(rotation,x) for x in flatExtraPoints1]
    flatClosePoints1In1 = [np.matmul(rotation,x) for x in flatClosePoints1In1]
    farthestPoint1 = np.matmul(rotation,farthestPoint1)
    
    flat1 -= (centroid1 - centroid0)
    flatExtraPoints1 = [x - (centroid1 - centroid0) for x in flatExtraPoints1]
    flatClosePoints1In1 = [x - (centroid1 - centroid0) for x in flatClosePoints1In1]
    farthestPoint1 -= (centroid1 - centroid0)
    
    # Transformation to align the short axes from both halves
    rotation = rotation_matrix_from_vectors(flatExtraPoints1[-1],flatExtraPoints0[-1])

    # Rotate all points1, and also normal1
    flat1 = np.matmul(rotation,flat1)
    normal1 = np.matmul(rotation,normal1) # should not change
    flatExtraPoints1 = [np.matmul(rotation,x) for x in flatExtraPoints1]
    flatClosePoints1In1 = [np.matmul(rotation,x) for x in flatClosePoints1In1]
    farthestPoint1 = np.matmul(rotation,farthestPoint1)

    flat1 = CageTransform(flatClosePoints1In1,farthestPoint1,flatClosePoints1In0,flat1)
    
    flatExtraPoints0 = flatExtraPoints0[:-1]
    flatExtraPoints1 = flatExtraPoints1[:-1]
 
    # Using epCondition, join flatExtraPoints0 and flatExtraPoints1 together
    re = []
    i0 = 0
    i1 = 0
    for c in epCondition:
      if c:
        re += [flatExtraPoints0[i0]]
        i0 += 1
      else:
        re += [flatExtraPoints1[i1]]
        i1 += 1
        
    r = np.append(flat0,flat1,1)

    print(" " * depth + "returned cextrapoints")
    print(" " * depth + str(re))    
    return (r,re[:-1],re[-1],normal1)


"""
# A simple test
v = [(0,0,0),(1,0,0),(2,0,0),(3,0,0),(0,10,10)]
v = [np.reshape(np.array(x),(3,1)) for x in v]

d = [(0,0,0),(1.5,0,0),(3,0,0),(4.5,1.0,0)]
d = [np.reshape(np.array(x),(3,1)) for x in d]

p = [[0.1,0.1,0.1],[1.5,0.5,0.5],[2.7,1.0,1.0]]
p = np.array([[x[0] for x in p],[x[1] for x in p],[x[2] for x in p]])

print(v)    
print(Triangulate(v))

print("v before CageTransform")
print(v)    

print(CageTransform(v[:-1],v[-1],d,p))

sys.exit(0)
"""
    
print("Loading points")
points = []
with open(r"../flatten_interp_test/bulk/v2011601_32_interp.csv", newline='') as csvfile:
#with open(r"flatplane.csv", newline='') as csvfile:
  pointreader = csv.reader(csvfile)

  for row in pointreader:
    points += [[float(row[0]),float(row[1]),float(row[2])]]

points = np.array([[x[0] for x in points],[x[1] for x in points],[x[2] for x in points]])
    
(flat,a,c,normal)=Flatten(points,[])


print("Saving points")
for i in range(0,len(flat[0])):
  print("%f,%f,%f" %(flat[0,i],flat[1,i],flat[2,i]))
  
""" 
print("Subtracting centroid")
# subtract out the centroid
points = points - np.mean(points, axis=1, keepdims=True)

print("Fitting")
(normal,shortaxis,longaxis) = FitPlane(points)
shortaxis = np.reshape(shortaxis,(3,1))

(points0,points1) = Split(points,longaxis)

centroid0 = np.mean(points0, axis=1, keepdims=True)
centroid1 = np.mean(points1, axis=1, keepdims=True)
points0 = points0 - centroid0
centroidIn0 = -centroid0
shortaxisIn0 = shortaxis - centroid0
points1 = points1 - centroid1
centroidIn1 = -centroid1
shortaxisIn1 = shortaxis - centroid1

print(type(shortaxis))
print(shortaxis.shape)
print(type(centroid0))
print(centroid0.shape)

print("The shortaxis...")
print(shortaxis)
print(shortaxisIn0)
print(shortaxisIn1)

(normal0,shortaxis0,longaxis0) = FitPlane(points0)
(normal1,shortaxis1,longaxis1) = FitPlane(points1)

# If the dot product is negative then flip normal1
if normal0.dot(normal1)<0:
  normal1 = -normal1

print(normal0.dot(normal1))
  
# How do we rotate normal1 to normal0?
rotation = rotation_matrix_from_vectors(normal1,normal0)

# Rotate all points1, and also normal1
points1 = np.matmul(rotation,points1)
normal1 = np.matmul(rotation,normal1)
centroid1 = np.matmul(rotation,centroid1)
centroidIn1 = np.matmul(rotation,centroidIn1)
shortaxisIn1 = np.matmul(rotation,shortaxisIn1)

projected0 = Project(points0,normal0)+centroid0
projected1 = Project(points1,normal1)+centroid1

# Also project things that will help us align
# Project the original centroid onto both plane0 and plane1
# Project the original short axis vector onto both plane0 and plane1
projectedCentroidIn0 = Project(centroidIn0,normal0)+centroid0
projectedShortaxisIn0 = Project(shortaxisIn0,normal0)+centroid0
projectedCentroidIn1 = Project(centroidIn1,normal1)+centroid1
projectedShortaxisIn1 = Project(shortaxisIn1,normal1)+centroid1

print("Position of original centroids")
print(projectedCentroidIn0)
print(projectedCentroidIn1)

projected1 -= (projectedCentroidIn1 - projectedCentroidIn0)
projectedShortaxisIn1 -= (projectedCentroidIn1 - projectedCentroidIn0)


print("Short axis positions")
print(projectedShortaxisIn0)
print(projectedShortaxisIn1)

# How do we rotate one to ther other?
rotation = rotation_matrix_from_vectors(projectedShortaxisIn1,projectedShortaxisIn0)

# Rotate all points1, and also normal1
projected1 = np.matmul(rotation,projected1)
print("normal1 should be unchanged")
print(normal1)
# Normal1 should be unchanged
normal1 = np.matmul(rotation,normal1)
print(normal1)



print("Saving points")
for i in range(0,len(projected0[0])):
  print("%f,%f,%f" %(projected0[0,i],projected0[1,i],projected0[2,i]))
for i in range(0,len(projected1[0])):
  print("%f,%f,%f" %(projected1[0,i],projected1[1,i],projected1[2,i]))


"""
"""
(points00,points01) = Split(points0,longaxis0)
(points10,points11) = Split(points1,longaxis1)

centroid00 = np.mean(points00, axis=1, keepdims=True)
centroid01 = np.mean(points01, axis=1, keepdims=True)
centroid10 = np.mean(points10, axis=1, keepdims=True)
centroid11 = np.mean(points11, axis=1, keepdims=True)

points00 = points00 - centroid00
points01 = points01 - centroid01
points10 = points10 - centroid10
points11 = points11 - centroid11

(normal00,longaxis00) = FitPlane(points00)
(normal01,longaxis00) = FitPlane(points01)
(normal10,longaxis10) = FitPlane(points10)
(normal11,longaxis11) = FitPlane(points11)

projected00 = Project(points00,normal00)+centroid00+centroid0
projected01 = Project(points01,normal01)+centroid01+centroid0
projected10 = Project(points10,normal10)+centroid10+centroid1
projected11 = Project(points11,normal11)+centroid11+centroid1

# How do we rotate normal1 to normal0?
# Rotate all points in projected1 by that amount

print("Saving points")
for i in range(0,len(projected00[0])):
  print("%f,%f,%f" %(projected00[0,i],projected00[1,i],projected00[2,i]))
for i in range(0,len(projected01[0])):
  print("%f,%f,%f" %(projected01[0,i],projected01[1,i],projected01[2,i]))
for i in range(0,len(projected10[0])):
  print("%f,%f,%f" %(projected10[0,i],projected10[1,i],projected10[2,i]))
for i in range(0,len(projected11[0])):
  print("%f,%f,%f" %(projected11[0,i],projected11[1,i],projected11[2,i]))

"""
#print("Fitness")
#print(FitMetric(points,normal))
#print("Projecting")
#projected = Project(points,normal)

#print("Saving points")
#for i in range(0,len(projected[0])):
#  print("%f,%f,%f" %(projected[0,i],projected[1,i],projected[2,i]))
    
#print(points)
#print(longaxis)
#print(np.matmul(longaxis,points))       
#print(np.matmul(longaxis,points)<0)         
#print(Split(points,longaxis))


