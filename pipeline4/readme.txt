Fourth version of simpaper pipeline

Major changes for this version include:

Binary representation for all patches
Patch files are deleted after they are added to the surface (no information is discarded because it is containined within the surface)
The boundary of the growing surface is tracked - so that new seed points can be chosen without having to regenerate the boundary every time
All patch positioning is deferred until the end of the pipeline, using affine transformation that locate patches relative to each other to refine positions
All operations are now more-or-less independent of the size of the surface so it should scale well

