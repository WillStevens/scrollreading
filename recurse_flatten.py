# Will Stevens 
# 17th March 2025
# Released under GNU Public License V3

# Program to test an idea about recursively flattening a pointset by splitting it into two parts, flattening each part, then
# aligning the two parts.

# SVD is used to fit a plane to a pointset, and also to find the plane to split along.
# Because SVD is expensive for a lrge pointset, pick random points from the pointset and do SVD on those.

import numpy as np
from math import sqrt
import csv

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
  print(points.shape)
  # Take a random subset of 1000 points
  rpoints = points[:,np.random.randint(points.shape[1],size=1000)]
  print(rpoints.shape)
  svd = np.linalg.svd(rpoints)

  # Extract the left singular vectors
  left = svd[0]

  print(left[:, -1])
  
  # Return the best fitting plane and the short axis, and the long axis
  return (left[:, -1],left[:,-2],left[:, -3])

# Metric for how the plane fits the points
def FitMetric(points,normal):
  dist = np.matmul(normal,points)
  return sqrt(sum([x*x for x in dist])/len(dist))
  
# Is it a good fit?
def GoodFit(fitMetric):
  return true
  
# Split the pointSet up into two parts (the divide is perpendicular to the long axis)
# Return the two pointSets
def Split(points,longaxis):
  condition = np.matmul(longaxis,points)<0
  return [points[:,condition],points[:,~condition]]
  
# Project points onto a plane  
def Project(points,normal):
  # How far is the point from the plane? Move it that much
  dist = np.matmul(normal,points)
  print(dist)
  offset = np.transpose([normal * d for d in dist])
  points -= offset
  return points
  
  
# Given a pointSet, return a fitting plane and a co-planar pointSet in 1:1 correspondance with the original pointSet  
def Flatten(points):
  plane = FitPlane(points)
  if GoodFit(FitMetric(points,plane)):
    return (Project(points,plane),plane)
  else:
    twoPlanes = [Flatten(q) for q in Split(points,centroid)]
	# Rotate one of the planes so that it is aligned with the other, then work out how to translate the points so that
    # the centroid and a vector directed from the centroid along the split line match up.	

	
points = [(0,0,0),(10,0,0),(10,5,0),(9,3,0),(8,4,0),(4,2,0),(1,1,0),(0,5,0),(6,3,0),(10,1,0)]
points = np.array([[x[0] for x in points],[x[1] for x in points],[x[2] for x in points]])
print(points.shape)
print("Loading points")
points = []
with open(r"../flatten_interp_test/bulk/v2011601_32_interp.csv", newline='') as csvfile:
  pointreader = csv.reader(csvfile)

  for row in pointreader:
    points += [[float(row[0]),float(row[1]),float(row[2])]]

points = np.array([[x[0] for x in points],[x[1] for x in points],[x[2] for x in points]])
	
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


