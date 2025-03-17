# Make shell scripts and FTP scripts for flattening/interpolation

ToFlatten = ["s02512_02966_01500/nonint_sj/output_jigsaw3/v1500601_19.csv",
             "s02512_02966_02011/nonint_sj/output_jigsaw3/v2011601_79.csv",
             "s02512_02966_02522/nonint_sj/output_jigsaw3/v2522601_557.csv",
			 
			 "s02512_03477_01500/nonint_sj/output_jigsaw3/v1500601_23.csv",
             "s02512_03477_02011/nonint_sj/output_jigsaw3/v2011601_7.csv",
             "s02512_03477_02522/nonint_sj/output_jigsaw3/v2522601_1024.csv",
			 
			 "s02512_03988_01500/nonint_sj/output_jigsaw3/v-259235897_61.csv",
             "s02512_03988_02011/nonint_sj/output_jigsaw3/v2011601_32.csv",
             "s02512_03988_02522/nonint_sj/output_jigsaw3/v2522601_1815.csv",
            ]

orientation = ["1000 0 0 0 0 1000",
               "1000 0 0 0 0 1000",
               "981 190 0 0 0 1000",

               "1000 0 0 0 0 1000",
               "1000 0 0 0 0 1000",
               "1000 0 0 0 0 1000",

               "1000 0 0 0 0 1000",
               "1000 0 0 0 0 1000",
               "866 -500 0 -250 -433 866",
              ]
			  
def GetFileCube(s):
  return s[1:18]
  
def GetFileName(s):
  return s[s.rfind('/')+1:]
  
def GetFileRoot(s):
  return s[s.rfind('/')+1:s.rfind('.')]
  
for s in ToFlatten:
  print("put d:/scroll1_construct/" + s)

i = 0
for s in ToFlatten:
  print("./surfaceFlatten3 " + GetFileName(s) + " " + GetFileRoot(s) + "_flattened.csv " + orientation[i])
  i+=1
  
for s in ToFlatten:
  print("get " + GetFileRoot(s) + "_flattened.csv")
  
for s in ToFlatten:
  print("../../code/resample_flattened_and_holes d:/scroll1_construct/" + s + " " + GetFileRoot(s) + "_flattened.csv " + GetFileRoot(s) + "_target.csv " + GetFileRoot(s) + "_flat.csv " + GetFileRoot(s) + "_hole.csv")
  
for s in ToFlatten:
  print("put " + GetFileRoot(s) + "_target.csv")
  print("put " + GetFileRoot(s) + "_flat.csv")
  print("put " + GetFileRoot(s) + "_hole.csv")

i=0
for s in ToFlatten:
  print("./surfaceUnFlatten3 " + GetFileRoot(s) + "_target.csv " + GetFileRoot(s) + "_flat.csv " + GetFileRoot(s) + "_hole.csv " + orientation[i] + " > " + GetFileRoot(s) + "_interp.csv")
  i+=1
  
for s in ToFlatten:
  print("get " + GetFileRoot(s) + "_interp.csv")

for s in ToFlatten:
  print("cat " + GetFileRoot(s) + "_flat.csv " + GetFileRoot(s) + "_hole.csv > " + GetFileRoot(s) + "_flat_hole.csv" )
  
for s in ToFlatten:
  print("../../code/render_slices d:/scroll1_cubes/" + GetFileCube(s) + " " + GetFileRoot(s) + "_flat_hole.csv " + GetFileRoot(s) + "_interp.csv " + GetFileRoot(s) + ".tif")

