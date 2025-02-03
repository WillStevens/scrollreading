import vtk
import vtk.util.numpy_support
import numpy as np
import csv
import sys

if len(sys.argv) not in [3,4]:
  print("Usage: makevtk.py <csv file> <output file> [x-mate]")
  exit(-1)
  
csvFilename = sys.argv[1]
outputFilename = sys.argv[2]

xMate = 1

if len(sys.argv)==4:
  xMate = int(sys.argv[3])

pointcloud = []

with open(csvFilename, newline='') as csvfile:

    pointreader = csv.reader(csvfile)

    for row in pointreader:
      pointcloud += [[float(row[0]),float(row[1]),float(row[2])]]

points = np.array(pointcloud[::xMate])

# Create the vtkPoints object.
vtk_points = vtk.vtkPoints()
vtk_points.SetData(vtk.util.numpy_support.numpy_to_vtk(points))

# Create the vtkPolyData object.
polydata = vtk.vtkPolyData()
polydata.SetPoints(vtk_points)

# Create the vtkSphereSource object.
sphere = vtk.vtkSphereSource()
sphere.SetRadius(1.0)
sphere.SetThetaResolution(4)
sphere.SetPhiResolution(4)



# Create the vtkGlyph3D object.
glyph = vtk.vtkGlyph3D()
glyph.SetInputData(polydata)
glyph.SetSourceConnection(sphere.GetOutputPort())
glyph.Update()

writer = vtk.vtkPolyDataWriter()

writer.SetInputData(glyph.GetOutput())
writer.SetFileName(outputFilename)
writer.Update()

