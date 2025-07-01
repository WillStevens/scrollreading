Zarr is a  large mutlidimensional array storage library.

zarrgen.py is a code generation script that will generate C functions for reading and writing zarr arrays.

It only supports zarr's stored on a file-system, and only supports the types that I have used so far (at the time of writing this is u1 and <f4, and structured data types composed of those).

To use it, make a dictionary containing metadata for the zarr (which can be based on the .zarray file of an existing zarr), and pass this dictionary CodeGen along with the number of in-memory buffers needed, and a suffix to append to all data type and function names:

# A zarr with small chunk size
metadata = {
  "chunks":[32,32,32,4],
  "dtype":"<f4", 
  "dimension_separator": ".",
  "compressor": {
        "blocksize": 0,
        "clevel": 3,
        "cname": "zstd",
        "id": "blosc",
        "shuffle": 1
   },
   
  "fill_value": 0,
  "filters": None,
  "order": "C",
  "zarr_format": 2
}
  
# Use 5000 buffers
CodeGen(metadata,5000,"_6")

