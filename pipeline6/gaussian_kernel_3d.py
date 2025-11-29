import numpy as np

def gaussian_kernel_3d(size, sigma_x, sigma_y, sigma_z):
    """
    Generates a 3D Gaussian kernel.

    Args:
        size:  Kernel size (e.g., 5 for 5x5x5). Must be an odd number.
        sigma_x, sigma_y, sigma_z: Standard deviations along each axis.

    Returns:
        A 3D NumPy array representing the Gaussian kernel.
    """
    ax = np.linspace(-(size // 2), size // 2, size)
    xx, yy, zz = np.meshgrid(ax, ax, ax)

    kernel = np.exp(-0.5 * (xx**2 / sigma_x**2 + yy**2 / sigma_y**2 + zz**2 / sigma_z**2))
    kernel = kernel / np.sum(kernel) # Normalization

    return kernel
	
size = 5
k = gaussian_kernel_3d(size,0.8,0.8,0.8)

sum = 0.0
print("float kernel[%d][%d][%d] = {" % (size,size,size))
for z in range(0,size):
  print("  {");
  for y in range(0,size):
    print("    {",end="")
    for x in range(0,size):
      print(("" if x==0 else ",") + str(k[x][y][z]),end="")
      sum += k[x][y][z]
    print("},")
  print("  },")
print("};")

