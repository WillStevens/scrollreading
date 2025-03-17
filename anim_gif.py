import imageio

images = []
for i in range(0,12000,100):
  images.append(imageio.imread("anim_test_%06d.png" % i))
kargs = {'duration':100}
imageio.mimsave("anim_test.gif",images,'GIF',**kargs)
