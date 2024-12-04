from math import sqrt

particles = []
connections = []

def InitParticles():
  global particles,connections
  particles = [[(0,0,0),(0,0,0)],[(1.1,0,0),(0,0,0)]]
  connections = [(0,1,1),(1,0,1)]
	
def ParticleForces(p1,p2):
  REPEL_FORCE_CONSTANT = 0.1
  (x1,y1,z1)=p1
  (x2,y2,z2)=p2
  
  diff=(x2-x1,y2-y1,z2-z1)
  dist2=diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2];
  dist=sqrt(dist2)

  if dist < 1.0:
    f = 1.0-dist
    f = f*f * REPEL_FORCE_CONSTANT / dist
	
    return (-f*diff[0],-f*diff[1],-f*diff[2])
  else:
    return (0.0,0.0,0.0)

def ConnectionForces(p1,p2,distTarget):
  ATTRACT_FORCE_CONSTANT = 0.02
  (x1,y1,z1)=p1
  (x2,y2,z2)=p2
  
  diff=(x2-x1,y2-y1,z2-z1)
  dist2=diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2];
  dist=sqrt(dist2)

  distFromTarget = dist - distTarget;

  f = distFromTarget*ATTRACT_FORCE_CONSTANT;

  return (diff[0]*f/dist,diff[1]*f/dist,diff[2]*f/dist)
  
def Run():
  global particles,connections
  FRICTION_FORCE_CONSTANT = 0.9
  
  for i in range(0,len(particles)):
    for j in range(0,len(particles)):
      if i != j:
        f = ParticleForces(particles[i][0],particles[j][0])
        particles[i][1] = (particles[i][1][0]+f[0],particles[i][1][1]+f[1],particles[i][1][2]+f[2])
#        print(i)
#        print(f)
		
  for (p1,p2,d) in connections:
    f = ConnectionForces(particles[p1][0],particles[p2][0],d)
    particles[p1][1] = (particles[p1][1][0]+f[0],particles[p1][1][1]+f[1],particles[p1][1][2]+f[2])
#    print(p1)
#    print(f)

  for i in range(0,len(particles)): 
    particles[i][1] = (particles[i][1][0]*FRICTION_FORCE_CONSTANT,particles[i][1][1]*FRICTION_FORCE_CONSTANT,particles[i][1][2]*FRICTION_FORCE_CONSTANT) 
    particles[i][0] = (particles[i][0][0]+particles[i][1][0],particles[i][0][1]+particles[i][1][1],particles[i][0][2]+particles[i][1][2])

InitParticles()

for i in range(0,50):
  Run()
  print(particles)