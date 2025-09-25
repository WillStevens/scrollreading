import json

with open("parameters.json","r",encoding="utf-8") as f:
  data = json.load(f)
  
print(data)

with open("parameters.py","w") as f:
  for (key,value) in data.items():
    f.write(key + "=" + str(value) + "\n")
	
with open("parameters.h","w") as f:
  for (key,value) in data.items():
    f.write("#define " + key + " " + str(value) + "\n")	