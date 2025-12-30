import pickle

with open("d:/s4_explore/bpscoretot_50.pkl",'rb') as f:
  badScore = pickle.load(f)

for t in badScore:
  if t[0]>0.0:
    print(str(t[1])+",")
  
