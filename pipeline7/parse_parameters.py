import json

with open("parameters.json","r",encoding="utf-8") as f:
  data = json.load(f)
  
print(data)

with open("parameters.py","w") as f:
  for (key,value) in data.items():
    for (keyc,valuec) in value.items():
      if keyc[:11]!="__comment__":
        if type(valuec) is str:
          f.write(keyc + '="' + str(valuec) + '"\n')
        else:
          f.write(keyc + "=" + str(valuec) + "\n")
      else:
        f.write("# " + str(valuec)+"\n")      

with open("parameters.h","w") as f:
  for (key,value) in data.items():
    for (keyc,valuec) in value.items():
      if keyc[:11]!="__comment__":
        if type(valuec) is str:
          f.write('#define ' + keyc + ' "' + str(valuec) + '"\n')   
        else:
          f.write("#define " + keyc + " " + str(valuec) + "\n") 
      else:
        f.write("/* " + str(valuec) + " */\n")    
        