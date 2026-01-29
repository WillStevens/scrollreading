import zarr

#compressor = zarr.Blosc(cname="zstd", clevel=3, shuffle=zarr.Blosc.SHUFFLE)

za = zarr.open(r'D:\output_zarrs\s4_filtered',mode='r')

print(za.shape)

print(za.attrs.asdict())
print(za.info)

for z in range(1280,1281):
  for y in range(1280,1285):
    for x in range(1280,1285):
      print(za[z,y,x])
